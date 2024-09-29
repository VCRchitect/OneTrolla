#include "LUFAConfig.h"
#include <LUFA.h>
#include "Joystick.h"
#include <Wire.h>
#include <MPU6050.h>
#define BOUNCE_WITH_PROMPT_DETECTION
#include <Bounce2.h>

#define MILLIDEBOUNCE 1  // Debounce time in milliseconds
#define pinOBLED 17      // Onboard LED pin

#define joyX A0
#define joyY A1

MPU6050 mpu;           // Create an instance of the MPU6050
bool altmode = false;  // Define the altmode variable

byte buttonStatus[7];  // Adjusted size to match the number of buttons

// Define masks for the various button states
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

// Button indices for buttonStatus array
#define BUTTON1 0
#define BUTTON2 1
#define BUTTON3 2
#define BUTTON4 3
#define BUTTON5 4
#define SHIFT1 5
#define SHIFT2 6

// Define pins for buttons and shifts
#define PIN_BUTTON1 4
#define PIN_BUTTON2 7
#define PIN_BUTTON3 9
#define PIN_BUTTON4 5
#define PIN_BUTTON5 15
#define PIN_SHIFT1 8
#define PIN_SHIFT2 6
#define PIN_ALT_MODE_0 0
#define PIN_ALT_MODE_1 1

Bounce button1 = Bounce();
Bounce button2 = Bounce();
Bounce button3 = Bounce();
Bounce button4 = Bounce();
Bounce button5 = Bounce();
Bounce shift1 = Bounce();
Bounce shift2 = Bounce();

// State machine indices for BUTTON1 to BUTTON4 and SHIFT1 to SHIFT2
#define BUTTON_SM_1 0
#define BUTTON_SM_2 1
#define BUTTON_SM_3 2
#define BUTTON_SM_4 3
#define SHIFT_SM_1 4
#define SHIFT_SM_2 5

// State machine for buttons
enum ButtonState { BUTTON_IDLE,
                   BUTTON_PRESSED,
                   BUTTON_LONG_PRESSED };
ButtonState buttonStates[6] = { BUTTON_IDLE, BUTTON_IDLE, BUTTON_IDLE, BUTTON_IDLE, BUTTON_IDLE, BUTTON_IDLE };

enum ButtonAction { ACTION_NONE,
                    ACTION_SHORT_PRESS,
                    ACTION_SHORT_PRESS_HOLDING,
                    ACTION_SHORT_RELEASE };
ButtonAction actionNeeded[6] = { ACTION_NONE, ACTION_NONE, ACTION_NONE, ACTION_NONE, ACTION_NONE, ACTION_NONE };

unsigned long buttonPressStartTime[6] = { 0, 0, 0, 0, 0, 0 };
unsigned long pressStartTime[6] = { 0, 0, 0, 0, 0, 0 };

void setupPins() {
  button1.attach(PIN_BUTTON1, INPUT_PULLUP);
  button2.attach(PIN_BUTTON2, INPUT_PULLUP);
  button3.attach(PIN_BUTTON3, INPUT_PULLUP);
  button4.attach(PIN_BUTTON4, INPUT_PULLUP);
  button5.attach(PIN_BUTTON5, INPUT_PULLUP);
  shift1.attach(PIN_SHIFT1, INPUT_PULLUP);
  shift2.attach(PIN_SHIFT2, INPUT_PULLUP);

  button1.interval(MILLIDEBOUNCE);
  button2.interval(MILLIDEBOUNCE);
  button3.interval(MILLIDEBOUNCE);
  button4.interval(MILLIDEBOUNCE);
  button5.interval(MILLIDEBOUNCE);
  shift1.interval(MILLIDEBOUNCE);
  shift2.interval(MILLIDEBOUNCE);

  pinMode(pinOBLED, OUTPUT);
  pinMode(PIN_ALT_MODE_0, INPUT_PULLUP);  // Set pin for altmode
  pinMode(PIN_ALT_MODE_1, INPUT_PULLUP);  // Set pin for altmode
  digitalWrite(pinOBLED, HIGH);
}

