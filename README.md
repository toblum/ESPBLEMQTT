# ESPBLEMQTT
Simple ESP32 BLE scanner with MQTT connection


## Goal of this project
This is a very simple writedown of a BLE to MQTT bridge. This sketch scan for BLE devices that advertise themselves. The result is sent as JSON via MQTT.

The project can easily be compiled using platform.io (recommended) and also via Arduino IDE.

## Used libraries
- [IotWebConf@2.3.1](https://github.com/prampec/IotWebConf)
- [NimBLE-Arduino](https://github.com/h2zero/NimBLE-Arduino)
- [ArduinoJson@6.15.2](https://arduinojson.org/)
- [MQTT@2.4.7](https://github.com/256dpi/arduino-mqtt)

Thanks to all the authors.

## Setup
This project uses the great IotWebConf library that has a nice configuration UI. 
- Just flash your device and power it up.
- The ESP spins up a access point with the name "ESPBLEMQTT" and password "configureme". Connect to this AP using a smartphone or PC.
- If connected, open the URL http://192.168.4.1/config. In most cases this should happen automatically.
- Fill in the requested data:
    - Thing name: Name of the device, also used for the local access point. Change only when you have multiple device in place.
    - AP password: Default "configureme", also used as password for the configuration page (user: "admin").
    - WiFi SSID: The SSID of your WiFi, you want the device to connect with.
    - WiFi password: The password of your local WiFi, you want the device to connect with.
    - Startup delay (seconds): Number of seconds the local AP is enabled after startup. Leave this at a low level.
    - MQTT hostname / username / password / port: The connection data of your MQTT server you want to use.
    - MQTT topic: Topic you want to use for MQTT communication (default: "espblemqtt").
        - If your topic is for example "espblemqtt" then the device will listen at "espblemqtt/cmd" for commands and publish data to "espblemqtt/data".
    - Scan interval (sec): Interval in seconds in that the device starts a new scan. Leave it at 0 to disable automatic scanning.
    - Scan time (sec): Time the ESP32 scans for BLE devices.
- Click "apply". Wait for confirmation.
- Restart ESP32.
- The ESP should now connect to your local Wifi and start working. You can check the result in the serial monitor.
- The config interface is reachable via web on the device IP, e.g. http://192.168.0.100/config

## Trigger manual scan
If you didn't eable automatic scanning or just want to start a scan manually, just send the payload "scan" to "<your_configured_topic>/cmd". After the scan time you should receive the result at "<your_configured_topic>/data".

## JSON result
The result should look like this:

```json
[
    {"address":"53:23:0a:0b:ce:9a","rssi":-61,"manufacturer":"4g001d063711414e1de1","tx_power":8},
    {"address":"ad:4:38:02:4d:af","rssi":-67,"name":"LYWSD03MMC"},
    {"address":"0b:17:03:0e:ab:4f","rssi":-61,"service_uuid":"0xfd6f"},
    {"address":"4d:4e:46:6e:4d:c8","rssi":-70,"manufacturer":"5f03500ded23a6958b","tx_power":12},
    {"address":"54:5d:5e:43:a9:d7","rssi":-75,"manufacturer":"6b001dab031c8a5402","tx_power":12}
]
```

## Licence
The code is licensed under the MPLv2 license.