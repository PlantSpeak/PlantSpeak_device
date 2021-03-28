#include <DHTesp.h>
#include <PubSubClient.h>
#include "time.h"
#include <WiFi.h>

// ENTER YOUR DETAILS HERE
#define WIFI_SSID "" //ENTER YOUR WIFI SSID HERE
#define WIFI_PASSWORD "" //ENTER YOUR WIFI PASSWORD HERE
#define MQTT_SERVER_ADDR "" //ENTER THE MQTT SERVER ADDRESS HERE

#define MQTT_PORT 1883 //DEFAULT MQTT PORT

// Define input/output pins.
#define PHOTORESISTOR_PIN 35
#define DHT11_PIN 4
#define SOIL_MOISTURE_SENSOR_PIN 34

const int DEVICE_ID = 0;
const char MQTT_TOPIC[] = "SmartPlant";

// Client objects for connecting to wifi and mqtt,
// maintaining relevant data.
WiFiClient wifiClient;
PubSubClient client(wifiClient);

  //////////////////
 // NETWORK/WIFI //
//////////////////
// Routines for connect to network, and the internet, via WiFi.
void connectToWifi(){
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(5000);
  }
}

void connectToMQTT(){
  client.setServer(MQTT_SERVER_ADDR, MQTT_PORT);
  String clientId = "Plant";
    clientId +=String(DEVICE_ID);
    while (!client.connect(clientId.c_str())) {
      delay(500);
    }
}

void connect(){
  connectToWifi();
  connectToMQTT();
}

// Startup code.
void setup() { 
  // Uncomment the following line for serial output for debugging purposes.
  Serial.begin(9600);

  // Setup pins.
  pinMode(PHOTORESISTOR_PIN, INPUT);
  pinMode(DHT11_PIN, INPUT);
  pinMode(SOIL_MOISTURE_SENSOR_PIN, INPUT);
  
  connect();
}

void loop() {
  
  if (!client.connected()) {
    connectToMQTT();
    client.publish(MQTT_TOPIC, "test", true);
  }
  delay(2000);
} 
