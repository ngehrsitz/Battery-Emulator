from pathlib import Path
import subprocess

Import("env")


def apply_size_patch(project):
    # Keep the experiment reproducible after the component manager downloads or
    # restores these ignored sources. Refuse mismatched versions rather than
    # silently compiling an unpatched component or overwriting local changes.
    patch = project / "tools" / "patches" / "led_indicator-2.1.2-size.patch"
    command = ["git", "apply", "--check", str(patch)]
    if subprocess.run(command + ["--reverse"], cwd=project, capture_output=True).returncode == 0:
        return
    check = subprocess.run(command, cwd=project, capture_output=True, text=True)
    if check.returncode:
        raise RuntimeError("Cannot apply LED component size patch:\n" + check.stderr)
    subprocess.run(["git", "apply", str(patch)], cwd=project, check=True)


# Hybrid mode first builds the SDK ("Build"), then runs the Arduino application
# build with the copied archives. Linking these names in the SDK stage would
# require archives that have not been copied into the Arduino package yet.
if env.subst("$ARDUINO_LIB_COMPILE_FLAG") != "Build":
    if env.GetProjectOption("custom_led_lto", "no") != "yes":
        env.AppendUnique(LIBS=["espressif__led_indicator", "espressif__led_strip"])
    else:
        # Rebuild only these components in the application pass. The SDK's other
        # archives stay untouched, but LTO can now see the controller's constant
        # configuration inside the LED factories.
        project = Path(env.subst("$PROJECT_DIR"))
        apply_size_patch(project)
        managed = project / "managed_components"
        led_env = env.LedLtoEnv()
        led_env.ProcessUnFlags("-Werror")  # Upstream C sources use IDF warning flags.
        led_env.AppendUnique(
            CPPPATH=[
                str(managed / "espressif__led_indicator" / "include"),
                str(managed / "espressif__led_indicator" / "private_include"),
                str(managed / "espressif__led_strip" / "include"),
                str(managed / "espressif__led_strip" / "interface"),
            ],
            CPPDEFINES=[
                ("LED_INDICATOR_VER_MAJOR", 2),
                ("LED_INDICATOR_VER_MINOR", 1),
                ("LED_INDICATOR_VER_PATCH", 2),
                ("LED_INDICATOR_STRIP_SPI_ENABLED", 0),  # All addressable boards use RMT.
                ("LED_INDICATOR_ADVANCED_BLINK_ENABLED", 0),  # Controller supplies sampled absolute steps.
            ],
        )
        archives = [
            led_env.BuildLibrary(
                "$BUILD_DIR/led_lto/" + component, str(managed / component / "src")
            )[0]
            for component in ("espressif__led_indicator", "espressif__led_strip")
        ]
        env.Replace(LINKFLAGS=[flag for flag in env["LINKFLAGS"] if flag != "-fno-lto"])
        env.AppendUnique(LIBS=archives, LINKFLAGS=["-flto"])
        env.Depends(env["PIOMAINPROG"], archives)
