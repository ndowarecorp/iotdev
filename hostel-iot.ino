/*
  Hostel-IoT

  Send sensor data to from hostel room to server with MQTT
  14 April 2019   v1.0
  by Febrianto AN

  todo:
  - input config wifi & topic from client, save to eeprrom
  - subscribe config from server

*/
#include <ESP8266WiFi.h>
#include <MQTT.h>

/*            Edit configurasi dibawah ini                */
/* ------------------------------------------------------ */

/* DEVICE ID */
char const * deviceID = "ESP-fbr-120419-1";

// wifi
char const * ssid = "SSIDNAME";
char const * pass = "SSIDPASS";
// topic
String tClient   = "pemda";
String tRegion   = "yogyakarta";
String tLocation = "ishiro";
String tRoom     = "kamar13";

/* host */
char const * hostAddr = "xx.xx.xx.xx";
char const * userHost = "user";
char const * passHost = "password";

/* sersors */
boolean motionEnable = true;
boolean tempetureEnable = true;

/* interval sending data */
int iMenit = 10;   //minute
int iDetik = 0;   //s

/* interval heartbeat */
int iHB = 5000;   //ms // 0 = nothing

boolean debug = true;

/* ------------------------------------------------------ */
WiFiClient net;
MQTTClient client;

unsigned long lastMillis = 0;
unsigned long lastBeatMillis = 0;

String topic = "hostel-iot/" + tClient + "/" + tRegion + "/" + tLocation + "/" + tRoom + "/";

//const int pinSensor = D1;

void connect() {
  Serial.print("Connecting...");
  while (!client.connect(deviceID, userHost, passHost)) {
    Serial.print(".");
    digitalWrite(LED_BUILTIN, LOW);
    delay(500);
    digitalWrite(LED_BUILTIN, HIGH);
    delay(500);
  }

  Serial.println("\nconnected!");

  // client.subscribe(topic);
  // client.unsubscribe("topic");
}

void messageReceived(String &rTopic, String &rPayload) {
  Serial.println("incoming: " + rTopic + " - " + rPayload);
}

/* ------------------------------------------------------ */
void setup() {
  if (debug) {
    Serial.begin(9600);
  }
  WiFi.begin(ssid, pass);

  //pinMode(pinSensor, INPUT);
  pinMode(LED_BUILTIN, OUTPUT);

  client.begin(hostAddr, net);
  client.onMessage(messageReceived);

  connect();
}

/* ------------------------------------------------------ */
void loop() {
  client.loop();
  delay(10);  // <- fixes some issues with WiFi stability

  if (!client.connected()) {
    connect();
  }

  // publish a message roughly every interval
  unsigned long interval = ((iMenit * 60) + iDetik) * 1000;
  if ((lastMillis == 0) || (millis() - lastMillis > interval)) {
    lastMillis = millis();

    // motion detector sensor data
    if (motionEnable) {
      String deviceType1 = "motion";
      String topic1 = topic + deviceType1 + "/";
      String payload1 = String(random(2));
      client.publish(topic1, payload1);
      Serial.println("outgoing: " + topic1 + " - " + payload1);

    }

    // temperature sensor data
    if (tempetureEnable) {
      String deviceType2 = "temp";
      String topic2 = topic + deviceType2 + "/";
      String payload2 = String(random(16, 37));
      client.publish(topic2, payload2);
      Serial.println("outgoing: " + topic2 + " - " + payload2);
    }
  }

  // heartbeat
  if (iHB > 0) {
    if (millis() - lastBeatMillis > iHB) {
      lastBeatMillis = millis();
      digitalWrite(LED_BUILTIN, LOW);
    }
    if (millis() - lastBeatMillis > 50) {
      digitalWrite(LED_BUILTIN, HIGH);
    }
  }

}
