#define DB_ACTIVE 0

#include <MPU6050.h>
#include <Wire.h>
#include "LUFAConfig.h"
#include <LUFA.h>
#include "Joystick.h"
#define BOUNCE_WITH_PROMPT_DETECTION
#include <Bounce2.h>

#define MILLIDEBOUNCE 1  // Debounce time in milliseconds
#define pinOBLED 17      // Onboard LED pin

#define joyX A0
#define joyY A1

MPU6050 mpu;           // Create an instance of the MPU6050
bool altmode = false;  // Global altmode variable

// --- Global flag for homeheld mode ---
// When false, buttons L, A, and Y use toggle logic;
// when true, a held press sends the alternate (momentary) output.
bool homeheld = false;
bool homeheldToggleHandled = false;  // Prevent multiple toggles per long press

// --- Update button indices and array sizes ---
#define BUTTON1 0  // A button (normal: sends A; long press toggles L3)
#define BUTTON2 1  // B button (long press sends SELECT)
#define BUTTON3 2  // X button (long press sends START)
#define BUTTON4 3  // Y button (normal: sends Y; long press toggles R3)
#define BUTTON5 4  // Used for DPAD mode (unchanged)
#define SHIFT1 5   // L button (short press toggles ZL, long press = LB)
#define SHIFT2 6   // R button (short press toggles ZR, long press = RB)
#define BUTTON6 7  // HOME button (short press = HOME, long press toggles homeheld)
#define BUTTON7 8  // Button7 toggles altmode

byte buttonStatus[9];  // Now 9 entries

// --- Button state machine indices ---
#define BUTTON_SM_1 0  // For A button (L3 toggle in default mode)
#define BUTTON_SM_2 1  // For B button (long press = SELECT)
#define BUTTON_SM_3 2  // For X button (long press = START)
#define BUTTON_SM_4 3  // For Y button (R3 toggle in default mode)
#define SHIFT_SM_1 4   // For SHIFT1 (L button)
#define SHIFT_SM_2 5   // For SHIFT2 (R button)
#define BUTTON_SM_6 6  // For HOME button
#define BUTTON_SM_7 7  // For altmode toggle

// --- Button mask definitions ---
#define DPAD_NOTHING_MASK_ON 0x08
#define DPAD_UP_MASK_ON 0x00
#define DPAD_UPRIGHT_MASK_ON 0x01
#define DPAD_RIGHT_MASK_ON 0x02
#define DPAD_DOWNRIGHT_MASK_ON 0x03
#define DPAD_DOWN_MASK_ON 0x04
#define DPAD_DOWNLEFT_MASK_ON 0x05
#define DPAD_LEFT_MASK_ON 0x06
#define DPAD_UPLEFT_MASK_ON 0x07
#define A_MASK_ON 0x04
#define B_MASK_ON 0x02
#define X_MASK_ON 0x08
#define Y_MASK_ON 0x01
#define LB_MASK_ON 0x10
#define RB_MASK_ON 0x20
#define ZL_MASK_ON 0x40
#define ZR_MASK_ON 0x80
#define L3_MASK_ON 0x400
#define R3_MASK_ON 0x800
#define START_MASK_ON 0x200
#define SELECT_MASK_ON 0x100
#define HOME_MASK_ON 0x1000

// --- Define pins for buttons and shifts ---
#define PIN_BUTTON1 16
#define PIN_BUTTON2 10
#define PIN_BUTTON3 9
#define PIN_BUTTON4 7
#define PIN_BUTTON5 15
#define PIN_BUTTON6 5
#define PIN_BUTTON7 4
#define PIN_SHIFT1 6
#define PIN_SHIFT2 8
#define PIN_ALT_MODE_0 0  // When low: force active stick axes to neutral (128)
#define PIN_ALT_MODE_1 1  // When low: accelerometer functions normally

