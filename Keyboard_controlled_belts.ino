#include <Wire.h>
#include <Motoron.h>
#include <Adafruit_INA219.h>

// === INA219 current sensors ===
  Adafruit_INA219 ina219_1(0x40);  // First sensor 
  Adafruit_INA219 ina219_2(0x44);  // Second sensor 

// === Motoron M2T550 driver ===
  MotoronI2C motoron(0x10);

// === TB6612FNG control pins ===
  #define AIN1 2
  #define AIN2 4
  #define BIN1 7
  #define BIN2 8
  #define STBY 5
  #define PWMA1 3   // Motor M3
  #define PWMB1 6   // Motor M4
  #define PWMA2 9   // Motor M2
  #define PWMB2 10  // Motor M1

// === Switch pin ===
  #define SWITCH_PIN 13

// === Settings ===
  const float CURRENT_THRESHOLD_mA = 60.0;
  const unsigned long TIMEOUT_MS = 4500;
  const unsigned long TIMEOUT_MS_SHORT = 1500;
  const int MOTORON_SPEED = 300;      // PWM (0–800)
  // const int BELT_SPEED = 150;      // PWM (0–255), used if all motor gearboxes are the same

// === Internal switch state ===
  bool lastSwitchState = HIGH;

void setup() {
  Serial.begin(9600);

  // TB6612FNG control pins
  pinMode(AIN1, OUTPUT); pinMode(AIN2, OUTPUT);
  pinMode(BIN1, OUTPUT); pinMode(BIN2, OUTPUT);
  pinMode(PWMA1, OUTPUT); pinMode(PWMB1, OUTPUT);
  pinMode(PWMA2, OUTPUT); pinMode(PWMB2, OUTPUT);
  pinMode(STBY, OUTPUT);

  // Switch input
  pinMode(SWITCH_PIN, INPUT_PULLUP);

  // I2C setup
  Wire.begin();
  motoron.reinitialize();
  delay(100);
  motoron.clearResetFlag();
  motoron.disableCrc();
  motoron.disableCommandTimeout();

  // Initialize INA219 sensors
  ina219_1.begin();
  ina219_2.begin();

  Serial.println("Setup complete.");
}

// Run sequence when switch is activated
void loop() {
  bool currentSwitchState = digitalRead(SWITCH_PIN);

  if (lastSwitchState == HIGH && currentSwitchState == LOW) {
    Serial.println("Switch activated.");
    runMotorSequence(); // Run sequence when switch is activated
  }

  lastSwitchState = currentSwitchState;
  delay(10);  // Debounce
}

