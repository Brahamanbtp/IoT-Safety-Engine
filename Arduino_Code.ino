#include <Wire.h>
#include <SPI.h>
#include <MFRC522.h>
#include <DHT.h>
#include <LiquidCrystal_I2C.h>
#include <MPU6050.h>

/* ================= LCD ================= */

LiquidCrystal_I2C lcd(0x27, 16, 2);

/* ================= RFID ================= */

#define SS_PIN 5
#define RST_PIN 27
MFRC522 rfid(SS_PIN, RST_PIN);

/* ================= DHT ================= */

#define DHTPIN 4
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

/* ================= ULTRASONIC ================= */

#define TRIG_PIN 12
#define ECHO_PIN 14

/* ================= ANALOG SENSORS ================= */

#define GAS_PIN 34
#define CURRENT_PIN 35

/* ================= RELAY ================= */

#define RELAY1 26
#define RELAY2 25

/* ================= MPU6050 ================= */

MPU6050 mpu;

/* ================= GLOBAL VARIABLES ================= */

float temperature = 0;
float humidity = 0;
int gasValue = 0;
float distanceValue = 0;
float currentValue = 0;
float vibrationValue = 0;

bool authorized = false;

int risk = 0;
int confidence = 100;
String reason = "Normal";

/* ================= RFID UID ================= */

byte authorizedUID[4] = {0xC5, 0x1C, 0xD8, 0x3F};

/* ================= CURRENT SENSOR ================= */

float ACS_OFFSET = 1.65;
float ACS_SENSITIVITY = 0.185;

/* ================= ACCESS TIMER ================= */

unsigned long accessTimer = 0;
const unsigned long accessDuration = 5000;


/* ================= SETUP ================= */

void setup() {

  Serial.begin(115200);

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  pinMode(RELAY1, OUTPUT);
  pinMode(RELAY2, OUTPUT);

  digitalWrite(RELAY1, HIGH);
  digitalWrite(RELAY2, HIGH);

  Wire.begin(21,22);

  lcd.begin();
  lcd.backlight();

  lcd.setCursor(0,0);
  lcd.print("SMART SECURITY");
  lcd.setCursor(0,1);
  lcd.print("SYSTEM START");
  delay(2000);
  lcd.clear();

  SPI.begin();
  rfid.PCD_Init();

  dht.begin();

  mpu.initialize();

  Serial.println("SYSTEM READY");
}


/* ================= LOOP ================= */

void loop() {

  /* ===== RFID CHECK ===== */

  if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {

    bool match = true;

    Serial.print("Card UID: ");

    for (byte i = 0; i < 4; i++) {

      Serial.print(rfid.uid.uidByte[i], HEX);
      Serial.print(" ");

      if (rfid.uid.uidByte[i] != authorizedUID[i]) {
        match = false;
      }
    }

    Serial.println();

    if(match){

      authorized = true;
      accessTimer = millis();

      Serial.println("AUTHORIZED CARD");

      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("ACCESS OK");

    }
    else{

      authorized = false;

      Serial.println("UNAUTHORIZED CARD");

      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("ACCESS DENIED");
      delay(2000);
    }

    rfid.PICC_HaltA();
  }


  /* ===== ACCESS TIMEOUT ===== */

  if (authorized && millis() - accessTimer > accessDuration) {
    authorized = false;
  }


  /* ===== DHT SENSOR ===== */

  temperature = dht.readTemperature();
  humidity = dht.readHumidity();

  if(isnan(temperature)) temperature = 0;
  if(isnan(humidity)) humidity = 0;


  /* ===== GAS SENSOR ===== */

  gasValue = analogRead(GAS_PIN);


  /* ===== CURRENT SENSOR ===== */

  float voltage = analogRead(CURRENT_PIN) * (3.3 / 4095.0);
  currentValue = (voltage - ACS_OFFSET) / ACS_SENSITIVITY;

  if(currentValue < 0) currentValue = 0;


  /* ===== ULTRASONIC SENSOR ===== */

  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);

  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);

  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH, 30000);

  if(duration == 0)
    distanceValue = -1;
  else
    distanceValue = duration * 0.034 / 2;


  /* ===== MPU6050 ===== */

  int16_t ax, ay, az;
  mpu.getAcceleration(&ax, &ay, &az);

  vibrationValue = (abs(ax) + abs(ay) + abs(az)) / 16384.0;


  /* ===== RISK ENGINE ===== */

  risk = 0;
  reason = "";

  if(!authorized){
    risk += 20;
    reason += "Unauthorized ";
  }

  if(gasValue > 2000){
    risk += 30;
    reason += "GasHigh ";
  }

  if(temperature > 40){
    risk += 25;
    reason += "TempHigh ";
  }

  if(distanceValue >0 && distanceValue <100){
    risk += 15;
    reason += "Movement ";
  }

  if(currentValue >5){
    risk += 20;
    reason += "OverCurrent ";
  }

  if(vibrationValue >2){
    risk += 15;
    reason += "Vibration ";
  }

  if(reason=="") reason="Normal";

  risk = constrain(risk,0,100);
  confidence = 100 - risk;


  /* ===== RELAY CONTROL ===== */

  Serial.print("Relay State: ");

  if(risk >= 30){

    digitalWrite(RELAY1, LOW);
    digitalWrite(RELAY2, LOW);

    Serial.println("ON");

  } else {

    digitalWrite(RELAY1, HIGH);
    digitalWrite(RELAY2, HIGH);

    Serial.println("OFF");

  }


  /* ===== SERIAL MONITOR ===== */

  Serial.println("--------------");

  Serial.print("Temp: "); Serial.println(temperature);
  Serial.print("Humidity: "); Serial.println(humidity);
  Serial.print("Gas: "); Serial.println(gasValue);
  Serial.print("Current: "); Serial.println(currentValue);
  Serial.print("Distance: "); Serial.println(distanceValue);
  Serial.print("Vibration: "); Serial.println(vibrationValue);

  Serial.print("Authorized: "); Serial.println(authorized);

  Serial.print("Risk Score: "); Serial.println(risk);
  Serial.print("Confidence: "); Serial.println(confidence);
  Serial.print("Reason: "); Serial.println(reason);


  /* ===== LCD DISPLAY ===== */

  lcd.setCursor(0,0);
  lcd.print("R:");
  lcd.print(risk);
  lcd.print(" C:");
  lcd.print(confidence);
  lcd.print("   ");

  lcd.setCursor(0,1);

  if(authorized)
    lcd.print("ACCESS OK     ");
  else
    lcd.print("ACCESS DENIED ");

  delay(1000);
}
