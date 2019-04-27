/*
  Hostel-IoT

  Send sensor data to from hostel room to server with MQTT
  27 April 2019   v2.0
  by Febrianto AN

  v2.0 with wifi manager
*/

#include <FS.h>                   //this needs to be first, or it all crashes and burns...
#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager
#include <ArduinoJson.h>          //https://github.com/bblanchon/ArduinoJson
#include <MQTT.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#define ONE_WIRE_BUS 2 //D1           // DS18B20 Sensor suhu, sesuaikan pin disini!
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

const int pinMotion = 0;//D6; //0;    // PIR motion sensor, sesuaikan pin disini!

/* DEVICE ID */
char const * deviceID = "ndoware-" + ESP.getChipId();

/* sersors */
boolean motionEnable = true;
boolean motionDummy = false;
boolean tempetureEnable = true;
boolean tempetureDummy = false;

/* interval sending data */
int iMenit = 0;   //minute
int iDetik = 10;   //s


boolean debug = true;

/* ------------------------------------------------------ */
WiFiClient net;
MQTTClient client;

unsigned long lastMillis = 0;
unsigned long lastBeatMillis = 0;
String topic;

//define your default values here, if there are different values in config.json, they are overwritten.
char mqtt_server[40] = "52.74.248.240";
char mqtt_port[6] = "1883";

char mqtt_user[32] = "****";
char mqtt_pass[32] = "****";

char hostel_client[40];
char hostel_region[40];
char hostel_location[40];
char hostel_room[40];


//flag for saving data
bool shouldSaveConfig = true;

//callback notifying us of the need to save config
void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}


void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  Serial.println();
  Serial.println("before");
  printConfig();
  //clean FS, for testing
  //SPIFFS.format();

  //read configuration from FS json
  Serial.println("mounting FS...");

  if (SPIFFS.begin()) {
    Serial.println("mounted file system");
    if (SPIFFS.exists("/config.json")) {
      //file exists, reading and loading
      Serial.println("reading config file");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
        Serial.println("opened config file");
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject(buf.get());
        json.printTo(Serial);
        if (json.success()) {
          Serial.println("\nparsed json");

          strcpy(mqtt_server, json["mqtt_server"]);
          strcpy(mqtt_port, json["mqtt_port"]);
          strcpy(mqtt_user, json["mqtt_user"]);
          strcpy(mqtt_pass, json["mqtt_pass"]);

          strcpy(hostel_client, json["hostel_client"]);
          strcpy(hostel_region, json["hostel_region"]);
          strcpy(hostel_location, json["hostel_location"]);
          strcpy(hostel_room, json["hostel_room"]);

        } else {
          Serial.println("failed to load json config");
        }
        configFile.close();
      }
    }
  } else {
    Serial.println("failed to mount FS");
  }
  //end read



  // The extra parameters to be configured (can be either global or just in the setup)
  // After connecting, parameter.getValue() will get you the configured value
  // id/name placeholder/prompt default length
  WiFiManagerParameter custom_mqtt_server("server", "mqtt server", mqtt_server, 40);
  WiFiManagerParameter custom_mqtt_port("port", "mqtt port", mqtt_port, 6);

  WiFiManagerParameter custom_mqtt_user("user", "mqtt user", mqtt_user, 32);
  WiFiManagerParameter custom_mqtt_pass("pass", "mqtt pass", mqtt_pass, 32);

  WiFiManagerParameter custom_hostel_client("client", "hostel client", hostel_client, 40);
  WiFiManagerParameter custom_hostel_region("region", "hostel region", hostel_region, 40);
  WiFiManagerParameter custom_hostel_location("location", "hostel location", hostel_location, 40);
  WiFiManagerParameter custom_hostel_room("room", "hostel room", hostel_room, 40);

  //WiFiManager
  //Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;

  //set config save notify callback
  wifiManager.setSaveConfigCallback(saveConfigCallback);

  //set static ip
  //wifiManager.setSTAStaticIPConfig(IPAddress(10,0,1,99), IPAddress(10,0,1,1), IPAddress(255,255,255,0));

  //add all your parameters here

  WiFiManagerParameter custom_text1("<small>Topics</small>");
  wifiManager.addParameter(&custom_text1);
  wifiManager.addParameter(&custom_hostel_client);
  wifiManager.addParameter(&custom_hostel_region);
  wifiManager.addParameter(&custom_hostel_location);
  wifiManager.addParameter(&custom_hostel_room);
  WiFiManagerParameter custom_text2("<small>Host</small>");
  wifiManager.addParameter(&custom_text2);
  wifiManager.addParameter(&custom_mqtt_server);
  wifiManager.addParameter(&custom_mqtt_port);
  wifiManager.addParameter(&custom_mqtt_user);
  wifiManager.addParameter(&custom_mqtt_pass);


  wifiManager.setCustomHeadElement("<style>body{  background-image: linear-gradient(to right, #ffffff 0%, #eeeeee 75%);}h1{font-family: serif;text-shadow: 2px 2px rgba(0,0,0,0.19)}h3{color: #dddddd;}button{background-color:#ec741f;  box-shadow: 0 8px 16px 0 rgba(0,0,0,0.2), 0 6px 20px 0 rgba(0,0,0,0.19);}</style>");

  //reset settings - for testing
  //wifiManager.resetSettings();

  //set minimum quality of signal so it ignores AP's under that quality
  //defaults to 8%
  //wifiManager.setMinimumSignalQuality();

  //sets timeout until configuration portal gets turned off
  //useful to make it all retry or go to sleep
  //in seconds
  //wifiManager.setTimeout(120);

  //fetches ssid and pass and tries to connect
  //if it does not connect it starts an access point with the specified name
  //here  "AutoConnectAP"
  //and goes into a blocking loop awaiting configuration
  if (!wifiManager.autoConnect("Ndoware-device", "password")) {
    Serial.println("failed to connect and hit timeout");
    delay(3000);
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(5000);
  }

  //if you get here you have connected to the WiFi
  Serial.println("connected...yeey :)");

  //read updated parameters
  strcpy(mqtt_server, custom_mqtt_server.getValue());
  strcpy(mqtt_port, custom_mqtt_port.getValue());
  strcpy(mqtt_user, custom_mqtt_user.getValue());
  strcpy(mqtt_pass, custom_mqtt_pass.getValue());

  strcpy(hostel_client, custom_hostel_client.getValue());
  strcpy(hostel_region, custom_hostel_region.getValue());
  strcpy(hostel_location, custom_hostel_location.getValue());
  strcpy(hostel_room, custom_hostel_room.getValue());

  //
  Serial.println("after");
  printConfig();

  //save the custom parameters to FS
  if (shouldSaveConfig) {
    Serial.println("saving config");
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
    json["mqtt_server"] = mqtt_server;
    json["mqtt_port"] = mqtt_port;
    json["mqtt_user"] = mqtt_user;
    json["mqtt_pass"] = mqtt_pass;

    json["hostel_client"] = hostel_client;
    json["hostel_region"] = hostel_region;
    json["hostel_location"] = hostel_location;
    json["hostel_room"] = hostel_room;

    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile) {
      Serial.println("failed to open config file for writing");
    }

    json.printTo(Serial);
    json.printTo(configFile);
    configFile.close();
    //end save
  }

  Serial.println("local ip");
  Serial.println(WiFi.localIP());

  //  MQTT
  client.begin(mqtt_server, String(mqtt_port).toInt(), net);
  connect();
  topic = "hostel-iot/" + String(hostel_client) + "/" + String(hostel_region) + "/" + String(hostel_location) + "/" + String(hostel_room) + "/";


  // Dallas sensor
  sensors.begin();

  pinMode(pinMotion, INPUT);

}

