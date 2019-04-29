#include <ESP8266WiFi.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <HeatPump.h>

#include "mitsubishi_mqtt_ha.h"

#ifdef OTA
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>
#endif

// setup HA topics
String ha_power_set_topic    = heatpump_topic + "/" + ha_friendly_name + "/power/set";
String ha_mode_set_topic     = heatpump_topic + "/" + ha_friendly_name + "/mode/set";
String ha_temp_set_topic     = heatpump_topic + "/" + ha_friendly_name + "/temp/set";
String ha_fan_set_topic      = heatpump_topic + "/" + ha_friendly_name + "/fan/set";
String ha_vane_set_topic     = heatpump_topic + "/" + ha_friendly_name + "/vane/set";
String ha_wideVane_set_topic = heatpump_topic + "/" + ha_friendly_name + "/wideVane/set";
String ha_state_topic        = heatpump_topic + "/" + ha_friendly_name + "/state";
String ha_debug_topic        = heatpump_topic + "/" + ha_friendly_name + "/debug";
String ha_debug_set_topic    = heatpump_topic + "/" + ha_friendly_name + "/debug/set";

String ha_config_topic       = "homeassistant/climate/" + ha_friendly_name + "/config";

String uniqueID;
// wifi, mqtt and heatpump client instances
WiFiClient espClient;
PubSubClient mqtt_client(espClient);
HeatPump hp;
unsigned long lastTempSend;

void setup() {

  pinMode(redLedPin, OUTPUT);
  digitalWrite(redLedPin, HIGH);
  pinMode(blueLedPin, OUTPUT);
  digitalWrite(blueLedPin, HIGH);
  WiFi.mode(WIFI_STA);
  WiFi.hostname(ha_friendly_name);
  WiFi.begin(ssid, password);
  uniqueID = ha_friendly_name + "_" + WiFi.macAddress();
  
  while (WiFi.status() != WL_CONNECTED) {
    // wait 500ms, flashing the blue LED to indicate WiFi connecting...
    digitalWrite(blueLedPin, LOW);
    delay(250);
    digitalWrite(blueLedPin, HIGH);
    delay(250);
  }

  // startup mqtt connection
  mqtt_client.setServer(mqtt_server, mqtt_port);
  mqtt_client.setCallback(mqttCallback);
  mqttConnect();
  haConfig();
  hp.setSettingsChangedCallback(hpSettingsChanged);
  hp.setStatusChangedCallback(hpStatusChanged);
  hp.setPacketCallback(hpPacketDebug);

#ifdef OTA
  ArduinoOTA.setHostname(ha_friendly_name.c_str());
  ArduinoOTA.begin();
#endif

  hp.connect(&Serial);

  lastTempSend = millis();
}

void hpSettingsChanged() {
  // send room temp, operating info and all information
  heatpumpSettings currentSettings = hp.getSettings();

  const size_t bufferSizeInfo = JSON_OBJECT_SIZE(7);
  StaticJsonDocument<bufferSizeInfo> rootInfo;

  //  JsonObject root Info = jsonBufferInfo.createObject();
  rootInfo["roomTemperature"] = hp.CelsiusToFahrenheit(hp.getRoomTemperature());
  rootInfo["temperature"]     = hp.CelsiusToFahrenheit(currentSettings.temperature);
  rootInfo["fan"]             = currentSettings.fan;
  rootInfo["vane"]            = currentSettings.vane;
  rootInfo["wideVane"]        = currentSettings.wideVane;

  String hppower = String(currentSettings.power);
  String hpmode = String(currentSettings.mode);

  hppower.toLowerCase();
  hpmode.toLowerCase();
  
  if (hpmode == "fan") {   //Change mode name for HA to interpret
    rootInfo["mode"] = "fan_only";
  }
  else if(hppower == "off") {
    rootInfo["mode"] = "off";  
  }
  else {
    rootInfo["mode"] = hpmode.c_str();
  }

  String mqttOutput;
  //rootInfo.printTo(bufferInfo, sizeof(bufferInfo));
  serializeJson(rootInfo, mqttOutput);

  if (!mqtt_client.publish(ha_state_topic.c_str(), mqttOutput.c_str(), true)) {
    mqtt_client.publish(ha_debug_topic.c_str(), "failed to publish hp status");
  }
}

