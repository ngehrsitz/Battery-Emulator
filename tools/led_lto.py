"""Keep LTO limited to the LED controller and its managed components."""

Import("env")


def led_lto_env(build_env):
    result = build_env.Clone()
    result.ProcessUnFlags("-fno-lto")
    result.Append(CCFLAGS=["-flto"])
    # Slim LTO archives need GCC's plugin-aware binutils wrappers.
    result.Replace(AR="${CC}-ar", RANLIB="${CC}-ranlib", NM="${CC}-nm")
    return result


env.AddMethod(led_lto_env, "LedLtoEnv")


def build_led_controller(build_env, node):
    return build_env.LedLtoEnv().Object(node)


if env.GetProjectOption("custom_led_lto", "no") == "yes":
    env.AddBuildMiddleware(build_led_controller, "*/devboard/utils/led_handler.cpp")