void loop() {
  // put your main code here, to run repeatedly:
  client.loop();
  delay(10);  // <- fixes some issues with WiFi stability

  if (!client.connected()) {
    connect();
  }

  //Serial.print("Requesting temperatures...");
  sensors.requestTemperatures(); // Send the command to get temperatures
  //Serial.println("DONE");
  //Serial.print("Temperature for the device 1 (index 0) is: ");
  float temperaturSense = sensors.getTempCByIndex(0);
  //Serial.println(temperaturSense);

  byte motion = digitalRead(pinMotion);
  String devId = String(deviceID);
  // publish a message roughly every interval
  unsigned long interval = ((iMenit * 60) + iDetik) * 1000;
  if ((lastMillis == 0) || (millis() - lastMillis > interval)) {
    lastMillis = millis();

    // motion detector sensor data
    if (motionEnable) {
      String deviceType1 = "motion";
      String topic1 = topic + deviceType1 + "/";
      String payload1 = motionDummy? String(random(2)) :String(motion);
      client.publish(topic1, payload1);
      Serial.println(devId + " outgoing: " + topic1 + " - " + payload1);

    }

    // temperature sensor data
    if (tempetureEnable) {
      String deviceType2 = "temp";
      String topic2 = topic + deviceType2 + "/";
      String payload2 = tempetureDummy?String(random(16, 37)) : String(temperaturSense);
      client.publish(topic2, payload2);
      Serial.println(devId + " outgoing: " + topic2 + " - " + payload2);
    }
  }

}

void connect() {
  Serial.print("Connecting...");
  while (!client.connect(deviceID, mqtt_user, mqtt_pass)) {
    Serial.print(".");
    digitalWrite(LED_BUILTIN, LOW);
    delay(500);
    digitalWrite(LED_BUILTIN, HIGH);
    delay(500);
  }

  Serial.println("\nconnected!");
}
void printConfig() {
  Serial.println(mqtt_server);
  Serial.println(mqtt_port);
  Serial.println(mqtt_user);
  Serial.println(mqtt_pass);

  Serial.println(hostel_client);
  Serial.println(hostel_region);
  Serial.println(hostel_location);
  Serial.println(hostel_room);
}