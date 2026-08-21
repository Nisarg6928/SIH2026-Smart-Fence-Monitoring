#include <Wire.h>
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);

const int SENSOR_PIN = 34; // GPIO 34 Probe Wire
const int RELAY_PIN  = 18; // Relay IN1
const int BUZZER_PIN = 19;
const int RED_LED    = 4;
const int GREEN_LED  = 16;

unsigned long highStartTime = 0;
bool isHigh = false;
bool isTripped = false;

const unsigned long THRESHOLD_TIME = 1000; 

void setup() {
  pinMode(SENSOR_PIN, INPUT);
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(RED_LED, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);

  digitalWrite(RELAY_PIN, LOW); // Active-LOW: LOW keeps fence powered
  digitalWrite(GREEN_LED, HIGH);
  digitalWrite(RED_LED, LOW);
  digitalWrite(BUZZER_PIN, LOW);

  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("FENCE STATUS:");
  lcd.setCursor(0, 1);
  lcd.print("AUTHORIZED PASS ");
}

void loop() {
  if (isTripped) {
    digitalWrite(RELAY_PIN, HIGH); // Opens Relay -> 0V Power Cut
    digitalWrite(GREEN_LED, LOW);
    digitalWrite(RED_LED, HIGH);
    digitalWrite(BUZZER_PIN, HIGH);
    return;
  }

  int sensorValue = analogRead(SENSOR_PIN);

  if (sensorValue > 2000) { // Triggers when probe touches Column 22
    if (!isHigh) {
      highStartTime = millis();
      isHigh = true;
    } 
    else if (millis() - highStartTime >= THRESHOLD_TIME) {
      isTripped = true;
      digitalWrite(RELAY_PIN, HIGH); 
      digitalWrite(GREEN_LED, LOW);
      digitalWrite(RED_LED, HIGH);
      digitalWrite(BUZZER_PIN, HIGH);

      lcd.setCursor(0, 0);
      lcd.print("ALERT: ILLEGAL! ");
      lcd.setCursor(0, 1);
      lcd.print("POWER TRIPPED   ");
    }
  } 
  else {
    isHigh = false;
  }

  delay(20);
}