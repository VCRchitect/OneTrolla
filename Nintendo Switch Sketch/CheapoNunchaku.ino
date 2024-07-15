#include <NintendoExtensionCtrl.h>
#include "LUFAConfig.h"
#include <LUFA.h>
#include "Joystick.h"
#define BOUNCE_WITH_PROMPT_DETECTION
#include <Bounce2.h>

#define MILLIDEBOUNCE 1           // Debounce time in milliseconds
#define DOUBLE_TAP_THRESHOLD 250  // Time in milliseconds for double-tap detection

#define DPAD_UP_MASK_ON 0x00
#define DPAD_UPRIGHT_MASK_ON 0x01
#define DPAD_RIGHT_MASK_ON 0x02
#define DPAD_DOWNRIGHT_MASK_ON 0x03
#define DPAD_DOWN_MASK_ON 0x04
#define DPAD_DOWNLEFT_MASK_ON 0x05
#define DPAD_LEFT_MASK_ON 0x06
#define DPAD_UPLEFT_MASK_ON 0x07
#define DPAD_NOTHING_MASK_ON 0x08
#define A_MASK_ON 0x04
#define B_MASK_ON 0x02
#define X_MASK_ON 0x08
#define Y_MASK_ON 0x01
#define LB_MASK_ON 0x10
#define RB_MASK_ON 0x20
#define ZL_MASK_ON 0x40
#define ZR_MASK_ON 0x80
#define START_MASK_ON 0x200
#define SELECT_MASK_ON 0x100
#define HOME_MASK_ON 0x1000
#define CAPTURE_MASK_ON 0x2000
#define R3_MASK_ON 0x800
#define L3_MASK_ON 0x400

// Pin definitions
const uint8_t buttonPins[] = {
  0, 1, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
  22, 23, 24, 25, 26, 27
};

// Instantiate Bounce objects
Bounce bounceObjects[sizeof(buttonPins) / sizeof(buttonPins[0])];

byte buttonStatus[28];

typedef enum {
  ANALOG_MODE,
  DPAD_MODE
} State_t;
State_t state = ANALOG_MODE;

Nunchuk nunchuk;

const int accelThreshold = 350;     // Increase sensitivity by lowering threshold
const int accelNegThreshold = 350;  // Increase sensitivity by lowering threshold
unsigned long toggleStartTime = 0;
bool toggleInProgress = false;

bool ltToggled = false;
bool rtToggled = false;
bool l3Toggled = false;
bool r3Toggled = false;


void setupPins() {
  // Attach and set debounce intervals for Bounce objects
  for (uint8_t i = 0; i < sizeof(buttonPins) / sizeof(buttonPins[0]); ++i) {
    bounceObjects[i].attach(buttonPins[i], INPUT_PULLUP);
    bounceObjects[i].interval(MILLIDEBOUNCE);
  }
}

void setup() {
  ReportData.RX = 128;
  ReportData.RY = 128;

  Serial1.begin(9600);
  nunchuk.begin();
  while (!nunchuk.connect()) {
    Serial1.println("Nunchuk not detected!");
    delay(1000);
  }
  setupPins();
  SetupHardware();
  GlobalInterruptEnable();

  // Initialize buttonStatus array
  memset(buttonStatus, LOW, sizeof(buttonStatus));
}

void loop() {
  if (!nunchuk.update()) {
    Serial1.println("Controller disconnected!");
    delay(1000);
    return;
  }

  handleAccel1(nunchuk.accelY());
  handleAccel2(nunchuk.accelX());
  handleAccel3(nunchuk.accelY(), nunchuk.joyY(), nunchuk.buttonC(), nunchuk.buttonZ());
//  handleAccel4(nunchuk.accelY(), nunchuk.joyY(), nunchuk.buttonC(), nunchuk.buttonZ());
  handleAccel5(nunchuk.accelX(), nunchuk.joyX(), nunchuk.buttonC(), nunchuk.buttonZ());
//  handleAccel6(nunchuk.accelX(), nunchuk.joyX(), nunchuk.buttonC(), nunchuk.buttonZ());



  int sensorValueY = nunchuk.joyY();
  int sensorValueX = nunchuk.joyX();  // 0 - 255
  int clickStateC = nunchuk.buttonC();
  int clickStateZ = nunchuk.buttonZ();

  // Toggle mode handling
  if (clickStateC && clickStateZ) {
    if (!toggleInProgress) {
      toggleStartTime = millis();
      toggleInProgress = true;
    } else if (millis() - toggleStartTime >= 1500) {
      toggleMode();
      toggleInProgress = false;
    }
  } else {
    toggleInProgress = false;
  }

  // Update button status based on sensor values and click states
  updateButtonStatus(sensorValueY, sensorValueX, clickStateC, clickStateZ);

  processButtons();
  HID_Task();
  USB_USBTask();
}