void setup() {
  setupPins();

  // Initialize MPU6050
  Wire.begin();
  mpu.initialize();

  if (!mpu.testConnection()) {
    // Connection failed, handle error
    while (true) {
      // Blink onboard LED to indicate error
      digitalWrite(pinOBLED, !digitalRead(pinOBLED));
      delay(500);
    }
  }

  SetupHardware();
  GlobalInterruptEnable();
}

void loop() {
  // Check altmode status
  if (digitalRead(PIN_ALT_MODE_0) == LOW) {
    altmode = true;
  } else if (digitalRead(PIN_ALT_MODE_1) == LOW) {
    altmode = false;
  }

  readAnalogStick();
  readMPU6050();  // Read accelerometer data
  readButtons();
  processButtons();
  HID_Task();
  USB_USBTask();
}


void readAnalogStick() {
  // Define deadzone threshold
  const int deadzone = 15;

  // Read analog values
  int xValue = analogRead(joyX);
  int yValue = analogRead(joyY);

  // Apply deadzone to xValue
  if (abs(xValue - 512) < deadzone) {  // Assuming 512 is the center value for the analog stick
    xValue = 512;                      // Set to neutral position
  }

  // Apply deadzone to yValue
  if (abs(yValue - 512) < deadzone) {  // Assuming 512 is the center value for the analog stick
    yValue = 512;                      // Set to neutral position
  }

  // Check if BUTTON5 is pressed
  if (buttonStatus[BUTTON5] == LOW) {  // If BUTTON5 is pressed
    // Map analog stick values to DPAD with inverted left/right
    if (xValue < 341) {    // Now this is Right
      if (yValue < 341) {  // Up-Right
        ReportData.HAT = DPAD_UPRIGHT_MASK_ON;
      } else if (yValue > 682) {  // Down-Right
        ReportData.HAT = DPAD_DOWNRIGHT_MASK_ON;
      } else {  // Straight Right
        ReportData.HAT = DPAD_RIGHT_MASK_ON;
      }
    } else if (xValue > 682) {  // Now this is Left
      if (yValue < 341) {       // Up-Left
        ReportData.HAT = DPAD_UPLEFT_MASK_ON;
      } else if (yValue > 682) {  // Down-Left
        ReportData.HAT = DPAD_DOWNLEFT_MASK_ON;
      } else {  // Straight Left
        ReportData.HAT = DPAD_LEFT_MASK_ON;
      }
    } else {               // Center X-axis
      if (yValue < 341) {  // Up
        ReportData.HAT = DPAD_UP_MASK_ON;
      } else if (yValue > 682) {  // Down
        ReportData.HAT = DPAD_DOWN_MASK_ON;
      } else {  // Neutral
        ReportData.HAT = DPAD_NOTHING_MASK_ON;
      }
    }

    // Center the specified joystick axes based on altmode
    if (altmode || buttonStatus[BUTTON5] == LOW) {
      // If altmode is true, center LX and LY
      ReportData.LX = 128;
      ReportData.LY = 128;
    } else {
      // If altmode is false, center RX and RY
      ReportData.RX = 128;
      ReportData.RY = 128;
    }
  } else {
    ReportData.HAT = DPAD_NOTHING_MASK_ON;

    // Map analog stick to standard functionality
    if (altmode) {
      // Map to LX and LY when altmode is true
      ReportData.LX = map(xValue, 0, 1023, 255, 0);  // Flipping directions
      ReportData.LY = map(yValue, 0, 1023, 0, 255);
    } else {
      // Map to RX and RY when altmode is false
      ReportData.RX = map(xValue, 0, 1023, 255, 0);  // Flipping directions
      ReportData.RY = map(yValue, 0, 1023, 0, 255);
    }
  }
}

