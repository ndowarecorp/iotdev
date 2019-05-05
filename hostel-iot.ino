/*
  Hostel-IoT
  Send sensor data to from hostel room to server with MQTT
  author: Abu Azzam
  v1   Monday, 22 April 2019
       initial
  v2.0 Saturday, 27 April 2019
       with wifi manager
  v2.1 Sunday, 5 May 2019
       - more parameter
       - led notif

  idea?
  -    
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
#include <Ticker.h>

/* PIN                     esp01      wemos         */
const int pinTemperature = 2;       //D2;           // DS18B20 Sensor suhu
const int pinMotion =      0;       //D7;           // PIR motion sensor
const int pinLED =         1;       //LED_BUILTIN;  // LED
const int pinButton =      3;       //D1;           // Button

/* sersors enable */
boolean motionEnable = true;
boolean motionDummy = false;
boolean tempetureEnable = true;
boolean tempetureDummy = false;

boolean debug = false;

//define your default values here, if there are different values in config.json, they are overwritten.

char mqtt_server[40] = "52.74.248.240";
char mqtt_port[6] = "1883";
char mqtt_user[32];
char mqtt_pass[32];

char hostel_client[40];
char hostel_region[40];
char hostel_location[40];
char hostel_room[40];

char led_mon[3] = "on";   // LED monitor enable

/* interval get sensor data */
char sense_min[3] = "1";  // setiap n menit
char sense_sec[3] = "0";  // setiap n detik
/* interval sending data */
char pub_min[3] = "10";  // setiap n menit
char pub_sec[3] = "0";   // setiap n detik

//flag for saving data
bool shouldSaveConfig = true;

/* ------------------------------------------------------ */

// init variables
unsigned long lastSensMillis = 0;
unsigned long lastPushMillis = 0;
unsigned long lastButtonLow = 0;
byte motionFlag = 0;
int bufferMotion = 0;
int bufferTemp = 0;
int bufferTempCount = 0;
String topic;
int iSensMenit = 0;
int iSensDetik = 0;
int iPushMenit = 0;
int iPushDetik = 0;

OneWire oneWire(pinTemperature);
DallasTemperature sensors(&oneWire);
WiFiClient net;
MQTTClient client;
Ticker ticker;

void tick() {
  //toggle state
  int state = digitalRead(pinLED);  // get the current state of GPIO1 pin
  digitalWrite(pinLED, !state);     // set pin to the opposite state
}

//callback notifying us of the need to save config
void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

//gets called when WiFiManager enters configuration mode
void configModeCallback (WiFiManager *myWiFiManager) {
  Serial.println("Entered config mode");
  Serial.println(WiFi.softAPIP());
  //if you used auto generated SSID, print it
  Serial.println(myWiFiManager->getConfigPortalSSID());
  //entered config mode, make led toggle faster
  ticker.attach(0.2, tick);
}

void mqttConnect() {
  Serial.print("Connecting ");
  String stDeviceID;
  stDeviceID = "ndosense-" + String(ESP.getChipId());
  char deviceID[stDeviceID.length()];
  stDeviceID.toCharArray(deviceID, sizeof(deviceID));
  Serial.print(deviceID);
  while (!client.connect(deviceID, mqtt_user, mqtt_pass)) {
    Serial.print(".");
    digitalWrite(pinLED, LOW);
    delay(500);
    digitalWrite(pinLED, HIGH);
    delay(500);
  }
  Serial.println("\nconnected!");
}

void joggingLed() {
  digitalWrite(pinLED, HIGH); delay(700);
  for (int i = 10; i < 46; i++) {
    digitalWrite(pinLED, !digitalRead(pinLED));
    delay(i);
  }
  for (int i = 46; i > 10; i--) {
    digitalWrite(pinLED, !digitalRead(pinLED));
    delay(i);
  }
  Serial.println("I'm refreshed, ready for action boss!");
  digitalWrite(pinLED, HIGH); delay(700);
}