void toggleMode() {
  state = (state == ANALOG_MODE) ? DPAD_MODE : ANALOG_MODE;  // Toggle the state
  if (state == ANALOG_MODE) {
    TXLED1;  // Turn off LED in ANALOG_MODE
  } else {
    TXLED0;  // Turn on LED in DPAD_MODE
  }
}

void updateButtonStatus(int sensorValueY, int sensorValueX, int clickStateC, int clickStateZ) {
  if (state == ANALOG_MODE) {
    buttonStatus[0] = (sensorValueY > 142 && !clickStateC && !clickStateZ) ? HIGH : LOW;  // BUTTONUP
    buttonStatus[1] = (sensorValueY < 114 && !clickStateC && !clickStateZ) ? HIGH : LOW;  // BUTTONDOWN
    buttonStatus[5] = (sensorValueX > 142 && !clickStateC && !clickStateZ) ? HIGH : LOW;  // BUTTONRIGHT
    buttonStatus[4] = (sensorValueX < 114 && !clickStateC && !clickStateZ) ? HIGH : LOW;  // BUTTONLEFT
  } else {                                                                                // DPAD_MODE
    buttonStatus[6] = (sensorValueY > 142 && !clickStateC && !clickStateZ) ? HIGH : LOW;  // BUTTONUP
    buttonStatus[7] = (sensorValueY < 114 && !clickStateC && !clickStateZ) ? HIGH : LOW;  // BUTTONDOWN
    buttonStatus[9] = (sensorValueX > 142 && !clickStateC && !clickStateZ) ? HIGH : LOW;  // BUTTONRIGHT
    buttonStatus[8] = (sensorValueX < 114 && !clickStateC && !clickStateZ) ? HIGH : LOW;  // BUTTONLEFT
  }

  // Toggle LT
  if (sensorValueY > 142 && clickStateZ && !clickStateC) {
    if (!ltToggled) {
      ltToggled = true;
      buttonStatus[20] = !buttonStatus[20];  // Toggle BUTTONLT
    }
  } else {
    ltToggled = false;
  }

  // Toggle RT
  if (sensorValueY < 114 && clickStateZ && !clickStateC) {
    if (!rtToggled) {
      rtToggled = true;
      buttonStatus[21] = !buttonStatus[21];  // Toggle BUTTONRT
    }
  } else {
    rtToggled = false;
  }

  if (sensorValueX > 142 && clickStateZ && clickStateC) {
    if (!l3Toggled) {
      l3Toggled = true;
      buttonStatus[25] = !buttonStatus[25];  // Toggle BUTTONL3
    }
  } else {
    l3Toggled = false;
  }

  // Toggle RT
  if (sensorValueX < 114 && clickStateZ && clickStateC) {
    if (!r3Toggled) {
      r3Toggled = true;
      buttonStatus[26] = !buttonStatus[26];  // Toggle BUTTONR3
    }
  } else {
    r3Toggled = false;
  }
  buttonStatus[17] = (sensorValueX < 114 && clickStateC && !clickStateZ) ? HIGH : LOW;  // BUTTONY
  buttonStatus[16] = (sensorValueY > 142 && clickStateC && !clickStateZ) ? HIGH : LOW;  // BUTTONX
  buttonStatus[19] = (sensorValueX > 142 && clickStateZ && !clickStateC) ? HIGH : LOW;  // BUTTONRB
  buttonStatus[18] = (sensorValueX < 114 && clickStateZ && !clickStateC) ? HIGH : LOW;  // BUTTONLB
  buttonStatus[22] = (sensorValueY > 142 && clickStateZ && clickStateC) ? HIGH : LOW;  // BUTTONSTART
  buttonStatus[23] = (sensorValueY < 114 && clickStateZ && clickStateC) ? HIGH : LOW;  // BUTTONSELECT
}

#define DEADZONE 100         // Define the size of the deadzone
#define SMOOTHING_WINDOW 25  // Number of samples for moving average
uint16_t accelYValues[SMOOTHING_WINDOW] = { 0 };
uint16_t accelXValues[SMOOTHING_WINDOW] = { 0 };
uint8_t index = 0;

void handleAccel1(uint16_t aY) {
  accelYValues[index] = aY;
  uint32_t sum = 0;
  for (int i = 0; i < SMOOTHING_WINDOW; i++) {
    sum += accelYValues[i];
  }
  uint16_t smoothedY = sum / SMOOTHING_WINDOW;

  if (abs(smoothedY - 512) < DEADZONE) {
    ReportData.RY = 128;  // Center value
  } else {
    ReportData.RY = map(smoothedY, 256, 768, 0, 255);
  }
}

