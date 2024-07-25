// Include the libraries
#include <Keyboard.h>
#include <Mouse.h>
#include <NintendoExtensionCtrl.h>

// Create a Nunchuk object
Nunchuk nunchuk;

// Define the states
typedef enum {
  ANALOG_MODE,
  DPAD_MODE
} State_t;
State_t state = ANALOG_MODE;

// Thresholds and constants
const int joyHighThreshold = 142;
const int joyLowThreshold = 114;
const int range = 5;
const int responseDelay = 10;
const int deadzoneAnalog = 100;
const int deadzoneDpad = 164;
const float accelFactorAnalog = 0.05;
const float accelFactorDpad = 0.15;

// Toggle state variables
unsigned long toggleStartTime = 0;
bool toggleInProgress = false;

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
    int sensorValueY = nunchuk.joyY();
    int sensorValueX = nunchuk.joyX();
    int clickStateC = nunchuk.buttonC();
    int clickStateZ = nunchuk.buttonZ();

    handleToggle(clickStateC, clickStateZ);

    switch (state) {
      case ANALOG_MODE:
        handleAnalogMode(sensorValueX, sensorValueY, clickStateC, clickStateZ);
        TXLED1;
        break;
      case DPAD_MODE:
        handleDpadMode(sensorValueX, sensorValueY, clickStateC, clickStateZ);
        TXLED0;
        break;
    }
  }
}

void handleToggle(int clickStateC, int clickStateZ) {
  if (clickStateC && clickStateZ) {
    if (!toggleInProgress) {
      toggleStartTime = millis();
      toggleInProgress = true;
    } else if (millis() - toggleStartTime >= 3000) {
      toggleMode();
      toggleInProgress = false;
    }
  } else {
    toggleInProgress = false;
  }
}

void toggleMode() {
  if (state == ANALOG_MODE) {
    state = DPAD_MODE;
  } else {
    state = ANALOG_MODE;
  }
  Serial.print("Mode changed to: ");
  Serial.println(state == ANALOG_MODE ? "ANALOG_MODE" : "DPAD_MODE");
}

// Functions for ANALOG_MODE
void handleAnalogMode(int joyX, int joyY, int clickState3, int clickState4) {
  handleMouseMovementWithAccel(nunchuk.accelX(), nunchuk.accelY(), accelFactorAnalog, deadzoneAnalog);
  handleKeyPressWithJoystick(joyX, joyY, clickState3, clickState4);
}

void handleMouseMovementWithAccel(uint16_t accelX, uint16_t accelY, float accelFactor, int deadzone) {
  int centeredX = accelX - 512;
  int centeredY = accelY - 512;

  int xDistance = (abs(centeredX) > deadzone) ? centeredX * accelFactor : 0;
  int yDistance = (abs(centeredY) > deadzone) ? centeredY * accelFactor : 0;

  if (xDistance != 0 || yDistance != 0) {
    Mouse.move(xDistance, yDistance, 0);
    delay(responseDelay);
  }
}

void handleKeyPressWithJoystick(int joyX, int joyY, int clickState3, int clickState4) {
  char upKey = 'e';
  char downKey = 'd';
  char leftKey = 's';
  char rightKey = 'f';

  if (clickState3 == 1) {
    upKey = 'k';
    downKey = 'j';
    leftKey = 'l';
    rightKey = 'm';
  }
  if (clickState4 == 1) {
    upKey = 'u';
    downKey = 'i';
    leftKey = 'y';
    rightKey = 'o';
  }
  if ((clickState3 == 1) && (clickState4 == 1)) {
    upKey = 'z';
    downKey = 'x';
    leftKey = 'c';
    rightKey = 'v';
  }

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
  if (joyX > joyHighThreshold) {
    Keyboard.press(rightKey);
  } else {
    Keyboard.release(rightKey);
  }
  if (joyX < joyLowThreshold) {
    Keyboard.press(leftKey);
  } else {
    Keyboard.release(leftKey);
  }
}

// Functions for DPAD_MODE
void handleDpadMode(int joyX, int joyY, int clickState3, int clickState4) {
  handleAccelKeyPress(nunchuk.accelX(), nunchuk.accelY(), accelFactorDpad, deadzoneDpad);

  if (clickState3 == 1 && clickState4 == 1) {
    handleKeyPress(joyX, joyY, 'z', 'x', 'c', 'v');
  } else if (clickState3 == 1) {
    handleKeyPress(joyX, joyY, 'h', 'j', 'k', 'l');
  } else if (clickState4 == 1) {
    handleKeyPress(joyX, joyY, 'y', 'u', 'i', 'p');
  } else {
    handleMouseMovement(joyX, joyY);
  }
}

void handleMouseMovement(int joyX, int joyY) {
  int xDistance = map(joyX, 0, 255, -range, range);
  int yDistance = map(joyY, 0, 255, -range, range);

  if (abs(xDistance) > 1) {
    Mouse.move(xDistance, 0, 0);
  }
  if (abs(yDistance) > 1) {
    Mouse.move(0, -yDistance, 0);
  }
  delay(responseDelay);
}

void handleAccelKeyPress(uint16_t accelX, uint16_t accelY, float accelFactor, int deadzone) {
  int centeredX = accelX - 512;
  int centeredY = accelY - 512;

  int xDistance = (abs(centeredX) > deadzone) ? centeredX * accelFactor : 0;
  int yDistance = (abs(centeredY) > deadzone) ? centeredY * accelFactor : 0;

  if (centeredY > deadzone) {
    Keyboard.press('d');
  } else {
    Keyboard.release('d');
  }
  if (centeredY < -deadzone) {
    Keyboard.press('e');
  } else {
    Keyboard.release('e');
  }
  if (centeredX > deadzone) {
    Keyboard.press('f');
  } else {
    Keyboard.release('f');
  }
  if (centeredX < -deadzone) {
    Keyboard.press('s');
  } else {
    Keyboard.release('s');
  }
}

void handleKeyPress(int joyX, int joyY, char upKey, char downKey, char leftKey, char rightKey) {
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
  if (joyX > joyHighThreshold) {
    Keyboard.press(rightKey);
  } else {
    Keyboard.release(rightKey);
  }
  if (joyX < joyLowThreshold) {
    Keyboard.press(leftKey);
  } else {
    Keyboard.release(leftKey);
  }
}