void readMPU6050() {
  // Get raw accelerometer data
  int16_t ax, ay, az;
  mpu.getAcceleration(&ax, &ay, &az);

  // Define deadzone threshold
  const int16_t deadzoneThreshold = 6500;

  int16_t adjustedX = ax;  // Adjusted X-axis
  int16_t adjustedY = az;  // Adjusted Y-axis

  // Initialize static variables for smoothing
  static float smoothedX = adjustedX;
  static float smoothedY = adjustedY;

  // Smoothing factor (alpha), between 0 (no smoothing) and 1 (maximum smoothing)
  const float alpha = 0.1;  // You can adjust this value as needed

  // Apply exponential smoothing
  smoothedX = alpha * adjustedX + (1 - alpha) * smoothedX;
  smoothedY = alpha * adjustedY + (1 - alpha) * smoothedY;

  // Map the smoothed accelerometer values to joystick range (0-255) with deadzone
  if (altmode || buttonStatus[BUTTON5] == LOW) {
    // Control right stick (RX, RY) when altmode is true
    if (abs(smoothedX) > deadzoneThreshold) {
      ReportData.RX = map(constrain((int32_t)smoothedX, -10000, 10000), -10000, 10000, 0, 255);
    } else {
      ReportData.RX = 128;  // Center position in the deadzone
    }

    if (abs(smoothedY) > deadzoneThreshold) {
      ReportData.RY = map(constrain((int32_t)smoothedY, -10000, 10000), -10000, 10000, 0, 255);
    } else {
      ReportData.RY = 128;  // Center position in the deadzone
    }
  } else {
    // Control left stick (LX, LY) when altmode is false
    if (abs(smoothedX) > deadzoneThreshold) {
      ReportData.LX = map(constrain((int32_t)smoothedX, -10000, 10000), -10000, 10000, 0, 255);
    } else {
      ReportData.LX = 128;  // Center position in the deadzone
    }

    if (abs(smoothedY) > deadzoneThreshold) {
      ReportData.LY = map(constrain((int32_t)smoothedY, -10000, 10000), -10000, 10000, 0, 255);
    } else {
      ReportData.LY = 128;  // Center position in the deadzone
    }
  }
}

void readButtons() {
  // Update the button states with debouncing
  button1.update();
  button2.update();
  button3.update();
  button4.update();
  button5.update();
  shift1.update();
  shift2.update();

  // Update the buttonStatus array
  buttonStatus[BUTTON1] = button1.read();
  buttonStatus[BUTTON2] = button2.read();
  buttonStatus[BUTTON3] = button3.read();
  buttonStatus[BUTTON4] = button4.read();
  buttonStatus[BUTTON5] = button5.read();
  buttonStatus[SHIFT1] = shift1.read();
  buttonStatus[SHIFT2] = shift2.read();

  // Handle button states for BUTTON1-4 and SHIFT1-2
  handleButtonPress(button1, BUTTON_SM_1);
  handleButtonPress(button2, BUTTON_SM_2);
  handleButtonPress(button3, BUTTON_SM_3);
  handleButtonPress(button4, BUTTON_SM_4);
  handleButtonPress(shift1, SHIFT_SM_1);
  handleButtonPress(shift2, SHIFT_SM_2);
}

void handleButtonPress(Bounce& button, int idx) {
  switch (buttonStates[idx]) {
    case BUTTON_IDLE:
      if (button.fell()) {
        // Button just pressed
        buttonPressStartTime[idx] = millis();
        buttonStates[idx] = BUTTON_PRESSED;
      }
      break;
    case BUTTON_PRESSED:
      if (button.read() == LOW) {
        // Button is still pressed
        if (millis() - buttonPressStartTime[idx] >= 500) {  // Long press threshold
          buttonStates[idx] = BUTTON_LONG_PRESSED;
        }
      } else if (button.rose()) {
        // Button released before long press threshold
        buttonStates[idx] = BUTTON_IDLE;
        actionNeeded[idx] = ACTION_SHORT_PRESS;  // Need to send short press action
      }
      break;
    case BUTTON_LONG_PRESSED:
      if (button.read() == HIGH) {
        // Button released after long press
        buttonStates[idx] = BUTTON_IDLE;
      }
      break;
  }
}

