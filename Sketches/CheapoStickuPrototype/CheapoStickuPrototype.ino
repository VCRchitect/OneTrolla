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

bool buttonStartBefore;
bool buttonSelectBefore;
byte buttonStatus[15];

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

#define BUTTON1 0
#define BUTTON2 1
#define BUTTON3 2
#define BUTTON4 3
#define BUTTON5 15
#define SHIFT1 4
#define SHIFT2 5

Bounce button1 = Bounce();
Bounce button2 = Bounce();
Bounce button3 = Bounce();
Bounce button4 = Bounce();
Bounce button5 = Bounce();
Bounce shift1 = Bounce();
Bounce shift2 = Bounce();

void setupPins() {
  button1.attach(4, INPUT_PULLUP);
  button2.attach(7, INPUT_PULLUP);
  button3.attach(9, INPUT_PULLUP);
  button4.attach(5, INPUT_PULLUP);
  button5.attach(15, INPUT_PULLUP);
  shift1.attach(6, INPUT_PULLUP);
  shift2.attach(8, INPUT_PULLUP);


  button1.interval(MILLIDEBOUNCE);
  button2.interval(MILLIDEBOUNCE);
  button3.interval(MILLIDEBOUNCE);
  button4.interval(MILLIDEBOUNCE);
  button5.interval(MILLIDEBOUNCE);
  shift1.interval(MILLIDEBOUNCE);
  shift2.interval(MILLIDEBOUNCE);

  pinMode(pinOBLED, OUTPUT);
  pinMode(0, INPUT_PULLUP);  // Set pin 0 for altmode
  pinMode(1, INPUT_PULLUP);  // Set pin 1 for altmode
  digitalWrite(pinOBLED, HIGH);
}

void setup() {
  buttonStartBefore = false;
  buttonSelectBefore = false;
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
  if (digitalRead(0) == LOW) {
    altmode = true;
  }
  if (digitalRead(1) == LOW) {
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
  if (abs(xValue - 512) < deadzone) { // Assuming 512 is the center value for the analog stick
    xValue = 512; // Set to neutral position
  }

  // Apply deadzone to yValue
  if (abs(yValue - 512) < deadzone) { // Assuming 512 is the center value for the analog stick
    yValue = 512; // Set to neutral position
  }

  // Check if button5 is pressed
  if (button5.read() == LOW) {  // If button5 is pressed
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
  const int16_t deadzoneThreshold = 9500;

  // Example: Adjust for 90-degree rotation around the Y-axis
  // Adjust the axes as follows:
  //   - X-axis (ax) -> new X-axis (az)
  //   - Z-axis (az) -> new inverted Y-axis (-ax)
  // Swapping and adjusting signs based on the new orientation.

  int16_t adjustedX = ax;  // New X-axis becomes the previous Z-axis
  int16_t adjustedY = az;  // New Y-axis becomes the inverted X-axis

  // Map the accelerometer values to joystick range (0-255) with deadzone
  if (altmode) {
    // Control right stick (RX, RY) when altmode is true
    if (abs(adjustedX) > deadzoneThreshold) {
      ReportData.RX = map(constrain(adjustedX, -17000, 17000), -17000, 17000, 0, 255);
    } else {
      ReportData.RX = 128;  // Center position in the deadzone
    }

    if (abs(adjustedY) > deadzoneThreshold) {
      ReportData.RY = map(constrain(adjustedY, -17000, 17000), -17000, 17000, 0, 255);
    } else {
      ReportData.RY = 128;  // Center position in the deadzone
    }
  } else {
    // Control left stick (LX, LY) when altmode is false
    if (abs(adjustedX) > deadzoneThreshold) {
      ReportData.LX = map(constrain(adjustedX, -17000, 17000), -17000, 17000, 0, 255);
    } else {
      ReportData.LX = 128;  // Center position in the deadzone
    }

    if (abs(adjustedY) > deadzoneThreshold) {
      ReportData.LY = map(constrain(adjustedY, -17000, 17000), -17000, 17000, 0, 255);
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
}

void processButtons() {
  // Clear the previous button states in ReportData
  ReportData.Button = 0;

  if (buttonStatus[SHIFT1] == LOW) {
    // Shift 1 is active
    if (buttonStatus[BUTTON1] == LOW) ReportData.Button |= ZL_MASK_ON;
    if (buttonStatus[BUTTON2] == LOW) ReportData.Button |= ZR_MASK_ON;
    if (buttonStatus[BUTTON3] == LOW) ReportData.Button |= LB_MASK_ON;
    if (buttonStatus[BUTTON4] == LOW) ReportData.Button |= RB_MASK_ON;
  } else if (buttonStatus[SHIFT2] == LOW) {
    // Shift 2 is active
    if (buttonStatus[BUTTON1] == LOW) ReportData.Button |= L3_MASK_ON;
    if (buttonStatus[BUTTON2] == LOW) ReportData.Button |= R3_MASK_ON;
    if (buttonStatus[BUTTON3] == LOW) ReportData.Button |= START_MASK_ON;
    if (buttonStatus[BUTTON4] == LOW) ReportData.Button |= SELECT_MASK_ON;
  } else {
    // No shift, primary function
    if (buttonStatus[BUTTON1] == LOW) ReportData.Button |= A_MASK_ON;
    if (buttonStatus[BUTTON2] == LOW) ReportData.Button |= B_MASK_ON;
    if (buttonStatus[BUTTON3] == LOW) ReportData.Button |= X_MASK_ON;
    if (buttonStatus[BUTTON4] == LOW) ReportData.Button |= Y_MASK_ON;
  }
}
