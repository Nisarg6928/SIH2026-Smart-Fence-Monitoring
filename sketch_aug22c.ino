#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

// --- Wi-Fi & Telegram Credentials ---
const char* WIFI_SSID     = "vivo V60";
const char* WIFI_PASSWORD = "1234ABCD";
const String BOT_TOKEN    = "8627923065:AAEM5GDCUZjoq3yDS3clKCeJoVXlQNm-tkg";
const String CHAT_ID      = "8655231156";

LiquidCrystal_I2C lcd(0x27, 16, 2);

const int SENSOR_PIN       = 34; // GPIO 34 Probe Wire (ADC)
const int RELAY_PIN        = 18; // GPIO 18 Relay IN1 Trigger
const int BUZZER_PIN       = 19; // GPIO 19 Piezo Buzzer
const int RED_LED          = 26; // GPIO 26 Red LED
const int GREEN_LED        = 16; // GPIO 16 Green LED

unsigned long highStartTime = 0;
unsigned long tripStartTime = 0;
unsigned long lastLcdUpdate = 0;
bool isHigh = false;
bool isTripped = false;
bool legalNotificationSent = false;

const unsigned long THRESHOLD_TIME  = 200;  // 0.2s contact trigger
const unsigned long AUTO_RESET_TIME = 5000; // Auto-resets 5s after overvoltage is removed

// Function to fetch approximate GPS coordinates via IP Geolocation
String getIPLocationLink() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin("http://ip-api.com/csv/?fields=lat,lon");
    http.setTimeout(5000);
    
    int httpCode = http.GET();
    if (httpCode == HTTP_CODE_OK) {
      String payload = http.getString();
      payload.trim(); 
      http.end();
      if (payload.length() > 0) {
        return "https://maps.google.com/?q=" + payload;
      }
    }
    http.end();
  }
  return "Location Unavailable";
}

void sendPhoneNotification(String message) {
  if (WiFi.status() == WL_CONNECTED) {
    WiFiClientSecure client;
    client.setInsecure();
    
    message.replace(" ", "%20");
    
    HTTPClient http;
    String url = "https://api.telegram.org/bot" + BOT_TOKEN + "/sendMessage?chat_id=" + CHAT_ID + "&text=" + message;
    
    http.begin(client, url);
    http.setTimeout(15000); 
    
    int httpCode = http.GET();
    if (httpCode > 0) {
      Serial.print("[IoT] Telegram Response Code: ");
      Serial.println(httpCode);
    } else {
      Serial.print("[IoT] Notification Failed, Error Code: ");
      Serial.println(httpCode);
    }
    http.end();
  } else {
    Serial.println("[IoT] Wi-Fi Disconnected.");
  }
}

void resetSystem() {
  isTripped = false;
  isHigh = false;
  legalNotificationSent = false;
  digitalWrite(RELAY_PIN, LOW);   // Fence Power RESTORED
  digitalWrite(GREEN_LED, LOW);   // Green LED OFF
  digitalWrite(RED_LED, LOW);     // Red LED OFF
  digitalWrite(BUZZER_PIN, LOW);  // Buzzer OFF
  Serial.println("[System] Alarm reset. System back to Standby.");
}

void setup() {
  Serial.begin(115200);

  pinMode(SENSOR_PIN, INPUT);
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(RED_LED, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);

  analogSetPinAttenuation(SENSOR_PIN, ADC_11db);

  digitalWrite(RELAY_PIN, LOW);
  digitalWrite(GREEN_LED, LOW);
  digitalWrite(RED_LED, LOW);
  digitalWrite(BUZZER_PIN, LOW);

  lcd.init();
  lcd.backlight();

  lcd.setCursor(0, 0);
  lcd.print("CONNECTING WIFI");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    lcd.setCursor(attempts % 16, 1);
    lcd.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n[Wi-Fi] Connected!");
    String location = getIPLocationLink();
    sendPhoneNotification("Fence System Online. Location: " + location);
  }

  lcd.clear();
}