void setup() {
  // put your setup code here, to run once:
  if (debug) {
    Serial.begin(115200);
  }
  Serial.println();

  pinMode(pinButton, INPUT_PULLUP);
  pinMode(pinLED, OUTPUT);
  pinMode(pinTemperature, INPUT_PULLUP);
  pinMode(pinMotion, INPUT);


  joggingLed();

  // start ticker with 0.6 because we start in AP mode and try to connect
  ticker.attach(0.6, tick);

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

          strcpy(led_mon, json["led_mon"]);

          strcpy(sense_min, json["sense_min"]);
          strcpy(sense_sec, json["sense_sec"]);
          strcpy(pub_min, json["pub_min"]);
          strcpy(pub_sec, json["pub_sec"]);

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

  WiFiManagerParameter custom_text_hostel("<small>Topics</small>");
  WiFiManagerParameter custom_hostel_client("client", "hostel client", hostel_client, 40);
  WiFiManagerParameter custom_hostel_region("region", "hostel region", hostel_region, 40);
  WiFiManagerParameter custom_hostel_location("location", "hostel location", hostel_location, 40);
  WiFiManagerParameter custom_hostel_room("room", "hostel room", hostel_room, 40);

  WiFiManagerParameter custom_text_mqtt("<small>Host</small>");
  WiFiManagerParameter custom_mqtt_server("server", "mqtt server", mqtt_server, 40);
  WiFiManagerParameter custom_mqtt_port("port", "mqtt port", mqtt_port, 6);
  WiFiManagerParameter custom_mqtt_user("user", "mqtt user", mqtt_user, 32);
  WiFiManagerParameter custom_mqtt_pass("pass", "mqtt pass", mqtt_pass, 32, "type=\"password\"");

  WiFiManagerParameter custom_text_led("<small>Motion Sensor Light</small>");
  String typeTag = (String(led_mon) == "on") ? "type=\"checkbox\" checked" : "type=\"checkbox\"";
  char typeTagBuffer[30];
  typeTag.toCharArray(typeTagBuffer, 30);
  WiFiManagerParameter custom_led_mon("led", "led enable", "on", 3, typeTagBuffer);
  
  WiFiManagerParameter custom_text_dev("<hr/><small>(dev only)</small><br/>");
  WiFiManagerParameter custom_text_sense("<small>Interval SENSE room (minutes, seconds)</small>");
  WiFiManagerParameter custom_sense_min("sminutes", "sense minutes", sense_min, 3);
  WiFiManagerParameter custom_sense_sec("sseconds", "sense seconds", sense_sec, 3);
  WiFiManagerParameter custom_text_pub("<small>Interval PUBLISH data (minutes, seconds)</small>");
  WiFiManagerParameter custom_pub_min("pminutes", "publish minutes", pub_min, 3);
  WiFiManagerParameter custom_pub_sec("pseconds", "publish seconds", pub_sec, 3);

  //WiFiManager
  WiFiManager wifiManager;

  //set callback that gets called when connecting to previous WiFi fails, and enters Access Point mode
  wifiManager.setAPCallback(configModeCallback);

  //set config save notify callback
  wifiManager.setSaveConfigCallback(saveConfigCallback);

  //add all your parameters here
  wifiManager.addParameter(&custom_text_hostel);
  wifiManager.addParameter(&custom_hostel_client);
  wifiManager.addParameter(&custom_hostel_region);
  wifiManager.addParameter(&custom_hostel_location);
  wifiManager.addParameter(&custom_hostel_room);
  wifiManager.addParameter(&custom_text_mqtt);
  wifiManager.addParameter(&custom_mqtt_server);
  wifiManager.addParameter(&custom_mqtt_port);
  wifiManager.addParameter(&custom_mqtt_user);
  wifiManager.addParameter(&custom_mqtt_pass);
  wifiManager.addParameter(&custom_text_led);
  wifiManager.addParameter(&custom_led_mon);
  wifiManager.addParameter(&custom_text_dev);
  wifiManager.addParameter(&custom_text_sense);
  wifiManager.addParameter(&custom_sense_min);
  wifiManager.addParameter(&custom_sense_sec);
  wifiManager.addParameter(&custom_text_pub);
  wifiManager.addParameter(&custom_pub_min);
  wifiManager.addParameter(&custom_pub_sec);

  wifiManager.setCustomHeadElement("<style>body{  background-image: linear-gradient(to left, #ffffff 0%, #eeeeee 75%);}h1{font-family: serif;text-shadow: 2px 2px rgba(0,0,0,0.19)}h3{color: #dddddd;}button{background-color:#ec741f;  box-shadow: 0 8px 16px 0 rgba(0,0,0,0.2), 0 6px 20px 0 rgba(0,0,0,0.19);}</style>");

  //wifiManager.setMinimumSignalQuality();
  //wifiManager.setTimeout(120);

  //fetches ssid and pass and tries to connect
  //if it does not connect it starts an access point with the specified name
  //here  "AutoConnectAP"
  //and goes into a blocking loop awaiting configuration
  if (!wifiManager.autoConnect("Ndoware-device", "password")) {
    ticker.attach(0.6, tick);
    Serial.println("failed to connect and hit timeout");
    delay(3000);
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(5000);
  }

  //if you get here you have connected to the WiFi
  Serial.println("connected...yeey :)");

  ticker.attach(0.6, tick);

  //read updated parameters
  strcpy(mqtt_server, custom_mqtt_server.getValue());
  strcpy(mqtt_port, custom_mqtt_port.getValue());
  strcpy(mqtt_user, custom_mqtt_user.getValue());
  strcpy(mqtt_pass, custom_mqtt_pass.getValue());

  strcpy(hostel_client, custom_hostel_client.getValue());
  strcpy(hostel_region, custom_hostel_region.getValue());
  strcpy(hostel_location, custom_hostel_location.getValue());
  strcpy(hostel_room, custom_hostel_room.getValue());

  strcpy(led_mon, custom_led_mon.getValue());

  strcpy(sense_min, custom_sense_min.getValue());
  strcpy(sense_sec, custom_sense_sec.getValue());
  strcpy(pub_min, custom_pub_min.getValue());
  strcpy(pub_sec, custom_pub_sec.getValue());

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

    json["led_mon"] = led_mon;

    json["sense_min"] = sense_min;
    json["sense_sec"] = sense_sec;
    json["pub_min"] = pub_min;
    json["pub_sec"] = pub_sec;

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

  /* interval get sensor data */
  iSensMenit = atoi(sense_min);   //minute
  iSensDetik = atoi(sense_sec);   //s
  /* interval sending data */
  iPushMenit = atoi(pub_min);   //minute
  iPushDetik = atoi(pub_sec);   //

  //  MQTT
  client.begin(mqtt_server, String(mqtt_port).toInt(), net);
  mqttConnect();
  topic = "hostel-iot/" + String(hostel_client) + "/" + String(hostel_region) + "/" + String(hostel_location) + "/" + String(hostel_room) + "/";

  ticker.detach();

  // Dallas sensor
  sensors.begin();

  joggingLed();
  digitalWrite(pinMotion, LOW);
}

