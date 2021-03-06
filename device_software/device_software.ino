#include <EEPROM.h>
#include <DHTesp.h>
#include <PubSubClient.h>
#include "time.h"
#include <WiFi.h>

// NETWORK CONNECTION DETAILS
//// These must be filled in for program to compile and correctly for the device to function.
#define WIFI_SSID //ENTER YOUR WIFI SSID HERE
#define WIFI_PASSWORD //ENTER YOUR WIFI PASSWORD HERE
#define MQTT_SERVER_ADDR //ENTER THE MQTT SERVER ADDRESS HERE
#define MQTT_PORT 1883 //DEFAULT MQTT PORT 

#define READING_DELAY 15000
#define MQTT_CONNECTION_RETRY_DELAY 500
#define WIFI_RECONNECT_DELAY 5000

// Define input/output pins.
#define PHOTORESISTOR_PIN 35
#define DHT11_PIN 4
#define SOIL_MOISTURE_SENSOR_PIN 34

const int DEVICE_ID = 0;
const char MQTT_TOPIC[] = "SmartPlant";
const char MQTT_TOPIC_Pairing[] = "SmartPlant_pairing";

bool connection_established = false;

WiFiClient wifiClient;
PubSubClient client(wifiClient);

  //////////////////
 // NETWORK/WIFI //
//////////////////
// Routines for connect to network, and the internet, via WiFi.
void connectToWifi(){
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(WIFI_RECONNECT_DELAY);
  }
}

void connectToMQTT(){
  client.setServer(MQTT_SERVER_ADDR, MQTT_PORT);
  String clientId = "Plant";
    clientId +=String(DEVICE_ID);
    while (!client.connect(clientId.c_str())) {
      delay(MQTT_CONNECTION_RETRY_DELAY);
    }
}

void connect(){
  connectToWifi();
  connectToMQTT();
}

  //////////
 // TIME //
//////////
// Get current time from ntp server (internet).
// Get current GMT.
void timeserver_connect(){
  const char* ntpServer = "pool.ntp.org";
  configTime(0, 0, ntpServer);
}

struct tm getTime(){
  struct tm current_time;
  getLocalTime(&current_time);
  return current_time;
}

  ///////////////////
 // SOIL MOISTURE //
///////////////////
// Soil Moisture Sensor - Calibration
//  ESP32 ADCs have a maximum resolution of 12 bits... so when calibrating
//  the sensors expect a reading in the range 0-4096. Also note that sensor 
//  sensitivity will vary from module to module.
const int SOIL_MOISTURE_MIN = 1631; // Min reading when completely submerged in water.
const int SOIL_MOISTURE_MAX = 2851; // Max reading in dry air.
const int SOIL_MOISTURE_RANGE = SOIL_MOISTURE_MAX - SOIL_MOISTURE_MIN;


// Estimating the volumetric soil moisture. This section uses estimates for the low-cost
// V1.2 capacitive soil moisture sensor which does not have a datasheet readily available.
// We are using similar methodologies to estimate of soil moisture found by Joshua Hrisko. His methodology is
// described in the excellent article linked below.
// Hrisko, J. (2020). Capacitive Soil Moisture Sensor Calibration with Arduino. Maker Portal.
// https://makersportal.com/blog/2020/5/26/capacitive-soil-moisture-calibration-with-arduino
//float slope = 2.48;

float maxVoltage = 2.33; // in dry air
float minVoltage = 1.20; //submerged in water
float slope = 2.48;
//float intercept = -0.72;
float intercept = -1.07181;
//float intercept = -0.92;
float adcMin = 0.15;
float adcMax = 3.15;
float estimate_volumetric_soil_moisture(uint16_t reading){
  float vin = 3.3;
  //float vin = adcMax-adcMin;
  int adcResolution = 12;
  float voltage = (float(reading)/pow(2,adcResolution))*vin;
  return ((1.0/voltage)*slope)+intercept;
}

enum MOISTURE_LEVELS{
  DRY=0,
  MOIST,
  WET
};

// Estimate soil moisture according to index on
// store-bought soil moisture.
int estimate_moisture_index(float vol_soil_moisture){
  int index = floor(0.7034*exp(3.2585*vol_soil_moisture));
  if(index<0)
    index = 0;
  else if(index > 10)
    index = 10;
  return index;
}

