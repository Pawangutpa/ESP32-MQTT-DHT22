#include <Wire.h>
#include <PubSubClient.h>
#include <WiFi.h>

// MPU6050 I2C address
#define MPU6050_ADDR       0x68

// MPU6050 register map
#define WHO_AM_I_REG       0x75
#define PWR_MGMT_1_REG     0x6B
#define ACCEL_CONFIG_REG   0x1C
#define GYRO_CONFIG_REG    0x1B
#define ACCEL_XOUT_H       0x3B
#define TEMP_OUT_H         0x41

// LED pin (GPIO 2 is safe on ESP32)
#define LedPin 9

// WiFi and MQTT credentials
const char* ssid = "Wokwi-GUEST";
const char* password = "";
const char* mqtt_server = "broker.hivemq.com";

// MQTT client setup
WiFiClient espClient;
PubSubClient client(espClient);

int reset = 0;

// MPU6050 global variables
int16_t ax, ay, az, gx, gy, gz, tempRaw;
float accelX=0, accelY=0, accelZ=0, gyroX=0, gyroY=0, gyroZ=0, tempC=0;



// WiFi connection
void setup_wifi() {
  delay(10);
  Serial.print("Connecting  ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

// ----------------------------------------------------------------
// MQTT callback
void callback(char* topic, byte* payload, unsigned int length) {
  char msg = 0;
  char res = 0;
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("]: ");

  for (int i = 0; i < length; i++) {
    msg = (char)payload[i];
  }
  Serial.println(msg);

   if ('1' == msg) {
    digitalWrite(LedPin, HIGH);
    reset = 0;
  } else if ('2' == msg) {
    digitalWrite(LedPin, LOW);
    reset = 0;
  } else if ('3' == msg) {
    reset = 1;
  } else {
    reset = 0;
  }

  Serial.print("Reset Status: ");
  Serial.println(reset);
}

// ----------------------------------------------------------------
// MQTT reconnect
void reconnect() {
  while (!client.connected()) {
    Serial.println("Attempting MQTT connection...");
    if (client.connect("ESPClient")) {
      Serial.println("Connected to MQTT broker");
      client.subscribe("/LedControl");
      client.subscribe("/Rest");
    } else {
      Serial.print("Failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

// ----------------------------------------------------------------
// Setup
void setup() {
  Serial.begin(115200);
  pinMode(LedPin, OUTPUT);
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

  // Initialize I2C
  Wire.begin(21, 22);

  // Check MPU6050 connection
  uint8_t whoami = readRegister(WHO_AM_I_REG);
  if (whoami != 0x68) {
    Serial.print("MPU6050 not detected! WHO_AM_I = 0x");
    Serial.println(whoami, HEX);
    while (1);
  }
  Serial.println("MPU6050 detected!");

  // Wake up and configure MPU6050
  writeRegister(PWR_MGMT_1_REG, 0x00);
  delay(100);
  writeRegister(ACCEL_CONFIG_REG, 0x00);
  writeRegister(GYRO_CONFIG_REG, 0x00);
  Serial.println("MPU6050 initialization complete.\n");
}

// ----------------------------------------------------------------
// Main loop
void loop() {
   if (!client.connected()) {
    reconnect();
  }
  reset = 0;
  client.loop();
  
  readRawData();
  
  Serial.print("Reset Status : ");
  Serial.println(reset);
  // Print
  Serial.println("==================================");
  Serial.printf("Accel (g): %.3f, %.3f, %.3f\n", accelX, accelY, accelZ);
  Serial.printf("Gyro (°/s): %.3f, %.3f, %.3f\n", gyroX, gyroY, gyroZ);
  Serial.printf("Temperature: %.2f °C\n", tempC);
  Serial.println("==================================\n");
  delay(2000);
}



// ----------------------------------------------------------------
// I2C helper functions
void writeRegister(uint8_t reg, uint8_t data) {
  Wire.beginTransmission(MPU6050_ADDR);
  Wire.write(reg);
  Wire.write(data);
  Wire.endTransmission();
}

uint8_t readRegister(uint8_t reg) {
  Wire.beginTransmission(MPU6050_ADDR);
  Wire.write(reg);
  Wire.endTransmission(false);
  Wire.requestFrom(MPU6050_ADDR, 1);
  return Wire.read();
}



void readRawData() {
  Wire.beginTransmission(MPU6050_ADDR);
  Wire.write(ACCEL_XOUT_H);
  Wire.endTransmission(false);
  Wire.requestFrom(MPU6050_ADDR, 14);

  ax = (Wire.read() << 8) | Wire.read();
  ay = (Wire.read() << 8) | Wire.read();
  az = (Wire.read() << 8) | Wire.read();
  tempRaw = (Wire.read() << 8) | Wire.read();
  gx = (Wire.read() << 8) | Wire.read();
  gy = (Wire.read() << 8) | Wire.read();
  gz = (Wire.read() << 8) | Wire.read();
  // Convert raw data
  accelX = ax / 16384.0;
  accelY = ay / 16384.0;
  accelZ = az / 16384.0;
  gyroX = gx / 131.0;
  gyroY = gy / 131.0;
  gyroZ = gz / 131.0;
  tempC = (tempRaw / 340.0) + 36.53;
  if(reset==1){
  accelX = 0;
  accelY = 0;
  accelZ = 0;
  gyroX= 0;
  gyroY= 0;
  gyroZ =0;
  tempC = 0;
  }

  String payload = "{";
  payload += "\"ax\":" + String(accelX, 2) + ",";
  payload += "\"ay\":" + String(accelY, 2) + ",";
  payload += "\"az\":" + String(accelZ, 2) + ",";
  payload += "\"gx\":" + String(gyroX, 2) + ",";
  payload += "\"gy\":" + String(gyroY, 2) + ",";
  payload += "\"gz\":" + String(gyroZ, 2) + ",";
  payload += "\"tempC\":" + String(tempC, 2);
  payload += "}";

  Serial.print("Publishing: ");
  Serial.println(payload);
  client.publish("home/sensor/mpu6050", payload.c_str());
}