void processButtons() {
  // Clear the previous button states in ReportData
  ReportData.Button = 0;

  // Handle BUTTON1-4 and SHIFT1-2
  for (int i = BUTTON_SM_1; i <= SHIFT_SM_2; i++) {
    if (buttonStates[i] == BUTTON_LONG_PRESSED) {
      // Button is held down after long press threshold
      // Send long press action
      switch (i) {
        case BUTTON_SM_1:
          ReportData.Button |= L3_MASK_ON;
          break;
        case BUTTON_SM_2:
          ReportData.Button |= R3_MASK_ON;
          break;
        case BUTTON_SM_3:
          ReportData.Button |= START_MASK_ON;
          break;
        case BUTTON_SM_4:
          ReportData.Button |= SELECT_MASK_ON;
          break;
        case SHIFT_SM_1:
          ReportData.Button |= ZL_MASK_ON;  // Long press of SHIFT1 is ZL
          break;
        case SHIFT_SM_2:
          ReportData.Button |= RB_MASK_ON;  // Long press of SHIFT2 is RB
          break;
      }
    } else if (actionNeeded[i] == ACTION_SHORT_PRESS) {
      // Start short press action
      switch (i) {
        case BUTTON_SM_1:
          ReportData.Button |= A_MASK_ON;
          break;
        case BUTTON_SM_2:
          ReportData.Button |= B_MASK_ON;
          break;
        case BUTTON_SM_3:
          ReportData.Button |= X_MASK_ON;
          break;
        case BUTTON_SM_4:
          ReportData.Button |= Y_MASK_ON;
          break;
        case SHIFT_SM_1:
          ReportData.Button |= LB_MASK_ON;  // Short press of SHIFT1 is LB
          break;
        case SHIFT_SM_2:
          ReportData.Button |= ZR_MASK_ON;  // Short press of SHIFT2 is ZR
          break;
      }
      // Record the time when we started the press
      pressStartTime[i] = millis();
      actionNeeded[i] = ACTION_SHORT_PRESS_HOLDING;
    } else if (actionNeeded[i] == ACTION_SHORT_PRESS_HOLDING) {
      // Continue to hold the button down
      switch (i) {
        case BUTTON_SM_1:
          ReportData.Button |= A_MASK_ON;
          break;
        case BUTTON_SM_2:
          ReportData.Button |= B_MASK_ON;
          break;
        case BUTTON_SM_3:
          ReportData.Button |= X_MASK_ON;
          break;
        case BUTTON_SM_4:
          ReportData.Button |= Y_MASK_ON;
          break;
        case SHIFT_SM_1:
          ReportData.Button |= LB_MASK_ON;  // Short press of SHIFT1 is LB
          break;
        case SHIFT_SM_2:
          ReportData.Button |= ZR_MASK_ON;  // Short press of SHIFT2 is ZR
          break;
      }
      // Check if we have held the button for the minimum duration
      if (millis() - pressStartTime[i] >= 150) {  // 150 ms minimum press duration
        actionNeeded[i] = ACTION_SHORT_RELEASE;
      }
    } else if (actionNeeded[i] == ACTION_SHORT_RELEASE) {
      // Release the button by not setting any button mask
      actionNeeded[i] = ACTION_NONE;
    }
  }

  // Handle BUTTON5 and DPAD (Unchanged logic)
  if (buttonStatus[BUTTON5] == LOW) {
    // Your existing BUTTON5 logic for the DPAD
  }
}