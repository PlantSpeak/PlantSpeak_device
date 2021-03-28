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
 // LIGHT SENSOR //
//////////////////
uint16_t get_light_reading(){
  return analogRead(PHOTORESISTOR_PIN);
}

// Converting analog signal from photoresistor to lux (unit for light intensity).
// The following function converts voltage reading on the analog pin
// (from the LDR) to lux. This was calibrated
// by comparing voltage readings to lux readings from a smartphone sensor.
float estimate_light_intensity(){
  return 8715.57985748*exp(-0.00489288*get_light_reading());
}

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

  ////////////////////////////
 // TEMPERATURE + HUMIDITY //
////////////////////////////
DHTesp dht11;
// Prepares the dht sensor for use.
void setup_temp_sensor(){
  dht11.setup(DHT11_PIN, DHTesp::DHT11);
}

float get_temperature(){
  return dht11.getTemperature();
}

float get_humidity(){
  return dht11.getHumidity();
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
  Serial.println(get_temperature());
  Serial.println(get_humidity());
  Serial.println(estimate_light_intensity());
  delay(2000);
} 