void handleAccel3(uint16_t aY, int sensorValueY, int clickStateC, int clickStateZ) {
  const uint16_t flickRangeLow3 = 225;           // Adjusted lower bound of flick range
  const uint16_t flickRangeHigh3 = 350;          // Adjusted upper bound of flick range
  const unsigned long flickDurationLimit = 250;  // 500 milliseconds
  static unsigned long flickStartTime = 0;       // To track the start time of the flick

  // Check if the value falls within the flick range after being below the threshold
  if (aY >= flickRangeLow3 && aY <= flickRangeHigh3) {
    if (flickStartTime == 0) {  // If flick just started
      flickStartTime = millis();
    }
    if (millis() - flickStartTime < flickDurationLimit) {
      buttonStatus[15] = HIGH;  // Press Button15 if within flick range for less than 500 ms
    } else {
      buttonStatus[15] = LOW;  // Release Button15 if exceeding 500 ms
    }
  } else {
    flickStartTime = 0;      // Reset flick start time if outside the flick range
    buttonStatus[15] = LOW;  // Release Button15
  }

  // Additional condition for sensor value and click state
  if (sensorValueY < 114 && clickStateC && !clickStateZ) {
    buttonStatus[15] = HIGH;
  }
}

void handleAccel4(uint16_t aY, int sensorValueY, int clickStateC, int clickStateZ) {
  const uint16_t flickRangeLow4 = 256;   // Adjusted lower bound of flick range
  const uint16_t flickRangeHigh4 = 356;  // Adjusted upper bound of flick range

  // Check if the value falls within the flick range after being below the threshold
  if (aY >= flickRangeLow4 && aY <= flickRangeHigh4) {
    buttonStatus[16] = HIGH;  // Press Button15
  } else {
    buttonStatus[16] = LOW;  // Release Button15
  }

  // Additional condition for sensor value and click state
  if (sensorValueY > 142 && clickStateC && !clickStateZ) {
    buttonStatus[16] = HIGH;
  }
}

void handleAccel5(uint16_t aX, int sensorValueX, int clickStateC, int clickStateZ) {
  const uint16_t flickRangeLow5 = 725;           // Adjusted lower bound of flick range
  const uint16_t flickRangeHigh5 = 850;          // Adjusted upper bound of flick range
  const unsigned long flickDurationLimit = 250;  // 500 milliseconds
  static unsigned long flickStartTime = 0;       // To track the start time of the flick

  // Check if the value falls within the flick range after being below the threshold
  if (aX >= flickRangeLow5 && aX <= flickRangeHigh5) {
    if (flickStartTime == 0) {  // If flick just started
      flickStartTime = millis();
    }
    if (millis() - flickStartTime < flickDurationLimit) {
      buttonStatus[14] = HIGH;  // Press Button15 if within flick range for less than 500 ms
    } else {
      buttonStatus[14] = LOW;  // Release Button15 if exceeding 500 ms
    }
  } else {
    flickStartTime = 0;      // Reset flick start time if outside the flick range
    buttonStatus[14] = LOW;  // Release Button15
  }

  // Additional condition for sensor value and click state
  if (sensorValueX > 142 && clickStateC && !clickStateZ) {
    buttonStatus[14] = HIGH;
  }
}

void handleAccel6(uint16_t aX, int sensorValueX, int clickStateC, int clickStateZ) {
  const uint16_t flickRangeLow6 = 256;   // Adjusted lower bound of flick range
  const uint16_t flickRangeHigh6 = 356;  // Adjusted upper bound of flick range

  // Check if the value falls within the flick range after being below the threshold
  if (aX >= flickRangeLow6 && aX <= flickRangeHigh6) {
    buttonStatus[17] = HIGH;  // Press Button15
  } else {
    buttonStatus[17] = LOW;  // Release Button15
  }

  // Additional condition for sensor value and click state
  if (sensorValueX < 114 && clickStateC && !clickStateZ) {
    buttonStatus[17] = HIGH;
  }
}


void handleAccel2(uint16_t aX) {
  accelXValues[index] = aX;
  uint32_t sum = 0;
  for (int i = 0; i < SMOOTHING_WINDOW; i++) {
    sum += accelXValues[i];
  }
  uint16_t smoothedX = sum / SMOOTHING_WINDOW;

  if (abs(smoothedX - 512) < DEADZONE) {
    ReportData.RX = 128;  // Center value
  } else {
    ReportData.RX = map(smoothedX, 256, 768, 0, 255);
  }

  index = (index + 1) % SMOOTHING_WINDOW;
}