// --- Create Bounce objects ---
Bounce button1 = Bounce();
Bounce button2 = Bounce();
Bounce button3 = Bounce();
Bounce button4 = Bounce();
Bounce button5 = Bounce();
Bounce shift1 = Bounce();
Bounce shift2 = Bounce();
Bounce button6 = Bounce();
Bounce button7 = Bounce();

// --- State machine definitions ---
enum ButtonState {
  BUTTON_IDLE,
  BUTTON_PRESSED,
  BUTTON_LONG_PRESSED
};
ButtonState buttonStates[8] = {
  BUTTON_IDLE, BUTTON_IDLE, BUTTON_IDLE, BUTTON_IDLE,
  BUTTON_IDLE, BUTTON_IDLE, BUTTON_IDLE, BUTTON_IDLE
};

enum ButtonAction {
  ACTION_NONE,
  ACTION_SHORT_PRESS,
  ACTION_SHORT_PRESS_HOLDING,
  ACTION_SHORT_RELEASE
};
ButtonAction actionNeeded[8] = {
  ACTION_NONE, ACTION_NONE, ACTION_NONE, ACTION_NONE,
  ACTION_NONE, ACTION_NONE, ACTION_NONE, ACTION_NONE
};

unsigned long buttonPressStartTime[8] = { 0,0,0,0,0,0,0,0 };
unsigned long pressStartTime[8]       = { 0,0,0,0,0,0,0,0 };

// --- Variables for toggle logic (default mode) ---
bool shift1ZLToggle = false;
bool shift1ToggleHandled = false;
bool shift2ZRToggle = false;       // <-- New toggle variable for ZR
bool shift2ToggleHandled = false;  // <-- Helper to avoid repeated toggles
bool aToggle = false;
bool aToggleHandled = false;
bool yToggle = false;
bool yToggleHandled = false;

void setupPins() {
  button1.attach(PIN_BUTTON1, INPUT_PULLUP);
  button2.attach(PIN_BUTTON2, INPUT_PULLUP);
  button3.attach(PIN_BUTTON3, INPUT_PULLUP);
  button4.attach(PIN_BUTTON4, INPUT_PULLUP);
  button5.attach(PIN_BUTTON5, INPUT_PULLUP);
  shift1.attach(PIN_SHIFT1, INPUT_PULLUP);
  shift2.attach(PIN_SHIFT2, INPUT_PULLUP);
  button6.attach(PIN_BUTTON6, INPUT_PULLUP);
  button7.attach(PIN_BUTTON7, INPUT_PULLUP);

  button1.interval(MILLIDEBOUNCE);
  button2.interval(MILLIDEBOUNCE);
  button3.interval(MILLIDEBOUNCE);
  button4.interval(MILLIDEBOUNCE);
  button5.interval(MILLIDEBOUNCE);
  shift1.interval(MILLIDEBOUNCE);
  shift2.interval(MILLIDEBOUNCE);
  button6.interval(MILLIDEBOUNCE);
  button7.interval(MILLIDEBOUNCE);

  pinMode(pinOBLED, OUTPUT);
  digitalWrite(pinOBLED, LOW);

  // Set up the accelerometer control pins
  pinMode(PIN_ALT_MODE_0, INPUT_PULLUP);
  pinMode(PIN_ALT_MODE_1, INPUT_PULLUP);
}

void setup() {
  setupPins();

  Wire.begin();
  mpu.initialize();
  if (!mpu.testConnection()) {
    while (true) {
      digitalWrite(pinOBLED, !digitalRead(pinOBLED));
      delay(500);
    }
  }

  SetupHardware();
  GlobalInterruptEnable();
}

void loop() {
  readAnalogStick();
  readMPU6050();
  readButtons();
  processButtons();
  HID_Task();
  USB_USBTask();
}

