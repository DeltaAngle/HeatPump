//#define OTA

// wifi settings
const char* ssid     = "YOUR_SSID";
const char* password = "YOUR_PSK";

// mqtt server settings
const char* mqtt_server   = "MQTT_SERVER_IP";
const int mqtt_port       = 1883;
const char* mqtt_username = "YOUR_MQTT_LOGIN";
const char* mqtt_password = "YOUR_MQTT_PASSWORD";

//Homeassistant configuration
// mqtt ha discovery
// Note PubSubClient.h has a MQTT_MAX_PACKET_SIZE of 128 defined, so either raise it to 256 or use short topics

String ha_discovery_topic          = "homeassitant"; // Discovery topic for HA
String ha_friendly_name            = "hvac_1";  // Heatpump name for HA and MQTT

// mqtt base topic client settings
String heatpump_topic              = "heatpump";

// debug mode, when true, will send all packets received from the heatpump to topic heatpump_debug_topic
// this can also be set by sending "on" to heatpump_debug_set_topic
bool _debugMode = false;

// pinouts
const int redLedPin  = 0; // Onboard LED = digital pin 0 (red LED on adafruit ESP8266 huzzah)
const int blueLedPin = 2; // Onboard LED = digital pin 0 (blue LED on adafruit ESP8266 huzzah)

// sketch settings
const unsigned int SEND_ROOM_TEMP_INTERVAL_MS = 30000;