void processLANALOG() {
  ReportData.HAT = DPAD_NOTHING_MASK_ON;
  processAnalogStick();
}

void processAnalogStick() {
  if (buttonStatus[0] && buttonStatus[5]) {  // BUTTONUP & BUTTONRIGHT
    ReportData.LY = 0;
    ReportData.LX = 255;
  } else if (buttonStatus[1] && buttonStatus[5]) {  // BUTTONDOWN & BUTTONRIGHT
    ReportData.LY = 255;
    ReportData.LX = 255;
  } else if (buttonStatus[1] && buttonStatus[4]) {  // BUTTONDOWN & BUTTONLEFT
    ReportData.LY = 255;
    ReportData.LX = 0;
  } else if (buttonStatus[0] && buttonStatus[4]) {  // BUTTONUP & BUTTONLEFT
    ReportData.LY = 0;
    ReportData.LX = 0;
  } else if (buttonStatus[0]) {  // BUTTONUP
    ReportData.LY = 0;
    ReportData.LX = 128;
  } else if (buttonStatus[1]) {  // BUTTONDOWN
    ReportData.LY = 255;
    ReportData.LX = 128;
  } else if (buttonStatus[4]) {  // BUTTONLEFT
    ReportData.LX = 0;
    ReportData.LY = 128;
  } else if (buttonStatus[5]) {  // BUTTONRIGHT
    ReportData.LX = 255;
    ReportData.LY = 128;
  } else {
    ReportData.LX = 128;
    ReportData.LY = 128;
  }
}

void processDPad() {
  ReportData.LX = 128;
  ReportData.LY = 128;
  // Reset HAT state to nothing
  ReportData.HAT = DPAD_NOTHING_MASK_ON;

  if (buttonStatus[6] && buttonStatus[9]) {
    ReportData.HAT = DPAD_UPRIGHT_MASK_ON;
  } else if (buttonStatus[6] && buttonStatus[8]) {
    ReportData.HAT = DPAD_UPLEFT_MASK_ON;
  } else if (buttonStatus[7] && buttonStatus[9]) {
    ReportData.HAT = DPAD_DOWNRIGHT_MASK_ON;
  } else if (buttonStatus[7] && buttonStatus[8]) {
    ReportData.HAT = DPAD_DOWNLEFT_MASK_ON;
  } else if (buttonStatus[6]) {
    ReportData.HAT = DPAD_UP_MASK_ON;
  } else if (buttonStatus[7]) {
    ReportData.HAT = DPAD_DOWN_MASK_ON;
  } else if (buttonStatus[8]) {
    ReportData.HAT = DPAD_LEFT_MASK_ON;
  } else if (buttonStatus[9]) {
    ReportData.HAT = DPAD_RIGHT_MASK_ON;
  } else {
    ReportData.HAT = DPAD_NOTHING_MASK_ON;
  }
}

void processButtons() {
  switch (state) {
    case ANALOG_MODE:
      processLANALOG();
      break;
    case DPAD_MODE:
      processDPad();
      break;
  }
  buttonProcessing();
}

void buttonProcessing() {
  if (buttonStatus[14]) ReportData.Button |= A_MASK_ON;        // BUTTONA
  if (buttonStatus[15]) ReportData.Button |= B_MASK_ON;        // BUTTONB
  if (buttonStatus[16]) ReportData.Button |= X_MASK_ON;        // BUTTONX
  if (buttonStatus[17]) ReportData.Button |= Y_MASK_ON;        // BUTTONY
  if (buttonStatus[18]) ReportData.Button |= LB_MASK_ON;       // BUTTONLB
  if (buttonStatus[19]) ReportData.Button |= RB_MASK_ON;       // BUTTONRB
  if (buttonStatus[20]) ReportData.Button |= ZL_MASK_ON;       // BUTTONLT
  if (buttonStatus[21]) ReportData.Button |= ZR_MASK_ON;       // BUTTONRT
  if (buttonStatus[22]) ReportData.Button |= START_MASK_ON;    // BUTTONSTART
  if (buttonStatus[23]) ReportData.Button |= SELECT_MASK_ON;   // BUTTONSELECT
  if (buttonStatus[24]) ReportData.Button |= HOME_MASK_ON;     // BUTTONHOME
  if (buttonStatus[25]) ReportData.Button |= L3_MASK_ON;       // BUTTONL3
  if (buttonStatus[26]) ReportData.Button |= R3_MASK_ON;       // BUTTONR3
  if (buttonStatus[27]) ReportData.Button |= CAPTURE_MASK_ON;  // BUTTONCAPTURE
}
