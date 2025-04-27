#include "secrets.h"
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "WiFi.h"
 
#include "DHT.h"
#define DHTPIN 12     // Digital pin connected to the DHT sensor
#define DHTTYPE DHT22   // DHT 22

#define MQ135PIN 34   // Analog pin connected to the MQ135 sensor
 
// RGB LED pins
#define RED_PIN 27   
#define GREEN_PIN 26
#define BLUE_PIN 25

#define AWS_IOT_PUBLISH_TOPIC   "esp32/pub"
#define AWS_IOT_SUBSCRIBE_TOPIC "esp32/sub"
 
float h;
float t;
float airQuality;  // Variable to store MQ135 sensor reading
 
DHT dht(DHTPIN, DHTTYPE);
 
WiFiClientSecure net = WiFiClientSecure();
PubSubClient client(net);

// Timing variables
unsigned long lastPublishTime = 0;
const unsigned long PUBLISH_INTERVAL = 2000;  // Publish every 2 seconds
const unsigned long LOOP_INTERVAL = 50;       // Check for messages every 50ms

// Function to set RGB LED color
void setLEDColor(int red, int green, int blue) {
  analogWrite(RED_PIN, red);
  analogWrite(GREEN_PIN, green);
  analogWrite(BLUE_PIN, blue);
}

void messageHandler(char* topic, byte* payload, unsigned int length)
{
  Serial.print("incoming: ");
  Serial.println(topic);
 
  StaticJsonDocument<200> doc;
  deserializeJson(doc, payload);
  
  // Check if message contains LED command
  if (doc.containsKey("led")) {
    int red = doc["led"]["red"];
    int green = doc["led"]["green"];
    int blue = doc["led"]["blue"];
    
    Serial.printf("Setting LED - R:%d G:%d B:%d\n", red, green, blue);
    setLEDColor(red, green, blue);
  } else {
    const char* message = doc["message"];
    Serial.println(message);
  }
}
 
void connectAWS()
{
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
 
  Serial.println("Connecting to Wi-Fi");
 
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
 
  // Configure WiFiClientSecure to use the AWS IoT device credentials
  net.setCACert(AWS_CERT_CA);
  net.setCertificate(AWS_CERT_CRT);
  net.setPrivateKey(AWS_CERT_PRIVATE);
 
  // Connect to the MQTT broker on the AWS endpoint we defined earlier
  client.setServer(AWS_IOT_ENDPOINT, 8883);
 
  // Create a message handler
  client.setCallback(messageHandler);
 
  Serial.println("Connecting to AWS IOT");
 
  while (!client.connect(THINGNAME))
  {
    Serial.print(".");
    delay(100);
  }
 
  if (!client.connected())
  {
    Serial.println("AWS IoT Timeout!");
    return;
  }
 
  // Subscribe to a topic
  client.subscribe(AWS_IOT_SUBSCRIBE_TOPIC);
 
  Serial.println("AWS IoT Connected!");
}
 
void publishMessage()
{
  StaticJsonDocument<200> doc;
  doc["humidity"] = h;
  doc["temperature"] = t;
  doc["airQuality"] = airQuality;  // Add air quality data to JSON
  char jsonBuffer[512];
  serializeJson(doc, jsonBuffer); // print to client
 
  client.publish(AWS_IOT_PUBLISH_TOPIC, jsonBuffer);
}

float readMQ135()
{ 
  // Take average
  int sum = 0;
  for (int i = 0; i < 10; i++) {
    sum += analogRead(MQ135PIN);
    delay(10);
  }
  int sensorValue = sum / 10;
  
  return sensorValue;
}

void readSensors()
{
  h = dht.readHumidity();
  t = dht.readTemperature();
  airQuality = readMQ135();
 
  if (isnan(h) || isnan(t) || h > 100.0 || t < -40.0 || t > 80.0)  // Check if any reads failed and exit early (to try again).
  {
    Serial.println(F("Failed to read from DHT sensor!"));
    return;
  }
 
  Serial.print(F("Humidity: "));
  Serial.print(h, 1);
  Serial.print(F("%  Temperature: "));
  Serial.print(t, 1);
  Serial.print(F("°C  Air Quality: "));
  Serial.println(airQuality);
}

void setup()
{
  Serial.begin(115200);
  
  // Initialize RGB LED pins
  pinMode(RED_PIN, OUTPUT);
  pinMode(GREEN_PIN, OUTPUT);
  pinMode(BLUE_PIN, OUTPUT);
  
  connectAWS();
  dht.begin();
  pinMode(MQ135PIN, INPUT);  // Set MQ135 pin as input
  
  // Initialize timing
  lastPublishTime = millis();
}
 
void loop()
{
  // Ensure we're still connected to AWS IoT
  if (!client.connected()) {
    Serial.println("AWS IoT disconnected. Reconnecting...");
    connectAWS();
  }
  
  // Call client.loop() frequently to process incoming messages quickly
  // and chnage the LED color with almost no delay
  client.loop();
  
  // Publish sensor data
  unsigned long currentTime = millis();
  if (currentTime - lastPublishTime >= PUBLISH_INTERVAL) {
    readSensors();
    publishMessage();
    lastPublishTime = currentTime;
  }
  
  // Small delay to prevent CPU overload
  delay(LOOP_INTERVAL);
}