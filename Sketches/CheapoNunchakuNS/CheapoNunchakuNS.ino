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
bool altmode = false;

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
  ReportData.LX = 128;
  ReportData.LY = 128;

  Serial1.begin(9600);
  nunchuk.begin();
  while (!nunchuk.connect()) {
    Serial1.println("Nunchuk not detected!");
    delay(1000);
  }

  nunchuk.update();  // Ensure the nunchuk state is updated
  int clickStateC = nunchuk.buttonC();
  if (clickStateC == 1) {
    altmode = true;
  } else {
    altmode = false;
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

  handleAccel(nunchuk.accelY(), nunchuk.accelX(), nunchuk.buttonC(), nunchuk.buttonZ());

  int sensorValueY = nunchuk.joyY();
  int sensorValueX = nunchuk.joyX();  // 0 - 255
  int clickStateC = nunchuk.buttonC();
  int clickStateZ = nunchuk.buttonZ();

  // Toggle mode handling
  if (clickStateC && clickStateZ) {
    if (!toggleInProgress) {
      toggleStartTime = millis();
      toggleInProgress = true;
    } else if (millis() - toggleStartTime >= 4000) {
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
  if (state == ANALOG_MODE && altmode == false && clickStateC == 0 && clickStateZ == 0) {
    ReportData.RX = sensorValueX;
    ReportData.RY = 255 - sensorValueY;  // Invert the Y-axis value
  } else if (state == ANALOG_MODE && altmode == true && clickStateC == 0 && clickStateZ == 0) {
    ReportData.LX = sensorValueX;
    ReportData.LY = 255 - sensorValueY;  // Invert the Y-axis value
  } else {
    ReportData.RX = 128;
    ReportData.RY = 128;  // DPAD_MODE
    buttonStatus[6] = (sensorValueY > 192 && !clickStateC && !clickStateZ) ? HIGH : LOW;  // BUTTONUP
    buttonStatus[7] = (sensorValueY < 64 && !clickStateC && !clickStateZ) ? HIGH : LOW;   // BUTTONDOWN
    buttonStatus[9] = (sensorValueX > 192 && !clickStateC && !clickStateZ) ? HIGH : LOW;  // BUTTONRIGHT
    buttonStatus[8] = (sensorValueX < 64 && !clickStateC && !clickStateZ) ? HIGH : LOW;   // BUTTONLEFT
  }

  // Toggle LT
  if (sensorValueY > 192 && clickStateZ && !clickStateC) {
    if (!ltToggled) {
      ltToggled = true;
      buttonStatus[20] = !buttonStatus[20];  // Toggle BUTTONLT
    }
  } else {
    ltToggled = false;
  }
  buttonStatus[17] = (sensorValueX < 64 && clickStateC && !clickStateZ) ? HIGH : LOW;   // BUTTONY
  buttonStatus[16] = (sensorValueY > 192 && clickStateC && !clickStateZ) ? HIGH : LOW;  // BUTTONX
  buttonStatus[14] = (sensorValueX > 192 && clickStateC && !clickStateZ) ? HIGH : LOW;  // BUTTONY
  buttonStatus[15] = (sensorValueY < 64 && clickStateC && !clickStateZ) ? HIGH : LOW;   // BUTTONX

  buttonStatus[19] = (sensorValueX > 192 && clickStateZ && !clickStateC) ? HIGH : LOW;  // BUTTONRB
  buttonStatus[18] = (sensorValueX < 64 && clickStateZ && !clickStateC) ? HIGH : LOW;   // BUTTONLB
  buttonStatus[21] = (sensorValueY < 64 && clickStateZ && !clickStateC) ? HIGH : LOW;   // BUTTONRT

  buttonStatus[22] = (sensorValueY > 192 && clickStateZ && clickStateC) ? HIGH : LOW;  // BUTTONSTART
  buttonStatus[23] = (sensorValueY < 64 && clickStateZ && clickStateC) ? HIGH : LOW;   // BUTTONSELECT
  buttonStatus[25] = (sensorValueX > 192 && clickStateZ && clickStateC) ? HIGH : LOW;  // BUTTONL3
  buttonStatus[26] = (sensorValueX < 64 && clickStateZ && clickStateC) ? HIGH : LOW;   // BUTTONR3
}

void handleAccel(uint16_t aY, uint16_t aX, uint16_t clickStateC, uint16_t clickStateZ) {
  if (clickStateC == 0 || clickStateZ == 0) {
    const uint16_t accel3Threshold = 400;  // Controller pointed up
    const uint16_t accel4Threshold = 600;  // Controller pointed up

    const uint8_t countThreshold = 6;  // Arbitrary limit to filter noise

    static uint8_t ax1Count = 0;
    static uint8_t ax2Count = 0;

    if ((aY <= accel3Threshold && altmode == false) || (aY >= accel4Threshold && altmode == false)) {
      ReportData.LY = map(aY, 256, 768, 0, 255);
    } else {
      ReportData.LY = 128;
    }
    if ((aX <= accel3Threshold && altmode == false) || (aX >= accel4Threshold && altmode == false)) {
      ReportData.LX = map(aX, 200, 800, 0, 255);
    } else {
      ReportData.LX = 128;
    }
    if ((aY <= accel3Threshold && altmode == true) || (aY >= accel4Threshold && altmode == true)) {
      ReportData.RY = map(aY, 256, 768, 0, 255);
    } else {
      ReportData.RY = 128;
    }
    if ((aX <= accel3Threshold && altmode == true) || (aX >= accel4Threshold && altmode == true)) {
      ReportData.RX = map(aX, 200, 800, 0, 255);
    } else {
      ReportData.RX = 128;
    }
  }
}

void processLANALOG() {
  ReportData.HAT = DPAD_NOTHING_MASK_ON;
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
