#include <PubSubClient.h>
#include <WiFi.h>
#include "DHT.h"

#define DHTTYPE DHT11 // Define the type of DHT sensor

const char* WIFI_SSID = "jeremylau"; // Your WiFi SSID
const char* WIFI_PASSWORD = "13131555"; // Your WiFi password
const char* MQTT_SERVER = "34.133.60"; // Your VM instance public IP address
const char* MQTT_TOPIC = "iot"; // MQTT topic for subscription
const int MQTT_PORT = 8883; // TLS communication port
const int dhtPin = 4;        // DHT11 pin
const int irPin = 42;        // IR sensor pin
const int buzzerPin = 6;     // Buzzer pin
const int relayPin = 39;     // Relay pin for fan
const int ventilatorPin = 3; // Ventilator motor pin
const int lightBulbPin = 5;  // Light bulb pin
const int rainAnalogPin = A1; // Analog pin for rain sensor
const int rainDigitalPin = 8; // Digital pin for rain sensor
const int moisturePin = A2;  // Analog pin for soil moisture sensor

DHT dht(dhtPin, DHTTYPE);

WiFiClient espClient;
PubSubClient client(espClient);

// Buffer for sensor data
char buffer[128] = ""; 

// Last message time
unsigned long lastMsgTime = 0;
#define INTERVAL 5000 // 5 seconds

bool irDetected = false;       // Tracks if IR sensor detects an object

void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void setup() {
  Serial.begin(115200); // Initiate serial communication
  dht.begin();          // Initialize DHT sensor
  setup_wifi();         // Connect to the WiFi network
  client.setServer(MQTT_SERVER, MQTT_PORT); // Set up the MQTT client

  pinMode(irPin, INPUT_PULLUP);
  pinMode(buzzerPin, OUTPUT);
  pinMode(relayPin, OUTPUT);
  pinMode(ventilatorPin, OUTPUT);
  pinMode(lightBulbPin, OUTPUT);
  pinMode(rainDigitalPin, INPUT);
  pinMode(moisturePin, INPUT);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  unsigned long cur = millis();

  if (cur - lastMsgTime > INTERVAL) {
    lastMsgTime = cur;

    detectIR();  // Detect foreign objects using IR sensor
    readMoisture();  // Read soil moisture
    readDHT();  // Read DHT sensor for temperature and humidity
    readRain(); // Read rain sensor (analog and digital)
  }
}

void detectIR() {
  irDetected = !digitalRead(irPin);
  if (irDetected) {
    Serial.println("IR sensor detected an object!");
    activateBuzzerAndLight(); // Activate buzzer and light bulb on detection
    delay(1000);              // Cooldown to prevent repeated detection
  }
}

void readMoisture() {
  int sensorValue = analogRead(moisturePin);
  int moisture = map(sensorValue, 0, 1023, 0, 100);  // Assuming 0 is dry, 100 is wet
  sprintf(buffer, "Soil Moisture: %d%%", moisture);
  client.publish(MQTT_TOPIC, buffer);  // Publish soil moisture data
  Serial.println(buffer);
}

void readRain() {
  int rainAnalogValue = analogRead(rainAnalogPin);
  int rainDigitalValue = digitalRead(rainDigitalPin);
  
  sprintf(buffer, "Rain Analog Value: %d", rainAnalogValue);
  client.publish(MQTT_TOPIC, buffer); // Publish analog rain data
  
  sprintf(buffer, "Rain Digital State: %d", rainDigitalValue == LOW ? 1 : 0);
  client.publish(MQTT_TOPIC, buffer); // Publish digital rain state (0: No rain, 1: Raining)
  
  Serial.print("Rain Analog Value: ");
  Serial.println(rainAnalogValue);
  Serial.print("Rain Digital State: ");
  Serial.println(rainDigitalValue == LOW ? "Raining" : "Not Raining");
}

void readDHT() {
  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();

  if (!isnan(temperature) && !isnan(humidity)) {
    Serial.print("Temperature: ");
    Serial.print(temperature);
    Serial.println("°C");

    Serial.print("Humidity: ");
    Serial.print(humidity);
    Serial.println("%");
    
    // Publish temperature and humidity
    sprintf(buffer, "Temperature: %.2f°C, Humidity: %.2f%%", temperature, humidity);
    client.publish(MQTT_TOPIC, buffer);

    // Activate relay (water motor) if humidity < 70%
    if (humidity < 70.0) {
      Serial.println("Humidity below 70%. Activating relay...");
      activateRelay();
    } else {
      Serial.println("Humidity above 70%. Relay remains off.");
    }

    // Warning to check for the ventilation system >28°C
    if (temperature > 28.0) {
      Serial.println("Temperature is above 28°C. Ventilator is initiated.");
      activateVentilator();
    } else {
      Serial.println("Temperature below 28°C. Ventilator is switched off.");
      digitalWrite(ventilatorPin, LOW); // Explicitly turn off the motor
    }
  } else {
    Serial.println("Failed to read from DHT sensor.");
  }
}

void activateBuzzerAndLight() {
  for (int i = 0; i < 2; i++) {
    digitalWrite(buzzerPin, HIGH); // Turn buzzer ON
    digitalWrite(lightBulbPin, HIGH); // Turn light bulb ON
    delay(500);
    digitalWrite(buzzerPin, LOW);  // Turn buzzer OFF
    digitalWrite(lightBulbPin, LOW); // Turn light bulb OFF
    delay(500);
  }
}

void activateRelay() {
  for (int i = 0; i < 2; i++) { // Repeat twice 
    digitalWrite(relayPin, HIGH); // Turn relay ON
    delay(500);
    digitalWrite(relayPin, LOW);  // Turn relay OFF
    delay(500);
  }
  delay(1000); // Total delay between repeated activations
}

void activateVentilator() {
  for (int i = 0; i < 2; i++) { // Repeat twice for demonstration
    digitalWrite(ventilatorPin, HIGH); // Turn ventilator motor ON
    delay(500);
    digitalWrite(ventilatorPin, LOW);  // Turn ventilator motor OFF
    delay(500);
  }
  delay(1000); // Total delay between repeated activations
}

void reconnect() {
  while (!client.connected()) {
    Serial.println("Attempting MQTT connection...");
    if (client.connect("ESP32Client")) {
      Serial.println("Connected to MQTT server");
    } else {
      Serial.print("Failed, rc=");
      Serial.print(client.state());
      Serial.println(" Retrying in 5 seconds...");
      delay(5000);
    }
  }
}