// === Full motor sequence ===
void runMotorSequence() {

// === MOTORON motors (back motors) CLOSING ===
  Serial.println("Fingers closing...");
  Serial.println("M1_current\tM2_current"); // Serial Plotter labels

  motoron.setSpeed(1, -MOTORON_SPEED);  // Motor 1 (via INA219_1)
  motoron.setSpeed(2, -MOTORON_SPEED);  // Motor 2 (via INA219_2)
                      // - to close and + to open

  unsigned long start1 = millis();
  unsigned long start2 = millis();
  float current1 = 0;
  float current2 = 0;
  bool done1 = false;
  bool done2 = false;

// Close fingers until current spikes
while (!(done1 && done2)) {
  if (!done1) {
    current1 = ina219_1.getCurrent_mA();
  }
  if (!done2) {
    current2 = ina219_2.getCurrent_mA();
  }
 
  // Plot both currents to Serial Plotter
  Serial.print(abs(current1));
  Serial.print("\t");
  Serial.println(abs(current2)); 

  if (!done1) {
    if (abs(current1) > CURRENT_THRESHOLD_mA) {
      motoron.setSpeed(1, 0);
      done1 = true;
      Serial.println("Motor 1 stopped (threshold reached).");
    } else if (millis() - start1 > TIMEOUT_MS) {
      motoron.setSpeed(1, 0);
      done1 = true;
      Serial.println("Motor 1 stopped (timeout).");
    }
  }

  if (!done2) {
    if (abs(current2) > CURRENT_THRESHOLD_mA) {
      motoron.setSpeed(2, 0);
      done2 = true;
      Serial.println("Motor 2 stopped (threshold reached).");
    } else if (millis() - start2 > TIMEOUT_MS) {
      motoron.setSpeed(2, 0);
      done2 = true;
      Serial.println("Motor 2 stopped (timeout).");
    }
  }

  delay(10); 
}

// delay(300);  // Short pause (optional)


// === BELT MOTORS (TB6612FNGs) UP ===
Serial.println("Keyboard control active (w = up, s = down, ESC = exit control).");

bool done = false;

while (!done) {
  if (Serial.available()) {
    char cmd = Serial.read();

    if (cmd == 'w') { // Motors up
      digitalWrite(STBY, HIGH);

      digitalWrite(AIN1, HIGH); digitalWrite(AIN2, LOW);  // Right motors up
      digitalWrite(BIN1, HIGH); digitalWrite(BIN2, LOW);  // Left motors up

      analogWrite(PWMB2, 113); // Motor M1
      analogWrite(PWMA2, 88);  // Motor M2
      analogWrite(PWMA1, 145); // Motor M3 
      analogWrite(PWMB1, 197); // Motor M4
    }

    else if (cmd == 's') { // Motors down
      digitalWrite(STBY, HIGH);

      digitalWrite(AIN1, LOW); digitalWrite(AIN2, HIGH);  // Right motors down
      digitalWrite(BIN1, LOW); digitalWrite(BIN2, HIGH);  // Left motors down

      analogWrite(PWMB2, 110); // Motor M1
      analogWrite(PWMA2, 88);  // Motor M2
      analogWrite(PWMA1, 145); // Motor M3
      analogWrite(PWMB1, 193); // Motor M4
    }

    else if (cmd == '0') { // Stop if no key is pressed
      analogWrite(PWMA1, 0);
      analogWrite(PWMA2, 0);
      analogWrite(PWMB1, 0);
      analogWrite(PWMB2, 0);
      digitalWrite(STBY, LOW);
    }

    else if (cmd == 'x') { // Exit if ESC key is pressed
      Serial.println("Exiting belt control...");
      analogWrite(PWMA1, 0);
      analogWrite(PWMA2, 0);
      analogWrite(PWMB1, 0);
      analogWrite(PWMB2, 0);
      digitalWrite(STBY, LOW);
      done = true;
    }
  }

  delay(10); // Loop every 10ms
}

// After ESC key is pressed:

// === MOTORON motors (back motors) OPENING ===
  Serial.println("Fingers opening...");
  Serial.println("M1_current\tM2_current");

  motoron.setSpeed(1, MOTORON_SPEED);  // Motor 1 (via INA219_1)
  motoron.setSpeed(2, MOTORON_SPEED);  // Motor 2 (via INA219_2)
                      // - to close and + to open

  unsigned long start1open = millis();
  unsigned long start2open = millis();
  float current1open = 0;
  float current2open = 0;
  bool done1open = false;
  bool done2open = false;

// Open fingers
while (!(done1open && done2open)) {
  if (!done1open) {
    current1open = ina219_1.getCurrent_mA();
  }
  if (!done2open) {
    current2open = ina219_2.getCurrent_mA();
  }
 
  // Plot both currents to Serial Plotter
  Serial.print(abs(current1open));
  Serial.print("\t");
  Serial.println(abs(current2open)); 

  if (!done1open) {
    if (abs(current1open) > CURRENT_THRESHOLD_mA) {
      motoron.setSpeed(1, 0);
      done1open = true;
      Serial.println("Motor 1 stopped (threshold reached).");
    } else if (millis() - start1open > TIMEOUT_MS) {
      motoron.setSpeed(1, 0);
      done1open = true;
      Serial.println("Motor 1 stopped (timeout).");
    }
  }

  if (!done2open) {
    if (abs(current2open) > CURRENT_THRESHOLD_mA) {
      motoron.setSpeed(2, 0);
      done2open = true;
      Serial.println("Motor 2 stopped (threshold reached).");
    } else if (millis() - start2open > TIMEOUT_MS) {
      motoron.setSpeed(2, 0);
      done2open = true;
      Serial.println("Motor 2 stopped (timeout).");
    }
  }

  delay(10); 
}
}
