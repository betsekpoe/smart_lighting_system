#include <IRremote.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

#define PIR_PIN 2
#define CLAP_PIN 3
#define IR_PIN 7
#define LED_PIN 9
#define LDR_PIN A0

#define LDR_DARK_THRESHOLD 600
#define NO_MOTION_TIMEOUT 30000
#define NUM_READINGS 15
#define BRIGHTNESS_STEP 25
#define CLAP_WINDOW 500  // ms to wait for a second clap

// ── IR Codes ─────────────────────────────────────────
#define IR_BTN_1 0xC            // AUTO_MODE
#define IR_BTN_2 0x18           // CLAP_MODE
#define IR_BTN_BRIGHT_UP 0x15   // Vol+
#define IR_BTN_BRIGHT_DOWN 0x7  // Vol-
// ─────────────────────────────────────────────────────

LiquidCrystal_I2C lcd(0x27, 16, 2);

enum Mode { AUTO_MODE,
            CLAP_MODE,
            MANUAL_BRIGHTNESS };
Mode currentMode = AUTO_MODE;

bool lightOn = false;
bool lcdBacklight = true;
int brightness = 0;

unsigned long lastMotionTime = 0;
unsigned long lastLCDUpdate = 0;

// LDR smoothing
int readings[NUM_READINGS];
int readIndex = 0;
long total = 0;
int average = 0;

// Clap detection
int clapCount = 0;
unsigned long firstClapTime = 0;
bool lastClapState = false;

void setLight(bool on, int pwm = -1);

// ─────────────────────────────────────────────────────
void setup() {
  Serial.begin(9600);
  pinMode(PIR_PIN, INPUT);
  pinMode(CLAP_PIN, INPUT);
  pinMode(LED_PIN, OUTPUT);

  for (int i = 0; i < NUM_READINGS; i++) readings[i] = 0;

  lcd.init();
  lcd.backlight();
  IrReceiver.begin(IR_PIN, ENABLE_LED_FEEDBACK);

  Serial.println("System Ready.");
}

// ─────────────────────────────────────────────────────
void loop() {
  // ── PYTHON RECEIVER ──────────────────────────────
  if (Serial.available() > 0) {
    String voiceCmd = Serial.readStringUntil('\n');
    voiceCmd.trim();  // Remove any hidden spaces

    if (voiceCmd == "V_ON") {
      currentMode = MANUAL_BRIGHTNESS;
      setLight(true, 255);
    } else if (voiceCmd == "V_OFF") {
      currentMode = MANUAL_BRIGHTNESS;
      setLight(false);
    } else if (voiceCmd == "V_DIM") {
      currentMode = MANUAL_BRIGHTNESS;
      setLight(true, 50);  // 50 is roughly 20% brightness
    }
  }


  // ── IR REMOTE ──────────────────────────────────────
  if (IrReceiver.decode()) {
    int cmd = IrReceiver.decodedIRData.command;
    Serial.print("IR: 0x");
    Serial.println(cmd, HEX);

    if (cmd == IR_BTN_1) {
      currentMode = AUTO_MODE;
      Serial.println("→ AUTO");
    }
    if (cmd == IR_BTN_2) {
      currentMode = CLAP_MODE;
      Serial.println("→ CLAP");
    }

    if (cmd == IR_BTN_BRIGHT_UP) {
      currentMode = MANUAL_BRIGHTNESS;
      brightness = constrain(brightness + BRIGHTNESS_STEP, 0, 255);
      setLight(true, brightness);
      Serial.print("→ Bright: ");
      Serial.print(map(brightness, 0, 255, 0, 100));
      Serial.println("%");
    }
    if (cmd == IR_BTN_BRIGHT_DOWN) {
      currentMode = MANUAL_BRIGHTNESS;
      brightness = constrain(brightness - BRIGHTNESS_STEP, 0, 255);
      setLight(brightness > 0, brightness);
      Serial.print("→ Bright: ");
      Serial.print(map(brightness, 0, 255, 0, 100));
      Serial.println("%");
    }

    IrReceiver.resume();
  }

  // ── LDR SMOOTHING ──────────────────────────────────
  total -= readings[readIndex];
  readings[readIndex] = analogRead(LDR_PIN);
  total += readings[readIndex];
  readIndex = (readIndex + 1) % NUM_READINGS;
  average = total / NUM_READINGS;

  // ── AUTO MODE ──────────────────────────────────────
  // PIR triggers light; LDR sets the brightness level.
  // Light shuts off after NO_MOTION_TIMEOUT of no motion.
  if (currentMode == AUTO_MODE) {
    bool motion = (digitalRead(PIR_PIN) == HIGH);

    if (motion) {
      lastMotionTime = millis();
      if (!lcdBacklight) {
        lcd.backlight();
        lcdBacklight = true;
      }

      if (average < LDR_DARK_THRESHOLD) {
        // Darker room → higher brightness (inverted LDR scale)
        int autoBrightness = map(average, LDR_DARK_THRESHOLD, 0, 40, 255);
        autoBrightness = constrain(autoBrightness, 40, 255);
        setLight(true, autoBrightness);  // continuously track ambient light
      } else {
        // Room is bright enough — no need for the bulb
        if (lightOn) setLight(false);
      }
    } else {
      if (millis() - lastMotionTime > NO_MOTION_TIMEOUT) {
        if (lightOn) setLight(false);
        if (lcdBacklight) {
          lcd.noBacklight();
          lcdBacklight = false;
        }
      }
    }
  }

  // ── CLAP MODE ──────────────────────────────────────
  // 1 clap  → turn OFF
  // 2 claps → turn ON
  if (currentMode == CLAP_MODE) {
    bool clapState = (digitalRead(CLAP_PIN) == HIGH);

    // Rising edge = new clap detected
    if (clapState && !lastClapState) {
      if (clapCount == 0) {
        clapCount = 1;
        firstClapTime = millis();
      } else if (clapCount == 1) {
        clapCount = 2;  // second clap registered
      }
    }
    lastClapState = clapState;

    // Evaluate after the clap window closes
    if (clapCount > 0 && millis() - firstClapTime > CLAP_WINDOW) {
      if (clapCount == 1) {
        setLight(false);
        Serial.println("Clap ×1 → OFF");
      } else if (clapCount >= 2) {
        setLight(true, 255);
        Serial.println("Clap ×2 → ON");
      }
      clapCount = 0;  // reset for next clap sequence
    }
  }

  // ── LCD UPDATE (every 500 ms) ───────────────────────
  if (millis() - lastLCDUpdate > 500 && lcdBacklight) {
    lcd.setCursor(0, 0);
    switch (currentMode) {
      case AUTO_MODE: lcd.print("MODE: AUTO     "); break;
      case CLAP_MODE: lcd.print("MODE: CLAP     "); break;
      case MANUAL_BRIGHTNESS: lcd.print("MODE: MANUAL   "); break;
    }
    lcd.setCursor(0, 1);
    lcd.print("R:");
    lcd.print(map(average, 0, 1023, 0, 100));
    lcd.print("% ");
    lcd.setCursor(8, 1);
    lcd.print("B:");
    lcd.print(map(brightness, 0, 255, 0, 100));
    lcd.print("% ");
    lastLCDUpdate = millis();
  }
}

// ─────────────────────────────────────────────────────
void setLight(bool on, int pwm) {
  lightOn = on;
  brightness = on ? (pwm >= 0 ? pwm : brightness) : 0;
  analogWrite(LED_PIN, brightness);
}