void readAnalogStick() {
  const int deadzone = 15;
  int xValue = analogRead(joyX);
  int yValue = analogRead(joyY);

  if (abs(xValue - 512) < deadzone) xValue = 512;
  if (abs(yValue - 512) < deadzone) yValue = 512;

  // DPAD mode if button5 is pressed
  if (buttonStatus[BUTTON5] == LOW) {
    // Map analog to DPAD
    if (xValue < 341) {
      if (yValue < 341) ReportData.HAT = DPAD_UPRIGHT_MASK_ON;
      else if (yValue > 682) ReportData.HAT = DPAD_DOWNRIGHT_MASK_ON;
      else ReportData.HAT = DPAD_RIGHT_MASK_ON;
    } else if (xValue > 682) {
      if (yValue < 341) ReportData.HAT = DPAD_UPLEFT_MASK_ON;
      else if (yValue > 682) ReportData.HAT = DPAD_DOWNLEFT_MASK_ON;
      else ReportData.HAT = DPAD_LEFT_MASK_ON;
    } else {
      if (yValue < 341) ReportData.HAT = DPAD_UP_MASK_ON;
      else if (yValue > 682) ReportData.HAT = DPAD_DOWN_MASK_ON;
      else ReportData.HAT = DPAD_NOTHING_MASK_ON;
    }
    // In DPAD mode, force whichever stick is active to neutral
    if (altmode) {
      ReportData.LX = 128;
      ReportData.LY = 128;
    } else {
      ReportData.RX = 128;
      ReportData.RY = 128;
    }
  } else {
    // Normal analog mode
    ReportData.HAT = DPAD_NOTHING_MASK_ON;
    if (altmode) {
      ReportData.LX = map(xValue, 0, 1023, 255, 0);
      ReportData.LY = map(yValue, 0, 1023, 0, 255);
    } else {
      ReportData.RX = map(xValue, 0, 1023, 255, 0);
      ReportData.RY = map(yValue, 0, 1023, 0, 255);
    }
  }
}

void readMPU6050() {
  // If PIN_ALT_MODE_0 is low, force stick axes to neutral
  if (digitalRead(PIN_ALT_MODE_0) == LOW) {
    if (altmode || buttonStatus[BUTTON5] == LOW) {
      ReportData.RX = 128;
      ReportData.RY = 128;
    } else {
      ReportData.LX = 128;
      ReportData.LY = 128;
    }
    return;
  }

  int16_t ax, ay, az;
  mpu.getAcceleration(&ax, &ay, &az);

  const int16_t deadzoneThreshold = 9001;
  const int16_t deadzoneThreshold2 = 9000;
  int16_t adjustedX = -ay;
  int16_t adjustedY = -az;

  static float smoothedX = 0;
  static float smoothedY = 0;
  const float alpha = 0.1;

  // Initialize smoothing if first time
  if (smoothedX == 0 && smoothedY == 0) {
    smoothedX = adjustedX;
    smoothedY = adjustedY;
  }

  smoothedX = alpha * adjustedX + (1 - alpha) * smoothedX;
  smoothedY = alpha * adjustedY + (1 - alpha) * smoothedY;

  if (altmode || buttonStatus[BUTTON5] == LOW) {
    // A bit bigger range for Right stick
    ReportData.RX = (abs(smoothedX) > deadzoneThreshold)
      ? map(constrain((int32_t)smoothedX, -10000, 10000), -10000, 10000, 0, 255)
      : 128;
    ReportData.RY = (abs(smoothedY) > deadzoneThreshold)
      ? map(constrain((int32_t)smoothedY, -10000, 10000), -10000, 10000, 0, 255)
      : 128;
  } else {
    // Slightly narrower range for Left stick
    ReportData.LX = (abs(smoothedX) > deadzoneThreshold2)
      ? map(constrain((int32_t)smoothedX, -8000, 8000), -8000, 8000, 0, 255)
      : 128;
    ReportData.LY = (abs(smoothedY) > deadzoneThreshold2)
      ? map(constrain((int32_t)smoothedY, -6000, 6000), -6000, 6000, 0, 255)
      : 128;
  }
}

