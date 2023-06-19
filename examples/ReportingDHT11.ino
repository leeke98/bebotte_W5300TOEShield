#include <SPI.h>
#include <Ethernet.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <DHT.h>

#define DHTPIN            13         // Pin which is connected to the DHT sensor.
#define DHTTYPE           DHT11     // DHT 11

#define TOKEN "token_xxxxxxxx"    // Set your channel token here

#define CHANNEL "W5300TOE"          // Replace with your device name
#define TEMP_RESOURCE "temperature" // This is where we will store temperature
#define HUMID_RESOURCE "humidity"   // This is where we will store humidity

#define WRITE true // indicate if published data should be persisted or not

// Initialize DHT sensor.
DHT dht(DHTPIN, DHTTYPE);//

// Enter a MAC address of your shield
// It might be printed on a sticker on the shield
byte g_target_ip[] = { 54, 221, 205, 80 };
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
IPAddress ip(192, 168, 0, 0);
IPAddress myDns(8, 8, 8, 8);
IPAddress gateway(192, 168, 0, 0);
IPAddress subnet(0, 0, 0, 0);

EthernetClient ethClient;
PubSubClient client(ethClient);

// will store last time DHT sensor data was read
unsigned long lastReadingMillis = 0;

// interval for sending DHT temp and humidity to Beebotte
const long interval = 10000;
// to track delay since last reconnection
unsigned long lastReconnectAttempt = 0;

const char chars[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890";

char id[17]; // Identifier for the MQTT connection - will set it randomly

const char * generateID()
{
  randomSeed(analogRead(0));
  int i = 0;
  for(i = 0; i < sizeof(id) - 1; i++) {
    id[i] = chars[random(sizeof(chars))];
  }
  id[sizeof(id) -1] = '\0';

  return id;
}

// publishes data to the specified resource
void publish(const char* resource, float data, bool persist)
{
  StaticJsonDocument<128> jsonOutBuffer;
  JsonObject root = jsonOutBuffer.createNestedObject();
  root["channel"] = CHANNEL;
  root["resource"] = resource;
  if (persist) {
    root["write"] = true;
  }
  root["data"] = data;

  // Now print the JSON into a char buffer
  char buffer[128];
  serializeJson(root, buffer, sizeof(buffer));

  // Create the topic to publish to
  char topic[64];
  sprintf(topic, "%s/%s", CHANNEL, resource);

  // Now publish the char buffer to Beebotte
  client.publish(topic, buffer);
}

void readSensorData()
{
  Serial.println("readSensorData....");

  // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
  float h = dht.readHumidity();
  // Read temperature as Celsius (the default)
  float t = dht.readTemperature();

  String message = "The temperature is " + String(t) + " degrees Celsius.";
  Serial.println(message);


  // Check if any reads failed and exit early (to try again).
  if (isnan(h) || isnan(t)) {
    Serial.println(F("Failed to read from DHT sensor!"));
    return;
  }

  if (!isnan(h) || !isnan(t)) {
    publish(TEMP_RESOURCE, t, WRITE);
    publish(HUMID_RESOURCE, h, WRITE);
  }
}

// reconnects to Beebotte MQTT server
boolean reconnect() {
  if (client.connect(generateID(), TOKEN, "")) {
    Serial.println("Connected to Beebotte MQTT");
  }
  return client.connected();
}

void setup()
{
  // Open serial communications and wait for port to open:
  Serial3.setRx(PC11);
  Serial3.setTx(PC10);  
  delay(50);
  
  Serial3.begin(9600);
  dht.begin();
  
  while (!Serial3) {
    ;  // wait for serial port to connect. Needed for native USB port only
  }

  client.setServer(g_target_ip, 1883);

  // start the Ethernet connection:
  if (Ethernet.begin(mac) == 0) {
    Serial3.println("Failed to configure Ethernet using DHCP");
    Ethernet.begin(mac, ip, myDns, gateway, subnet);
  }

  // give the Ethernet shield a second to initialize:
  delay(1000);

  Serial3.println("connecting...");
  lastReconnectAttempt = 0;
}

void loop()
{
  if (!client.connected()) {
    unsigned long now = millis();
    if (now - lastReconnectAttempt > 5000) {
      lastReconnectAttempt = now;
      // Attempt to reconnect
      if (reconnect()) {
        lastReconnectAttempt = 0;
      }
    }
  } else {
    // Client connected

    // read sensor data every 10 seconds
    // and publish values to Beebotte
    unsigned long currentMillis = millis();
    if (currentMillis - lastReadingMillis >= interval) {
      // save the last time we read the sensor data
      lastReadingMillis = currentMillis;

      readSensorData();
    }
    client.loop();
  }
}