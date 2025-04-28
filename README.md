## Background
I've recently come across multiple projects where people used ESPHome based devices
in their camper vans. Those devices were obviously connected to the main device running
 HASS via WiFi. That made me thinking - what if we could use CAN instead of WiFi? 
 CAN protocol is used by numerous electronic modules in vehicles enabling them 
 to talk to each other, so it could probably have some advantages over WiFi in camper 
 van scenarios too.
* You wouldn't need to set up credentials for every device.
* No reception problems if some device needs to be placed into some metal box.
* May have some security benefits as well - e.g. you don't have to reconnect all the 
devices if your WiFi password gets exposed.

## Hardware
My test setup was a simple SLCAN USB 2 CAN dongle (from Aliexpress) and a couple of 
ESP32C3 + SN65HVD230

## How to build.
Since CAN is not a standard ESP transport protocol it requires a bit of brain surgery 
to build this project.

### Patch Platformio 
The instance of Platformio that your ESPHome install depends on needs to be patched. 
This has been tested on ESPHome 2025.3.3 and Platfromio 6.1.18 and might require some 
additional effort to build with other versions.
1) Go to  `~/.platformio/packages/framework-espidf/components/mqtt` 
  and apply `esp-mqtt/esp-mqtt.patch`
2) Copy `esp_can_transport/can_transport` to 
  `~/.platformio/packages/framework-espidf/components`

### Build and flash the firmware
In `esphome_example` run `esphome run mqtt_can_test.yaml`
Note: `esphome_example` contains a patched version of the standard ESPHome mqtt component. 
The only modified file is mqtt_client.c. Search for H42_CAN_PATCH to see the changes.

### Start CAN - MQTT bridge
In can_mqtt_bridge edit main.py to set your CAN dongle port and Mosquitto host and run 
`python main.py`

Your new device should pop up in Home Assistant.

## Notes:
* The current CAN-MQTT bridge implementation is just a quick POC and it is terribly suboptimal.
  It launches multiple thread per each client device. The main reason for that is the Python iso-tp 
  library that supports only one session, so multiple instances need to be launched.