void hpStatusChanged(heatpumpStatus currentStatus) {
  // send room temp, operating info and all information
  heatpumpSettings currentSettings = hp.getSettings();

  const size_t bufferSizeInfo = JSON_OBJECT_SIZE(6);
  StaticJsonDocument<bufferSizeInfo> rootInfo;

  //  JsonObject root Info = jsonBufferInfo.createObject();
  rootInfo["roomTemperature"] = hp.CelsiusToFahrenheit(hp.getRoomTemperature());
  rootInfo["temperature"]     = hp.CelsiusToFahrenheit(currentSettings.temperature);
  rootInfo["fan"]             = currentSettings.fan;
  rootInfo["vane"]            = currentSettings.vane;
  rootInfo["wideVane"]        = currentSettings.wideVane;

  String hppower = String(currentSettings.power);
  String hpmode = String(currentSettings.mode);

  hppower.toLowerCase();
  hpmode.toLowerCase();
  
  if (hpmode == "fan") {  //Change mode name for HA to interpret
    rootInfo["mode"] = "fan_only";
  }
  else if(hppower == "off") {
    rootInfo["mode"] = "off";  
  }
  else {
    rootInfo["mode"] = hpmode.c_str();
  }

  String mqttOutput;
  //rootInfo.printTo(bufferInfo, sizeof(bufferInfo));
  serializeJson(rootInfo, mqttOutput);

  if (!mqtt_client.publish(ha_state_topic.c_str(), mqttOutput.c_str())) {
    mqtt_client.publish(ha_debug_topic.c_str(), "failed to publish hp status change");
  }
}

void hpPacketDebug(byte* packet, unsigned int length, char* packetDirection) {
  if (_debugMode) {
    String message;
    for (int idx = 0; idx < length; idx++) {
      if (packet[idx] < 16) {
        message += "0"; // pad single hex digits with a 0
      }
      message += String(packet[idx], HEX) + " ";
    }

    const size_t bufferSize = JSON_OBJECT_SIZE(1);
    StaticJsonDocument<bufferSize> root;

    //    JsonObject root = jsonBuffer.createObject();

    root[packetDirection] = message;
    String mqttOutput;
    //root.printTo(buffer, sizeof(buffer));
    serializeJson(root, mqttOutput);
    if (!mqtt_client.publish(ha_debug_topic.c_str(), mqttOutput.c_str())) {
      mqtt_client.publish(ha_debug_topic.c_str(), "failed to publish to heatpump/debug topic");
    }
  }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  // Copy payload into message buffer
  char message[length + 1];
  for (int i = 0; i < length; i++) {
    message[i] = (char)payload[i];
  }
  message[length] = '\0';

  // HA topics
  if (strcmp(topic, ha_power_set_topic.c_str()) == 0) {
    hp.setPowerSetting(message);
    hp.update();
  }
  else if (strcmp(topic, ha_mode_set_topic.c_str()) == 0) {
    const size_t bufferSize = JSON_OBJECT_SIZE(2);
    StaticJsonDocument<bufferSize> root;
    root["mode"] = message;
    String mqttOutput;
    serializeJson(root, mqttOutput);
    mqtt_client.publish(ha_state_topic.c_str(), mqttOutput.c_str());
    String modeUpper = message;
    if (modeUpper == "fan_only") {  //Change mode name for heatpump to interpret
      modeUpper = "fan";
    }
    modeUpper.toUpperCase();
        hp.setModeSetting(modeUpper.c_str());
    hp.update();
  }
  else if (strcmp(topic, ha_temp_set_topic.c_str()) == 0) {
    float temperature = strtof(message, NULL);
    const size_t bufferSize = JSON_OBJECT_SIZE(2);
    StaticJsonDocument<bufferSize> root;
    root["temperature"] = message;
    String mqttOutput;
    serializeJson(root, mqttOutput);
    mqtt_client.publish(ha_state_topic.c_str(), mqttOutput.c_str());
    hp.setTemperature(hp.FahrenheitToCelsius(temperature));
    hp.update();
  }
  else if (strcmp(topic, ha_fan_set_topic.c_str()) == 0) {
    const size_t bufferSize = JSON_OBJECT_SIZE(2);
    StaticJsonDocument<bufferSize> root;
    root["fan"] = message;
    String mqttOutput;
    serializeJson(root, mqttOutput);
    mqtt_client.publish(ha_state_topic.c_str(), mqttOutput.c_str());
    hp.setFanSpeed(message);
    hp.update();
  }
  else if (strcmp(topic, ha_vane_set_topic.c_str()) == 0) {
    const size_t bufferSize = JSON_OBJECT_SIZE(2);
    StaticJsonDocument<bufferSize> root;
    root["vane"] = message;
    String mqttOutput;
    serializeJson(root, mqttOutput);
    mqtt_client.publish(ha_state_topic.c_str(), mqttOutput.c_str());
    hp.setVaneSetting(message);
    hp.update();
  }
  else if (strcmp(topic, ha_wideVane_set_topic.c_str()) == 0) {
    const size_t bufferSize = JSON_OBJECT_SIZE(2  );
    StaticJsonDocument<bufferSize> root;
    root["wideVane"] = message;
    String mqttOutput;
    serializeJson(root, mqttOutput);
    mqtt_client.publish(ha_state_topic.c_str(), mqttOutput.c_str());
    hp.setWideVaneSetting(message);
    hp.update();
  }
  else if (strcmp(topic, ha_debug_set_topic.c_str()) == 0) { //if the incoming message is on the heatpump_debug_set_topic topic...
    if (strcmp(message, "on") == 0) {
      _debugMode = true;
      mqtt_client.publish(ha_debug_topic.c_str(), "debug mode enabled");
    } else if (strcmp(message, "off") == 0) {
      _debugMode = false;
      mqtt_client.publish(ha_debug_topic.c_str(), "debug mode disabled");
    }
  } else {
    mqtt_client.publish(ha_debug_topic.c_str(), strcat("heatpump: wrong mqtt topic: ", topic));
  }
}

