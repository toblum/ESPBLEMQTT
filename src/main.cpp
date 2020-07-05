/**
 * ESP32 BLE scanner with MQTT connection
 * https://github.com/toblum/ESPBLEMQTT
 *
 * Copyright (C) 2020 Tobias Blum <make@tobiasblum.de>
 *
 * This software may be modified and distributed under the terms
 * of the MPL license. See the LICENSE file for details.
 */

#include <Arduino.h>
#include <MQTT.h>
#include <IotWebConf.h>

#include <ArduinoJson.h>

#include <NimBLEDevice.h>
#include <NimBLEAdvertisedDevice.h>
#include "BLEAdvertisedDeviceCallbacks.h"

// -- Initial name of the Thing. Used e.g. as SSID of the own Access Point.
const char thingName[] = "ESPBLEMQTT";

// -- Initial password to connect to the Thing, when it creates an own Access Point.
const char wifiInitialApPassword[] = "configureme";

#define STRING_LEN 128
#define NUMBER_LEN 6
#define RESULT_LEN 4096

// -- Configuration specific key. The value should be modified if config structure was changed.
#define CONFIG_VERSION "EBM_001"

// -- Status indicator pin.
//      First it will light up (kept LOW), on Wifi connection it will blink,
//      when connected to the Wifi it will turn off (kept HIGH).
#define STATUS_PIN LED_BUILTIN

// -- Callback method declarations.
void configSaved();
boolean formValidator();

DNSServer dnsServer;
WebServer server(80);

char mqttHostname[STRING_LEN];
char mqttUsername[STRING_LEN];
char mqttPassword[STRING_LEN];
char mqttPort[NUMBER_LEN];
char mqttTopic[STRING_LEN];
char scanInterval[NUMBER_LEN];
char scanTime[NUMBER_LEN];

IotWebConf iotWebConf(thingName, &dnsServer, &server, wifiInitialApPassword, CONFIG_VERSION);
IotWebConfSeparator separator1 = IotWebConfSeparator("MQTT");
IotWebConfParameter param1 = IotWebConfParameter("MQTT hostname", "mqttHostname", mqttHostname, STRING_LEN);
IotWebConfParameter param2 = IotWebConfParameter("MQTT username", "mqttUsername", mqttUsername, STRING_LEN);
IotWebConfParameter param3 = IotWebConfParameter("MQTT password", "mqttPassword", mqttPassword, STRING_LEN);
IotWebConfParameter param4 = IotWebConfParameter("MQTT port", "mqttPort", mqttPort, NUMBER_LEN, "number", "1833", "1..65535", "min='1' max='65535' step='1'");
IotWebConfParameter param5 = IotWebConfParameter("MQTT topic", "mqttTopic", mqttTopic, STRING_LEN);
IotWebConfSeparator separator2 = IotWebConfSeparator("General");
IotWebConfParameter param6 = IotWebConfParameter("Scan interval (sec)", "scanInterval", scanInterval, NUMBER_LEN, "number", "60", "0..65535", "min='0' max='65535' step='1'");
IotWebConfParameter param7 = IotWebConfParameter("Scan time (sec)", "scanTime", scanTime, NUMBER_LEN, "number", "5", "1..65535", "min='1' max='65535' step='1'");

// BLE
BLEScan *pBLEScan;
// int scanTime = 5; //In seconds

// Generic
unsigned long nextBLEScan = 0;
DynamicJsonDocument doc(RESULT_LEN);

// MQTT
void mqttMessageReceived(String &topic, String &payload);
WiFiClient net;
MQTTClient mqttClient(RESULT_LEN);
boolean needMqttConnect = false;
unsigned long lastMqttConnectionAttempt = 0;


/**
 * Handle web requests to "/" path.
 */
