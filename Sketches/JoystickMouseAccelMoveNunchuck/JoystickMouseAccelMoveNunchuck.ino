// Include the libraries
#include <Keyboard.h>
#include <Mouse.h>
#include <NintendoExtensionCtrl.h>

// Create a Nunchuk object
Nunchuk nunchuk;

const int joyHighThreshold = 142;
const int joyLowThreshold = 114;
const int range = 5;             // Increased range for faster movement
const int responseDelay = 5;     // Reduced response delay for quicker reaction
const int joyDeadzone = 5;       // 5 percent deadzone for the joystick
const int accelDeadzone = 150;   // Deadzone to ignore small movements for the accelerometer
const int accelDeadzone2 = 80;   // Deadzone for Vertical values (downward tilt)
const float accelFactor = 0.05;  // Adjust this value to control sensitivity
const float exponentialFactor = 1.5; // Exponential factor for mouse movement

void setup() {
  Serial.begin(9600);
  Keyboard.begin();
  Mouse.begin();
  nunchuk.begin();
  while (!nunchuk.connect()) {
    Serial.println("Nunchuk not detected!");
    delay(1000);
  }
}

void loop() {
  boolean success = nunchuk.update();

  if (!success) {
    Serial.println("Controller disconnected!");
    delay(1000);
  } else {
    int clickStateC = nunchuk.buttonC();
    int clickStateZ = nunchuk.buttonZ();
    int joyX = nunchuk.joyX();
    int joyY = nunchuk.joyY();
    uint16_t accelX = nunchuk.accelX();
    uint16_t accelY = nunchuk.accelY();

    if (clickStateC == 0 && clickStateZ == 0) {
      handleMouseMovementWithJoystick(joyX, joyY);
    }
    handleKeyPressWithAccel(accelX, accelY);

    if (clickStateC == 1 && clickStateZ == 1) {
      handleKeyPressWithJoystick(joyX, joyY, 'z', 'x', 'c', 'v');
    } else if (clickStateC == 1) {
      handleKeyPressWithJoystick(joyX, joyY, 'j', 'k', 'h', 'l');
    } else if (clickStateZ == 1) {
      handleKeyPressWithJoystick(joyX, joyY, 'y', 'u', 'i', 'p');
    }

    handleButtonPresses(clickStateC, clickStateZ);
  }
}

void handleMouseMovementWithJoystick(int joyX, int joyY) {
  // Adjust joystick values to control mouse movements with exponential curve
  int xDistance = 0;
  int yDistance = 0;

  int joyXCentered = joyX - 128;
  int joyYCentered = joyY - 128;

  if (abs(joyXCentered) > (joyDeadzone * 2.55)) {
    xDistance = (int)(pow(abs(joyXCentered), exponentialFactor) * (joyXCentered > 0 ? 1 : -1));
    xDistance = map(xDistance, -pow(127, exponentialFactor), pow(127, exponentialFactor), -range, range);
  }

  if (abs(joyYCentered) > (joyDeadzone * 2.55)) {
    yDistance = (int)(pow(abs(joyYCentered), exponentialFactor) * (joyYCentered > 0 ? -1 : 1));
    yDistance = map(yDistance, -pow(127, exponentialFactor), pow(127, exponentialFactor), -range, range); // Inverted vertical movement
  }

  if (xDistance != 0 || yDistance != 0) {
    Mouse.move(xDistance, yDistance, 0);
    delay(responseDelay);
  }
}

void handleKeyPressWithJoystick(int joyX, int joyY, char upKey, char downKey, char leftKey, char rightKey) {
  if (joyY > joyHighThreshold) {
    Keyboard.press(upKey);
  } else {
    Keyboard.release(upKey);
  }

  if (joyY < joyLowThreshold) {
    Keyboard.press(downKey);
  } else {
    Keyboard.release(downKey);
  }

  if (joyX < joyLowThreshold) {
    Keyboard.press(leftKey);
  } else {
    Keyboard.release(leftKey);
  }

  if (joyX > joyHighThreshold) {
    Keyboard.press(rightKey);
  } else {
    Keyboard.release(rightKey);
  }
}

void handleKeyPressWithAccel(uint16_t accelX, uint16_t accelY) {
  // Center the accelerometer values
  int centeredX = accelX - 512;
  int centeredY = accelY - 512;

  // Handle horizontal movement
  if (centeredX > accelDeadzone) {
    Keyboard.press('f');  // Right movement
  } else if (centeredX < -accelDeadzone) {
    Keyboard.press('s');  // Left movement
  } else {
    Keyboard.release('f');
    Keyboard.release('s');
  }

  // Handle vertical movement with increased sensitivity for up direction
  if (centeredY > accelDeadzone2 * 0.8) {  // Increase sensitivity by reducing deadzone
    Keyboard.press('d');  // Up movement (Corrected to 'd')
  } else if (centeredY < -accelDeadzone) {
    Keyboard.press('e');  // Down movement (Corrected to 'e')
  } else {
    Keyboard.release('e');
    Keyboard.release('d');
  }
}

void handleButtonPresses(int clickStateC, int clickStateZ) {
}