void haConfig() {
  
  // send HA config packet
  // setup HA payload device
  const int bufferSize = JSON_OBJECT_SIZE(48);
  StaticJsonDocument<bufferSize> haConfig;

  haConfig["name"]                          = ha_friendly_name;
  haConfig["mode_cmd_t"]                    = "heatpump/" + ha_friendly_name + "/mode/set";
  haConfig["mode_stat_t"]                   = "heatpump/" + ha_friendly_name + "/state";
  haConfig["mode_stat_tpl"]                 = "{{ value_json.mode }}";
  haConfig["temp_cmd_t"]                    = "heatpump/" + ha_friendly_name + "/temp/set";
  haConfig["temp_stat_t"]                   = "heatpump/" + ha_friendly_name + "/state";
  haConfig["temp_stat_tpl"]                 = "{{ value_json.temperature }}";
  haConfig["curr_temp_t"]                   = "heatpump/" + ha_friendly_name + "/state";
  haConfig["current_temperature_template"]  = "{{ value_json.roomTemperature }}";
  haConfig["min_temp"]                      = "61";
  haConfig["max_temp"]                      = "88";
  haConfig["unique_id"]                     = uniqueID;
  haConfig["modes"]                         = serialized("[\"auto\",\"off\",\"cool\",\"heat\",\"dry\",\"fan_only\"]");
  haConfig["fan_modes"]                     = serialized("[\"AUTO\",\"1\",\"2\",\"3\",\"4\"]");
  haConfig["swing_modes"]                   = serialized("[\"AUTO\",\"1\",\"2\",\"3\",\"4\",\"5\",\"SWING\"]");
  haConfig["pow_cmd_t"]                     = "heatpump/" + ha_friendly_name + "/power/set";
  haConfig["fan_mode_cmd_t"]                = "heatpump/" + ha_friendly_name + "/fan/set";
  haConfig["fan_mode_stat_t"]               = "heatpump/" + ha_friendly_name + "/state";
  haConfig["fan_mode_stat_tpl"]             = "{{ value_json.fan }}";
  haConfig["swing_mode_cmd_t"]              = "heatpump/" + ha_friendly_name + "/vane/set";
  haConfig["swing_mode_stat_t"]             = "heatpump/" + ha_friendly_name + "/state";
  haConfig["swing_mode_stat_tpl"]           = "{{ value_json.vane }}";
  
  JsonObject device = haConfig.createNestedObject("device");
  device["ids"]                             = "HeatPumpMQTT_" + ha_friendly_name;
  device["name"]                            = ha_friendly_name;
  device["sw"]                              = "HeatPump (MQTT) 1.0";
  device["mdl"]                             = "HVAC MiniSplit";
  device["mf"]                              = "MITSUBISHI";
 
  String mqttOutput;
  serializeJson(haConfig, mqttOutput);
  mqtt_client.beginPublish(ha_config_topic.c_str(), mqttOutput.length(), true);
  mqtt_client.print(mqttOutput);
  mqtt_client.endPublish();
}

void mqttConnect() {

  // Loop until we're reconnected
  while (!mqtt_client.connected()) {
    // Attempt to connect
    if (mqtt_client.connect(uniqueID.c_str(), mqtt_username, mqtt_password)) {
      //haConfig();
      mqtt_client.subscribe(ha_debug_set_topic.c_str());
      mqtt_client.subscribe(ha_power_set_topic.c_str());
      mqtt_client.subscribe(ha_mode_set_topic.c_str());
      mqtt_client.subscribe(ha_fan_set_topic.c_str());
      mqtt_client.subscribe(ha_temp_set_topic.c_str());
      mqtt_client.subscribe(ha_vane_set_topic.c_str());
      mqtt_client.subscribe(ha_wideVane_set_topic.c_str());
     
    } else {
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void loop() {

  if (!mqtt_client.connected()) {
    mqttConnect();
  }

  hp.sync();
  if (millis() > (lastTempSend + SEND_ROOM_TEMP_INTERVAL_MS)) { // only send the temperature every SEND_ROOM_TEMP_INTERVAL_MS
    hpStatusChanged(hp.getStatus());
    lastTempSend = millis();
  }

  mqtt_client.loop();

#ifdef OTA
  ArduinoOTA.handle();
#endif
}