void handleRoot()
{
	// -- Let IotWebConf test and handle captive portal requests.
	if (iotWebConf.handleCaptivePortal())
	{
		// -- Captive portal request were already served.
		return;
	}
	String s = "<!DOCTYPE html><html lang=\"en\"><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1, user-scalable=no\"/>";
	s += "<title>ESPBLEMQTT</title></head><body>";
	s += "<ul>";
	s += "<li>MQTT hostname: ";
	s += mqttHostname;
	s += "<li>MQTT username: ";
	s += mqttUsername;
	s += "<li>MQTT password: ";
	s += mqttPassword;
	s += "<li>MQTT port: ";
	s += atoi(mqttPort);
	s += "<li>MQTT topic: ";
	s += mqttTopic;
	s += "<li>Scan interval (sec): ";
	s += atoi(scanInterval);
	s += "<li>Scan time (sec): ";
	s += atoi(scanTime);
	s += "</ul><br>";
	s += "Go to <a href='config'>configure page</a> to change values.<br>";
	s += "GitHub: <a target='_blank' href='https://github.com/toblum/ESPBLEMQTT'>ESPBLEMQTT</a>";
	s += "</body></html>\n";

	server.send(200, "text/html", s);
}

void configSaved()
{
	Serial.println("Configuration was updated.");
}

boolean connectMqttOptions()
{
	boolean result;
	if (mqttPassword[0] != '\0')
	{
		result = mqttClient.connect(iotWebConf.getThingName(), mqttUsername, mqttPassword);
	}
	else if (mqttUsername[0] != '\0')
	{
		result = mqttClient.connect(iotWebConf.getThingName(), mqttUsername);
	}
	else
	{
		result = mqttClient.connect(iotWebConf.getThingName());
	}
	return result;
}

boolean connectMqtt()
{
	unsigned long now = millis();
	if (1000 > now - lastMqttConnectionAttempt)
	{
		// Do not repeat within 1 sec.
		return false;
	}
	Serial.println("Connecting to MQTT server...");
	if (!connectMqttOptions())
	{
		lastMqttConnectionAttempt = now;
		return false;
	}
	Serial.println("Connected!");

	char subscribeTopic[STRING_LEN];
	strcpy(subscribeTopic, mqttTopic);
	strcat(subscribeTopic, "/cmd");
	Serial.print("Subscribed: ");
	Serial.println(subscribeTopic);

	mqttClient.subscribe(subscribeTopic);
	return true;
}

void mqttMessageReceived(String &topic, String &payload)
{
	Serial.println("Incoming: " + topic + " - " + payload);
	if (payload.equals("scan")) {
		nextBLEScan = millis();
	}
}

boolean formValidator()
{
	Serial.println("Validating form.");
	boolean valid = true;

	//   int l = server.arg(stringParam.getId()).length();
	//   if (l < 3)
	//   {
	//     stringParam.errorMessage = "Please provide at least 3 characters for this test!";
	//     valid = false;
	//   }

	return valid;
}

void setup()
{
	Serial.begin(115200);
	Serial.println();
	Serial.println("Starting up...");

	iotWebConf.setStatusPin(STATUS_PIN);
	iotWebConf.addParameter(&separator1);
	iotWebConf.addParameter(&param1);
	iotWebConf.addParameter(&param2);
	iotWebConf.addParameter(&param3);
	iotWebConf.addParameter(&param4);
	iotWebConf.addParameter(&param5);
	iotWebConf.addParameter(&separator2);
	iotWebConf.addParameter(&param6);
	iotWebConf.addParameter(&param7);
	iotWebConf.setConfigSavedCallback(&configSaved);
	iotWebConf.setFormValidator(&formValidator);
	iotWebConf.getApTimeoutParameter()->visible = true;

	// -- Initializing the configuration.
	boolean validConfig = iotWebConf.init();
	if (!validConfig)
	{
		mqttHostname[0] = '\0';
		mqttUsername[0] = '\0';
		mqttPassword[0] = '\0';
		mqttPort[0] = '\0';
		mqttTopic[0] = '\0';
		scanInterval[0] = '\0';
		scanTime[0] = '\0';
	}

	// -- Set up required URL handlers on the web server.
	server.on("/", handleRoot);
	server.on("/config", [] { iotWebConf.handleConfig(); });
	server.onNotFound([]() { iotWebConf.handleNotFound(); });

	// MQTT
	mqttClient.begin(mqttHostname, net);
	mqttClient.onMessage(mqttMessageReceived);

	// BLE scanner
	BLEDevice::init("");
	pBLEScan = BLEDevice::getScan(); //create new scan
	// pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
	pBLEScan->setActiveScan(true); //active scan uses more power, but get results faster
	pBLEScan->setInterval(100);
	pBLEScan->setWindow(99); // less or equal setInterval value

	// If automatic scanning is enabled, schedule first scan
	if (atoi(scanInterval) > 0) {
		nextBLEScan = millis();
	}

	if (sizeof(mqttTopic) < 2) {
		strcpy(mqttTopic, "espblemqtt");
	}

	Serial.println("Ready.");
}

