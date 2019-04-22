/*
  Hostel-IoT

  Send sensor data to from hostel room to server with MQTT

  14 April 2019   v1.0
  by Febrianto AN

*/
#include <ESP8266WiFi.h>
#include <MQTT.h>

/* Edit configurasi dibawah ini */
// wifi
char const * ssid = "XLGO-B59B";
char const * pass = "0987612345";
// topic
String tClient   = "yudhi";
String tRegion   = "jogjakarta";
String tLocation = "yudhiguesthouse";
String tRoom     = "kamar13";
// next to do: input config from web, save to eeprom 

/*device id */
char const * deviceID = "ESP-fbr-120419-1";

/* host */
char const * hostAddr = "13.250.113.167";
char const * userHost = "ndoware";
char const * passHost = "ndoware";


WiFiClient net;
MQTTClient client;

unsigned long lastMillis = 0;

String topic = tClient + "/" + tRegion + "/" + tLocation + "/" + tRoom + "/";

const int pinSensor = D1;



void connect() {
  Serial.print("checking wifi...");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(1000);
  }

  Serial.print("\nconnecting...");
  while (!client.connect(deviceID, userHost, passHost)) {
    Serial.print(".");
    delay(1000);
  }

  Serial.println("\nconnected!");

  // client.subscribe(topic);
  // client.unsubscribe("/hello");
}

void messageReceived(String &rTopic, String &rPayload) {
  Serial.println("incoming: " + rTopic + " - " + rPayload);
}


void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, pass);
  
  pinMode(pinSensor, INPUT);
  pinMode(LED_BUILTIN, OUTPUT);

  // Note: Local domain names (e.g. "Computer.local" on OSX) are not supported by Arduino.
  // You need to set the IP address directly.
  client.begin(hostAddr, net);
  client.onMessage(messageReceived);

  connect();
}


void loop() {
  client.loop();
  delay(10);  // <- fixes some issues with WiFi stability

  if (!client.connected()) {
    connect();
  }

  byte sense = digitalRead(pinSensor);
  if (sense) {
    digitalWrite(LED_BUILTIN, LOW);
  } else {
    digitalWrite(LED_BUILTIN, HIGH);
  }

  // publish a message roughly every second.
  if (millis() - lastMillis > 1000) {
    lastMillis = millis();
    String payload = "sense";
    client.publish(topic, payload);
    Serial.println("outgoing: " + topic + " - " + payload);
  }
  if (millis() - lastMillis > 5) {
  }
   
}