void readButtons() {
  button1.update();
  button2.update();
  button3.update();
  button4.update();
  button5.update();
  shift1.update();
  shift2.update();
  button6.update();
  button7.update();

  buttonStatus[BUTTON1] = button1.read();
  buttonStatus[BUTTON2] = button2.read();
  buttonStatus[BUTTON3] = button3.read();
  buttonStatus[BUTTON4] = button4.read();
  buttonStatus[BUTTON5] = button5.read();
  buttonStatus[SHIFT1] = shift1.read();
  buttonStatus[SHIFT2] = shift2.read();
  buttonStatus[BUTTON6] = button6.read();
  buttonStatus[BUTTON7] = button7.read();

  // Update state machines for relevant buttons
  handleButtonPress(button1, BUTTON_SM_1);
  handleButtonPress(button2, BUTTON_SM_2);
  handleButtonPress(button3, BUTTON_SM_3);
  handleButtonPress(button4, BUTTON_SM_4);
  handleButtonPress(shift1, SHIFT_SM_1);
  handleButtonPress(shift2, SHIFT_SM_2);
  handleButtonPress(button6, BUTTON_SM_6);
  handleButtonPress(button7, BUTTON_SM_7);
}

void handleButtonPress(Bounce &button, int idx) {
  switch (buttonStates[idx]) {
    case BUTTON_IDLE:
      if (button.fell()) {
        buttonPressStartTime[idx] = millis();
        buttonStates[idx] = BUTTON_PRESSED;
      }
      break;

    case BUTTON_PRESSED:
      if (button.read() == LOW) {
        // Check if we've reached the long-press threshold
        if (millis() - buttonPressStartTime[idx] >= 500) {
          buttonStates[idx] = BUTTON_LONG_PRESSED;
        }
      } else if (button.rose()) {
        // It was a short press
        buttonStates[idx] = BUTTON_IDLE;
        actionNeeded[idx] = ACTION_SHORT_PRESS;
      }
      break;

    case BUTTON_LONG_PRESSED:
      // Once in long-press state, remain until button is released
      if (button.read() == HIGH) {
        buttonStates[idx] = BUTTON_IDLE;
      }
      break;
  }
}

