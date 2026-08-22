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
const int RELAY_PIN        = 18; // Relay IN1
const int BUZZER_PIN       = 19;
const int RED_LED          = 26; // Red LED on GPIO 25
const int GREEN_LED        = 16; // Green LED on GPIO 16

unsigned long highStartTime = 0;
unsigned long tripStartTime = 0;
unsigned long lastLcdUpdate = 0;
bool isHigh = false;
bool isTripped = false;
bool legalNotificationSent = false;

const unsigned long THRESHOLD_TIME  = 200;  // 0.2s contact trigger
const unsigned long AUTO_RESET_TIME = 5000; // Auto-resets 5s after 9V is removed

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
  digitalWrite(RELAY_PIN, LOW);   // Fence Power ON
  digitalWrite(GREEN_LED, LOW);   // Green LED OFF in standby
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
    sendPhoneNotification("Fence System Online and Connected to Hotspot");
  }

  lcd.clear();
}

void loop() {
  int sensorValue = analogRead(SENSOR_PIN);

  // Calculate voltage from 3-resistor divider
  float pinVoltage = (sensorValue * 3.3) / 4095.0;
  float batteryVoltage = pinVoltage * 3.0;

  bool isLegalVoltage   = (batteryVoltage >= 1.0 && batteryVoltage <= 2.0);
  bool isIllegalVoltage = (batteryVoltage > 2); // Lowered slightly to 2.5V for better tolerance

  // --- TRIPPED STATE (LATCHED ALARM) ---
  if (isTripped) {
    digitalWrite(RELAY_PIN, HIGH);
    digitalWrite(GREEN_LED, LOW);
    digitalWrite(RED_LED, HIGH);
    digitalWrite(BUZZER_PIN, HIGH);

    // Auto-reset 5 seconds AFTER the 9V battery is disconnected (< 2.5V)
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

        // Immediately trigger alarm outputs
        digitalWrite(RELAY_PIN, HIGH);
        digitalWrite(GREEN_LED, LOW);
        digitalWrite(RED_LED, HIGH);
        digitalWrite(BUZZER_PIN, HIGH);

        String alertMsg = "ALERT: illegal high-voltage tap detected!: " + String(batteryVoltage, 2) + "V. Power tripped.";
        sendPhoneNotification(alertMsg);
      }
    } 
    else {
      isHigh = false; // Only reset high trigger state when voltage settles below threshold

      if (isLegalVoltage) {
        // Legal 1.5V State
        digitalWrite(GREEN_LED, HIGH);
        digitalWrite(RED_LED, LOW);
        digitalWrite(BUZZER_PIN, LOW);

        if (!legalNotificationSent) {
          String legalMsg = "legal voltage detected. fence pass: " + String(batteryVoltage, 2) + "V. Power approved.";
          sendPhoneNotification(legalMsg);
          legalNotificationSent = true; 
        }
      } 
      else {
        // Standby State (0V - Disconnected)
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

    // Print live voltage to Arduino Serial Monitor for easy debugging
    Serial.print("Raw ADC: ");
    Serial.print(sensorValue);
    Serial.print(" | Calc Voltage: ");
    Serial.println(batteryVoltage);

    lcd.setCursor(0, 0);
    if (isTripped) {
      lcd.print("ALERT: ILLEGAL! ");
    } else if (isLegalVoltage) {
      lcd.print("STATUS: LEGAL   ");
    } else {
      lcd.print("STATUS: STANDBY ");
    }

    lcd.setCursor(0, 1);
    lcd.print("VOLTAGE: ");
    lcd.print(batteryVoltage, 2);
    lcd.print("V   ");
  }

  delay(20);
}