void loop() {
  // put your main code here, to run repeatedly:
  client.loop();
  delay(10);  // <- fixes some issues with WiFi stability

  if (!client.connected()) {
    mqttConnect();
  }

  // TEMPERATURE
  int temperaturSense = 0;
  if (!tempetureDummy) {
    sensors.requestTemperatures(); // Send the command to get temperatures
    temperaturSense = sensors.getTempCByIndex(0);
  } else {
    temperaturSense = random(16, 37);
  }


  // MOTION
  byte motion = motionDummy ? random(2) : digitalRead(pinMotion);
  if (motion) {
    motionFlag = 1;
  }
  if (String(led_mon) == "on") {
    digitalWrite(pinLED, !motion);
  }

  // SENSE surrounding roughly every interval
  unsigned long intervalSense = ((iSensMenit * 60) + iSensDetik) * 1000;
  if ((lastSensMillis == 0) || (millis() - lastSensMillis > intervalSense)) {
    lastSensMillis = millis();

    bufferTemp = bufferTemp + temperaturSense;
    bufferTempCount++;
    Serial.println("Temp: " + String(temperaturSense) + " Buffer: " + String(bufferTemp) + " total data : " + String(bufferTempCount));

    bufferMotion = bufferMotion + motionFlag;
    motionFlag = 0;
    Serial.println("Motion: " + String(motion) + " Buffer: " + String(bufferMotion));

  }

  // PUBLISH a message roughly every interval
  unsigned long intervalPush = ((iPushMenit * 60) + iPushDetik) * 1000;
  if ((lastPushMillis == 0) || ((millis() - lastPushMillis + 500) > intervalPush)) {
    lastPushMillis = millis() + 500;

    // motion detector sensor data
    if (motionEnable) {
      String deviceType1 = "motion";
      String topic1 = topic + deviceType1 + "/";
      String payload1 = String(bufferMotion);
      client.publish(topic1, payload1);
      Serial.println("outgoing: " + topic1 + " - " + payload1);
      bufferMotion = 0;
    }

    // temperature sensor data
    if (tempetureEnable) {
      String deviceType2 = "temp";
      String topic2 = topic + deviceType2 + "/";
      String payload2 = String(bufferTemp / bufferTempCount);
      client.publish(topic2, payload2);
      Serial.println("outgoing: " + topic2 + " - " + payload2);
      bufferTemp = 0;
      bufferTempCount = 0;
    }
  }

  // RESET button
  byte buttonPressed = !digitalRead(pinButton);
  if (buttonPressed && ((millis() - lastButtonLow) > 5000)) {
    joggingLed();
    WiFi.disconnect();    // disconnect & reset
    delay(2000);
    ESP.reset();
    delay(5000);
  }
  if (!buttonPressed) {
    lastButtonLow = millis();
  }

}
