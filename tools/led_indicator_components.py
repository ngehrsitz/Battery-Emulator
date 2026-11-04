Import("env")

# Hybrid mode first builds the SDK ("Build"), then runs the Arduino application
# build with the copied archives. Linking these names in the SDK stage would
# require archives that have not been copied into the Arduino package yet.
if env.subst("$ARDUINO_LIB_COMPILE_FLAG") != "Build":
    env.AppendUnique(LIBS=["espressif__led_indicator", "espressif__led_strip"])