void processButtons() {
  // Clear previous button states
  ReportData.Button = 0;

  // Process each button state machine
  for (int i = BUTTON_SM_1; i <= BUTTON_SM_7; i++) {
    // BUTTON_SM_1: A button (toggles L3 in default mode)
    if (i == BUTTON_SM_1) {
      if (!homeheld) {
        // Default mode: long press toggles L3; short press => A
        if (buttonStates[BUTTON_SM_1] == BUTTON_LONG_PRESSED) {
          if (!aToggleHandled) {
            aToggle = !aToggle;
            aToggleHandled = true;
          }
        } else {
          aToggleHandled = false;
          if (actionNeeded[BUTTON_SM_1] == ACTION_SHORT_PRESS) {
            ReportData.Button |= A_MASK_ON;
            pressStartTime[BUTTON_SM_1] = millis();
            actionNeeded[BUTTON_SM_1] = ACTION_SHORT_PRESS_HOLDING;
          } else if (actionNeeded[BUTTON_SM_1] == ACTION_SHORT_PRESS_HOLDING) {
            ReportData.Button |= A_MASK_ON;
            if (millis() - pressStartTime[BUTTON_SM_1] >= 255)
              actionNeeded[BUTTON_SM_1] = ACTION_SHORT_RELEASE;
          } else if (actionNeeded[BUTTON_SM_1] == ACTION_SHORT_RELEASE) {
            actionNeeded[BUTTON_SM_1] = ACTION_NONE;
          }
        }
      } else {
        // homeheld: long press => momentary L3, short press => A
        if (buttonStates[BUTTON_SM_1] == BUTTON_LONG_PRESSED) {
          ReportData.Button |= L3_MASK_ON;
        } else {
          if (actionNeeded[BUTTON_SM_1] == ACTION_SHORT_PRESS) {
            ReportData.Button |= A_MASK_ON;
            pressStartTime[BUTTON_SM_1] = millis();
            actionNeeded[BUTTON_SM_1] = ACTION_SHORT_PRESS_HOLDING;
          } else if (actionNeeded[BUTTON_SM_1] == ACTION_SHORT_PRESS_HOLDING) {
            ReportData.Button |= A_MASK_ON;
            if (millis() - pressStartTime[BUTTON_SM_1] >= 255)
              actionNeeded[BUTTON_SM_1] = ACTION_SHORT_RELEASE;
          } else if (actionNeeded[BUTTON_SM_1] == ACTION_SHORT_RELEASE) {
            actionNeeded[BUTTON_SM_1] = ACTION_NONE;
          }
        }
      }
    }
    // BUTTON_SM_2: B button
    else if (i == BUTTON_SM_2) {
      if (buttonStates[BUTTON_SM_2] == BUTTON_LONG_PRESSED) {
        ReportData.Button |= SELECT_MASK_ON;
      } else if (actionNeeded[BUTTON_SM_2] == ACTION_SHORT_PRESS) {
        ReportData.Button |= B_MASK_ON;
        pressStartTime[BUTTON_SM_2] = millis();
        actionNeeded[BUTTON_SM_2] = ACTION_SHORT_PRESS_HOLDING;
      } else if (actionNeeded[BUTTON_SM_2] == ACTION_SHORT_PRESS_HOLDING) {
        ReportData.Button |= B_MASK_ON;
        if (millis() - pressStartTime[BUTTON_SM_2] >= 255)
          actionNeeded[BUTTON_SM_2] = ACTION_SHORT_RELEASE;
      } else if (actionNeeded[BUTTON_SM_2] == ACTION_SHORT_RELEASE) {
        actionNeeded[BUTTON_SM_2] = ACTION_NONE;
      }
    }
    // BUTTON_SM_3: X button
    else if (i == BUTTON_SM_3) {
      if (buttonStates[BUTTON_SM_3] == BUTTON_LONG_PRESSED) {
        ReportData.Button |= START_MASK_ON;
      } else if (actionNeeded[BUTTON_SM_3] == ACTION_SHORT_PRESS) {
        ReportData.Button |= X_MASK_ON;
        pressStartTime[BUTTON_SM_3] = millis();
        actionNeeded[BUTTON_SM_3] = ACTION_SHORT_PRESS_HOLDING;
      } else if (actionNeeded[BUTTON_SM_3] == ACTION_SHORT_PRESS_HOLDING) {
        ReportData.Button |= X_MASK_ON;
        if (millis() - pressStartTime[BUTTON_SM_3] >= 255)
          actionNeeded[BUTTON_SM_3] = ACTION_SHORT_RELEASE;
      } else if (actionNeeded[BUTTON_SM_3] == ACTION_SHORT_RELEASE) {
        actionNeeded[BUTTON_SM_3] = ACTION_NONE;
      }
    }
    // BUTTON_SM_4: Y button (toggles R3 in default mode)
    else if (i == BUTTON_SM_4) {
      if (!homeheld) {
        if (buttonStates[BUTTON_SM_4] == BUTTON_LONG_PRESSED) {
          if (!yToggleHandled) {
            yToggle = !yToggle;
            yToggleHandled = true;
          }
        } else {
          yToggleHandled = false;
          if (actionNeeded[BUTTON_SM_4] == ACTION_SHORT_PRESS) {
            ReportData.Button |= Y_MASK_ON;
            pressStartTime[BUTTON_SM_4] = millis();
            actionNeeded[BUTTON_SM_4] = ACTION_SHORT_PRESS_HOLDING;
          } else if (actionNeeded[BUTTON_SM_4] == ACTION_SHORT_PRESS_HOLDING) {
            ReportData.Button |= Y_MASK_ON;
            if (millis() - pressStartTime[BUTTON_SM_4] >= 255)
              actionNeeded[BUTTON_SM_4] = ACTION_SHORT_RELEASE;
          } else if (actionNeeded[BUTTON_SM_4] == ACTION_SHORT_RELEASE) {
            actionNeeded[BUTTON_SM_4] = ACTION_NONE;
          }
        }
      } else {
        // homeheld: long press => momentary R3, short press => Y
        if (buttonStates[BUTTON_SM_4] == BUTTON_LONG_PRESSED) {
          ReportData.Button |= R3_MASK_ON;
        } else {
          if (actionNeeded[BUTTON_SM_4] == ACTION_SHORT_PRESS) {
            ReportData.Button |= Y_MASK_ON;
            pressStartTime[BUTTON_SM_4] = millis();
            actionNeeded[BUTTON_SM_4] = ACTION_SHORT_PRESS_HOLDING;
          } else if (actionNeeded[BUTTON_SM_4] == ACTION_SHORT_PRESS_HOLDING) {
            ReportData.Button |= Y_MASK_ON;
            if (millis() - pressStartTime[BUTTON_SM_4] >= 255)
              actionNeeded[BUTTON_SM_4] = ACTION_SHORT_RELEASE;
          } else if (actionNeeded[BUTTON_SM_4] == ACTION_SHORT_RELEASE) {
            actionNeeded[BUTTON_SM_4] = ACTION_NONE;
          }
        }
      }
    }
    // SHIFT_SM_1: L button (toggles ZL in default mode)
    else if (i == SHIFT_SM_1) {
      if (!homeheld) {
        // Default mode: short press => toggle ZL, long press => LB
        if (buttonStates[SHIFT_SM_1] == BUTTON_LONG_PRESSED) {
          ReportData.Button |= LB_MASK_ON;
        } else {
          // short press => toggle ZL
          if (actionNeeded[SHIFT_SM_1] == ACTION_SHORT_PRESS) {
            if (!shift1ToggleHandled) {
              shift1ZLToggle = !shift1ZLToggle;
              shift1ToggleHandled = true;
            }
            actionNeeded[SHIFT_SM_1] = ACTION_NONE;
          } else if (buttonStates[SHIFT_SM_1] == BUTTON_IDLE) {
            shift1ToggleHandled = false;
          }
        }
      } else {
        // Homeheld: short press => momentary ZL, long press => LB
        if (buttonStates[SHIFT_SM_1] == BUTTON_LONG_PRESSED) {
          ReportData.Button |= LB_MASK_ON;
        } else {
          if (actionNeeded[SHIFT_SM_1] == ACTION_SHORT_PRESS) {
            ReportData.Button |= ZL_MASK_ON;
            pressStartTime[SHIFT_SM_1] = millis();
            actionNeeded[SHIFT_SM_1] = ACTION_SHORT_PRESS_HOLDING;
          } else if (actionNeeded[SHIFT_SM_1] == ACTION_SHORT_PRESS_HOLDING) {
            ReportData.Button |= ZL_MASK_ON;
            if (millis() - pressStartTime[SHIFT_SM_1] >= 255) {
              actionNeeded[SHIFT_SM_1] = ACTION_SHORT_RELEASE;
            }
          } else if (actionNeeded[SHIFT_SM_1] == ACTION_SHORT_RELEASE) {
            actionNeeded[SHIFT_SM_1] = ACTION_NONE;
          }
        }
      }
    }
    // SHIFT_SM_2: R button (toggles ZR in default mode)
    else if (i == SHIFT_SM_2) {
      if (!homeheld) {
        // Default mode: short press => toggle ZR, long press => RB
        if (buttonStates[SHIFT_SM_2] == BUTTON_LONG_PRESSED) {
          ReportData.Button |= RB_MASK_ON;
        } else {
          // short press => toggle ZR
          if (actionNeeded[SHIFT_SM_2] == ACTION_SHORT_PRESS) {
            if (!shift2ToggleHandled) {
              shift2ZRToggle = !shift2ZRToggle;
              shift2ToggleHandled = true;
            }
            actionNeeded[SHIFT_SM_2] = ACTION_NONE;
          } else if (buttonStates[SHIFT_SM_2] == BUTTON_IDLE) {
            shift2ToggleHandled = false;
          }
        }
      } else {
        // Homeheld: short press => momentary ZR, long press => RB
        if (buttonStates[SHIFT_SM_2] == BUTTON_LONG_PRESSED) {
          ReportData.Button |= RB_MASK_ON;
        } else {
          if (actionNeeded[SHIFT_SM_2] == ACTION_SHORT_PRESS) {
            ReportData.Button |= ZR_MASK_ON;
            pressStartTime[SHIFT_SM_2] = millis();
            actionNeeded[SHIFT_SM_2] = ACTION_SHORT_PRESS_HOLDING;
          } else if (actionNeeded[SHIFT_SM_2] == ACTION_SHORT_PRESS_HOLDING) {
            ReportData.Button |= ZR_MASK_ON;
            if (millis() - pressStartTime[SHIFT_SM_2] >= 255) {
              actionNeeded[SHIFT_SM_2] = ACTION_SHORT_RELEASE;
            }
          } else if (actionNeeded[SHIFT_SM_2] == ACTION_SHORT_RELEASE) {
            actionNeeded[SHIFT_SM_2] = ACTION_NONE;
          }
        }
      }
    }
    // BUTTON_SM_6: HOME button (short press = HOME, long press toggles homeheld)
    else if (i == BUTTON_SM_6) {
      if (buttonStates[BUTTON_SM_6] == BUTTON_LONG_PRESSED) {
        if (!homeheldToggleHandled) {
          homeheld = !homeheld;
          homeheldToggleHandled = true;
        }
      } else {
        homeheldToggleHandled = false;
        if (actionNeeded[BUTTON_SM_6] == ACTION_SHORT_PRESS) {
          ReportData.Button |= HOME_MASK_ON;
          pressStartTime[BUTTON_SM_6] = millis();
          actionNeeded[BUTTON_SM_6] = ACTION_SHORT_PRESS_HOLDING;
        } else if (actionNeeded[BUTTON_SM_6] == ACTION_SHORT_PRESS_HOLDING) {
          ReportData.Button |= HOME_MASK_ON;
          if (millis() - pressStartTime[BUTTON_SM_6] >= 255)
            actionNeeded[BUTTON_SM_6] = ACTION_SHORT_RELEASE;
        } else if (actionNeeded[BUTTON_SM_6] == ACTION_SHORT_RELEASE) {
          actionNeeded[BUTTON_SM_6] = ACTION_NONE;
        }
      }
    }
    // BUTTON_SM_7: Altmode toggle
    else if (i == BUTTON_SM_7) {
      if (actionNeeded[BUTTON_SM_7] == ACTION_SHORT_PRESS) {
        altmode = !altmode;
        pressStartTime[BUTTON_SM_7] = millis();
        actionNeeded[BUTTON_SM_7] = ACTION_SHORT_PRESS_HOLDING;
      } else if (actionNeeded[BUTTON_SM_7] == ACTION_SHORT_PRESS_HOLDING) {
        if (millis() - pressStartTime[BUTTON_SM_7] >= 255)
          actionNeeded[BUTTON_SM_7] = ACTION_SHORT_RELEASE;
      } else if (actionNeeded[BUTTON_SM_7] == ACTION_SHORT_RELEASE) {
        actionNeeded[BUTTON_SM_7] = ACTION_NONE;
      }
    }
  }

  // In default mode, we apply persistent toggles for ZL, ZR, L3, and R3
  if (!homeheld) {
    if (shift1ZLToggle) ReportData.Button |= ZL_MASK_ON;
    if (shift2ZRToggle) ReportData.Button |= ZR_MASK_ON;
    if (aToggle)        ReportData.Button |= L3_MASK_ON;
    if (yToggle)        ReportData.Button |= R3_MASK_ON;
  }

  // Show homeheld status on onboard LED
  digitalWrite(pinOBLED, homeheld ? HIGH : LOW);
}