void loop() {
  int sensorValue = analogRead(SENSOR_PIN);

  // Clamp minimal ADC floating noise below ~0.15V
  if (sensorValue < 150) {
    sensorValue = 0;
  }

  // Calculate voltage from 3-resistor divider
  float pinVoltage = (sensorValue * 3.3) / 4095.0;
  float batteryVoltage = pinVoltage * 3.0;

  // Voltage Thresholds:
  bool isLegalVoltage   = (batteryVoltage >= 1.0 && batteryVoltage <= 3.3);
  bool isIllegalVoltage = (batteryVoltage > 3.3);

  // --- TRIPPED STATE (LATCHED ALARM & RELAY OPEN) ---
  if (isTripped) {
    digitalWrite(RELAY_PIN, HIGH); // Trigger Relay to CUT POWER
    digitalWrite(GREEN_LED, LOW);
    digitalWrite(RED_LED, HIGH);
    digitalWrite(BUZZER_PIN, HIGH);

    if (!isIllegalVoltage && (millis() - tripStartTime >= AUTO_RESET_TIME)) {
      resetSystem();
    }
  } 
  // --- NORMAL / UNTRIPPED STATE ---
  else {
    if (isIllegalVoltage) { 
      if (!isHigh) {
        highStartTime = millis();
        isHigh = true;
      } 
      else if (millis() - highStartTime >= THRESHOLD_TIME) {
        isTripped = true;
        tripStartTime = millis();

        digitalWrite(RELAY_PIN, HIGH); // Trigger Relay to CUT POWER
        digitalWrite(GREEN_LED, LOW);
        digitalWrite(RED_LED, HIGH);
        digitalWrite(BUZZER_PIN, HIGH);

        String locationLink = getIPLocationLink();
        String alertMsg = "ALERT: Illegal high-voltage tap detected!: " + String(batteryVoltage, 2) + "V. Power Tripped! Location: " + locationLink;
        sendPhoneNotification(alertMsg);
      }
    } 
    else {
      isHigh = false;

      if (isLegalVoltage) {
        digitalWrite(GREEN_LED, HIGH);
        digitalWrite(RED_LED, LOW);
        digitalWrite(BUZZER_PIN, LOW);

        if (!legalNotificationSent) {
          String locationLink = getIPLocationLink();
          String legalMsg = "Legal voltage detected.Fence Pass" + String(batteryVoltage, 2) + "V. Location: " + locationLink;
          sendPhoneNotification(legalMsg);
          legalNotificationSent = true; 
        }
      } 
      else {
        digitalWrite(GREEN_LED, LOW);
        digitalWrite(RED_LED, LOW);
        digitalWrite(BUZZER_PIN, LOW);
        legalNotificationSent = false;
      }
    }
  }

  // --- LCD & SERIAL REFRESH (Every 200ms) ---
  if (millis() - lastLcdUpdate >= 200) {
    lastLcdUpdate = millis();

    Serial.print("Raw ADC: ");
    Serial.print(sensorValue);
    Serial.print(" | Calc Voltage: ");
    Serial.println(batteryVoltage);

    // Row 0: Status Header
    lcd.setCursor(0, 0);
    if (isTripped) {
      lcd.print("ALERT: ILLEGAL! ");
    } else if (isLegalVoltage) {
      lcd.print("STATUS: LEGAL   ");
    } else {
      lcd.print("STATUS: STANDBY ");
    }

    // Row 1: Displays 0.00V when power is tripped, or live voltage otherwise
    lcd.setCursor(0, 1);
    lcd.print("VOLTAGE: ");
    if (isTripped) {
      lcd.print("0.00V   ");
    } else {
      lcd.print(batteryVoltage, 2);
      lcd.print("V   ");
    }
  }

  delay(20);
}