// Will estimate the relative moisture content of the soil.
uint8_t calc_soil_moisture(uint16_t moisture){
  if(moisture<SOIL_MOISTURE_MIN){
    return 0;
  } else if (moisture>SOIL_MOISTURE_MAX){
    return 100;
  } else {
    return 100-float(moisture-SOIL_MOISTURE_MIN)/SOIL_MOISTURE_RANGE*100;
  }
}
//2.6 max
//1.78 min
float get_soil_moisture(){
  uint16_t reading = analogRead(SOIL_MOISTURE_SENSOR_PIN);
  return estimate_volumetric_soil_moisture(reading);
  //return calc_soil_moisture(reading);
}


int estimate_relative_moisture(float vol_soil_moisture){
  int index = estimate_moisture_index(vol_soil_moisture);
  switch(index){
    case 0 ... 2:
      return DRY;
    case 3 ... 7:
      return MOIST;
    case 8 ... 10:
      return WET;
  }
}

/*
float estimate_relative_moisture(float vol_soil_moisture){
  if(vol_soil_moisture<0.425)
    return DRY;
  else if(vol_soil_moisture>=0.425 && vol_soil_moisture<0.7)
    return DAMP;
  else
    return WET;
}
*/

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

  //////////////////
 // LIGHT SENSOR //
//////////////////
uint16_t get_light_reading(){
  return analogRead(PHOTORESISTOR_PIN);
}

// Converting analog signal from photoresistor to lux (unit for light intensity).
float estimate_light_intensity(){
  return 8715.57985748*exp(-0.00489288*get_light_reading());
}

// Startup code.
void setup() { 
  // Setup pins.
  pinMode(2, INPUT);
  pinMode(4, INPUT);
  pinMode(15, INPUT);
//
  // Setup sensors
  setup_temp_sensor();
  
  connect();
  timeserver_connect();
  getTime();

  if(EEPROM.read(0)==1)
    connection_established = true;
}

// Data structure to contain the data taken from the sensors on the device.
struct Reading{
  float temp;
  float humidity;
  float soil_moisture;
  int moisture_index;
  String moisture_level;
  time_t timestamp;
  float light_intensity;
  char mac_address[20];
};

// Encodes reading data in JSON for sending over MQTT.
String generateMessage(struct Reading reading){
  char json_str[256];
  sprintf(json_str, "{\n \"id\":%d,\n \"temp\":%f,\n \"humidity\":%f,\n \"soil_moisture\":%f,\n  \"moisture_index\":%d,\n  \"moisture_level\":\"%s\",\n \"light_intensity\":%f,\n  \"time\":%ld,\n \"mac_address\":\"%s\"\n}", DEVICE_ID, reading.temp, reading.humidity, reading.soil_moisture, reading.moisture_index, reading.moisture_level.c_str(), reading.light_intensity, reading.timestamp, WiFi.macAddress().c_str());
  return String(json_str);
}

// Gives a description of the soil moisture based on volumetric soil moisuture %.
String describe_soil_moisture(int soil_moisture_level){
  switch(soil_moisture_level){
    case DRY:
      return "Dry";
    case MOIST:
      return "Moist";
    case WET:
      return "Wet";
    default:
      return "Unknown";
  }
}

// Get readings from sensors.
struct Reading takeReading(){
  struct Reading reading;
  reading.temp = get_temperature();
  reading.humidity = get_humidity();
  float soil_moisture = get_soil_moisture();
  reading.soil_moisture = soil_moisture;//estimate_volumetric_soil_moisture(soil_moisture);
  reading.moisture_index = estimate_moisture_index(soil_moisture);
  reading.moisture_level = describe_soil_moisture(estimate_relative_moisture(soil_moisture));
  reading.light_intensity = estimate_light_intensity();
  reading.timestamp = time(NULL);
  return reading;
}

// Record the message received on the pairing channel (confirms if connection successful).
void mqtt_callback(char* topic, byte* payload, unsigned int length){
  char str_received[length+1];
  for(int i=0; i<length; i++){
    str_received[i] = (char)payload[i];
  }
  str_received[length] = '\0';
  if(strcmp(str_received,WiFi.macAddress().c_str())==0)
    connection_established = true;
    EEPROM.write(0, 1);
}

void loop() {  
  struct Reading reading = takeReading();
  
  if (!client.connected()) {
    connectToMQTT();
  }
  client.publish(MQTT_TOPIC_Pairing, WiFi.macAddress().c_str(), true);
  client.publish(MQTT_TOPIC, generateMessage(reading).c_str(), true);
  // Delay between readings.
  delay(READING_DELAY);

} 
