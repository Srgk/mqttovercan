from os.path import join, isfile

Import("env")

FRAMEWORK_DIR = env.PioPlatform().get_package_dir("framework-espidf")
print("framework dir: %s" % (FRAMEWORK_DIR))
patchflag_path = join(FRAMEWORK_DIR, ".patching-done")

# patch file only if we didn't do it before
if not isfile(join(FRAMEWORK_DIR, ".patching-done")):
    original_file = join(FRAMEWORK_DIR, "variants", "standard", "pins_arduino.h")
    patched_file = join("patches", "1-framework-arduinoavr-add-pin-a8.patch")
    
    print("Apply MQTT client patches")
    env.Execute("rm -fr %s/mqttovercan || true" % (FRAMEWORK_DIR))
    env.Execute("rm -fr %s/components/can_transport || true" % (FRAMEWORK_DIR))

    env.Execute("git clone https://github.com/Srgk/mqttovercan.git %s/mqttovercan" % (FRAMEWORK_DIR))
    env.Execute("cp -r %s/mqttovercan/esp_can_transport/can_transport %s/components" % (FRAMEWORK_DIR, FRAMEWORK_DIR))

    env.Execute("patch -p1 -d %s/components/mqtt/esp-mqtt<%s/mqttovercan/esp-mqtt/esp-mqtt.patch" % (FRAMEWORK_DIR, FRAMEWORK_DIR))

    env.Execute("touch " + patchflag_path)

else:
    print("MQTT patches have already been applied");