void loop()
{
	// -- doLoop should be called as frequently as possible.
	iotWebConf.doLoop();

	// MQTT
	mqttClient.loop();

	if (needMqttConnect)
	{
		if (connectMqtt())
		{
			needMqttConnect = false;
		}
	}
	else if ((iotWebConf.getState() == IOTWEBCONF_STATE_ONLINE) && (!mqttClient.connected()))
	{
		Serial.println("MQTT reconnect");
		connectMqtt();
	}

	// BLE scan
	if (iotWebConf.getState() == IOTWEBCONF_STATE_ONLINE)
	{
		unsigned long now = millis();

		// nextBLEScan == 0 means no automatic scanning
		if (nextBLEScan < now && nextBLEScan > 0)
		{
			Serial.println("Start BLE scan!");
			// Serial.println(atoi(scanTime));
			// Serial.println(atoi(scanInterval));
			int BLEscanTime = atoi(scanTime);
			if (BLEscanTime < 1) {
				BLEscanTime = 5;
			}
			BLEScanResults foundDevices = pBLEScan->start(BLEscanTime, false);
			Serial.print("Devices found: ");
			Serial.println(foundDevices.getCount());
			Serial.println("BLE scan done!");

			doc.clear();

			int count = foundDevices.getCount();
			for (int i = 0; i < count; i++)
			{
				JsonObject object = doc.createNestedObject();

				BLEAdvertisedDevice d = foundDevices.getDevice(i);
				// Serial.println(d.getAddress().toString().c_str());
				// Serial.println(d.getRSSI());
				object["address"] = d.getAddress().toString();
				object["rssi"] = d.getRSSI();

				if (d.haveName())
				{
					// Serial.println(d.getName().c_str());
					object["name"] = d.getName();
				}

				if (d.haveAppearance())
				{
					// Serial.println(d.getAppearance());
					object["appearance"] = d.getAppearance();
				}

				if (d.haveManufacturerData())
				{
					std::string md = d.getManufacturerData();
					uint8_t *mdp = (uint8_t *)d.getManufacturerData().data();
					char *pHex = BLEUtils::buildHexData(nullptr, mdp, md.length());
					object["manufacturer"] = pHex;
					free(pHex);
				}

				if (d.haveServiceUUID())
				{
					// Serial.println(d.getServiceUUID().toString().c_str());
					object["service_uuid"] = d.getServiceUUID().toString();
				}

				if (d.haveTXPower())
				{
					// Serial.println((int)d.getTXPower());
					object["tx_power"] = (int)d.getTXPower();
				}

				// Serial.println("-------------------------------------------");
			}

			// serializeJsonPretty(doc, Serial);

			char output[RESULT_LEN];
			serializeJson(doc, output);
			Serial.print("Payload size: ");
			Serial.println(measureJson(doc));
			
			char publishTopic[STRING_LEN];
			strcpy(publishTopic, mqttTopic);
			strcat(publishTopic, "/data");
			Serial.print("Published to: ");
			Serial.println(publishTopic);
			mqttClient.publish(publishTopic, output);
			Serial.println("===========================================");

			pBLEScan->clearResults(); // delete results fromBLEScan buffer to release memory

			int BLEscanInterval = atoi(scanInterval);

			// If scan interval is less then 1, do not scan automatically.
			if (BLEscanInterval > 0) {
				nextBLEScan = millis() + (BLEscanInterval * 1000);
			} else {
				nextBLEScan = 0;
			}
		}
	}
}
