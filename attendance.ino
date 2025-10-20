#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#include <ThingSpeak.h>
#include <Wire.h>
#include <Adafruit_MLX90614.h>
#include <SPI.h>
#include <MFRC522.h>

// ==== RFID Pins ====
#define SS_PIN D4
#define RST_PIN D0

MFRC522 rfid(SS_PIN, RST_PIN);

// ==== Ultrasonic Pins ====
#define TRIG_PIN D3
#define ECHO_PIN D8

// ==== MLX90614 Sensor ====
Adafruit_MLX90614 mlx = Adafruit_MLX90614();

// ==== WiFi & ThingSpeak ====
const char* ssid = "abcg_4g";
const char* password = "12345678";
unsigned long myChannelNumber = 3115672;
const char* myWriteAPIKey = "J36UX92LN50PR4S1";

WiFiClient client;

// ==== Function to match UID to Name ====
String getName(String uid) {
  if (uid == "2925AB00") return "Alice";
  if (uid == "3B10AB00") return "Bob";
  return "Unknown";
}

void setup() {
  Serial.begin(115200);
  SPI.begin();
  rfid.PCD_Init();
  mlx.begin();

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  Serial.println("Connecting to WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Connected!");
  ThingSpeak.begin(client);

  Serial.println("Place your RFID card near the reader...");
}

// ==== Send data to IFTTT ====
void sendToIFTTT(String uid, double temp, String status) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String url = "https://maker.ifttt.com/trigger/Attendance_update/json/with/key/nopaIupHSnt7uehhtdN7Lzs5zNpVPkpf2rU_cF0SdPX";
    url += "?value1=" + uid;
    url += "&value2=" + String(temp, 2);
    url += "&value3=" + status;

    http.begin(client, url);
    int httpCode = http.GET();
    if (httpCode > 0) {
      Serial.println("✅ Data sent to Google Sheets via IFTTT!");
    } else {
      Serial.println("❌ Error sending to IFTTT: " + String(httpCode));
    }
    http.end();
  } else {
    Serial.println("WiFi is not connected");
  }
}

void loop() {
  // Step 1: Detect person using ultrasonic
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH);
  int distance = duration * 0.034 / 2; // cm

  if (distance > 0 && distance < 15) { // person detected
    Serial.println("Person Detected!");

    // Step 2: Wait for RFID scan
    if (!rfid.PICC_IsNewCardPresent()) return;
    if (!rfid.PICC_ReadCardSerial()) return;

    String uid = "";
    for (byte i = 0; i < rfid.uid.size; i++) {
      if (rfid.uid.uidByte[i] < 0x10) uid += "0"; // add leading zero
      uid += String(rfid.uid.uidByte[i], HEX);
    }
    uid.toUpperCase();

    Serial.print("RFID UID: ");
    Serial.println(uid);

    // Step 3: Read temperature
    double temp = mlx.readObjectTempC();
    Serial.print("Temperature: ");
    Serial.println(temp);

    // Step 4: Decide status
    String status = (temp < 37.5) ? "Normal" : "High Temperature";

    ThingSpeak.setField(1, uid);         // Field 1: UID
    ThingSpeak.setField(2, String(temp, 2));  // Field 2: Temperature
    ThingSpeak.setField(3, status);           // Field 3: Status

    int x = ThingSpeak.writeFields(myChannelNumber, myWriteAPIKey);

    if (x == 200) {
      Serial.println("✅ Data uploaded successfully to ThingSpeak!");
    } else {
      Serial.println("❌ Upload failed, code: " + String(x));
    }

    sendToIFTTT(uid, temp, status);

    delay(5000); // prevent multiple uploads
  }
}


