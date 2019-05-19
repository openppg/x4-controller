// Copyright 2018 <Zach Whitehead>
// OpenPPG

#include <AceButton.h>
#include <Adafruit_DRV2605.h>  // haptic controller
#include <Adafruit_SSD1306.h>  // screen
#include <Adafruit_SleepyDog.h> // watchdog
#include <AdjustableButtonConfig.h>
#include <ResponsiveAnalogRead.h>  // smoothing for throttle
#include <SPI.h>
#include <Thread.h>
#include <StaticThreadController.h>
#include <TimeLib.h>
#include <Wire.h>
#include "crc.c"

using namespace ace_button;

// Arduino Pins
#define BATT_IN       A1  // Battery voltage in (3.3v max)
#define BUTTON_TOP    6   // arm/disarm button_top
#define BUTTON_SIDE   7   // secondary button_top
#define BUZZER_PIN    5   // output for buzzer speaker
#define LED_SW        9  // output for LED on button_top switch
#define LED_2         0  // output for LED 2
#define LED_3         38  // output for LED 3
#define THROTTLE_PIN  A0  // throttle pot input
#define RX_TX_TOGGLE  11  // rs485

#define CTRL_VER 0x00
#define CTRL2HUB_ID 0x10
#define HUB2CTRL_ID 0x20

#define FEATURE_CRUISE false

#define CRUISE_GRACE 2  // 2 sec period to get off throttle
#define CRUISE_MAX 300  // 5 min max cruising

Adafruit_SSD1306 display(128, 64, &Wire, 4);
Adafruit_DRV2605 vibe;

ResponsiveAnalogRead pot(THROTTLE_PIN, false);
ResponsiveAnalogRead analogBatt(BATT_IN, false);
AceButton button_top(BUTTON_TOP);
AceButton button_side(BUTTON_SIDE);
AdjustableButtonConfig buttonConfig;

const int bgInterval = 100;  // background updates (milliseconds)

Thread ledBlinkThread = Thread();
Thread displayThread = Thread();
Thread throttleThread = Thread();
Thread butttonThread = Thread();
StaticThreadController<4> threads(&ledBlinkThread, &displayThread,
                                  &throttleThread, &butttonThread);

bool armed = false;
int page = 0;
uint32_t armedAtMilis = 0;
uint32_t cruisedAtMilis = 0;
unsigned int armedSecs = 0;
unsigned int last_throttle = 0;

#pragma message "Warning: OpenPPG software is in beta"

// TODO(zach): Move these to header type files
#define INTERFACE_HUB
#define CTRL_VER 0x00
#define CTRL2HUB_ID 0x10
#define HUB2CTRL_ID 0x20
#define HUB2CTRL_SIZE 22

#pragma pack(push, 1)
typedef struct {
  uint8_t version;
  uint8_t id;
  uint8_t length;
  uint8_t armed;
  uint16_t throttlePercent;  // 0 to 1000
  uint16_t crc;
}STR_CTRL2HUB_MSG;

typedef struct {
  uint8_t version;
  uint8_t id;
  uint8_t length;
  uint8_t armed;
  uint32_t voltage;
  uint32_t totalMah;
  uint32_t totalCurrent;
  uint16_t avgRpm;
  uint8_t avgCapTemp;
  uint8_t avgFetTemp;
  uint16_t crc;
}STR_HUB2CTRL_MSG;

#pragma pack(pop);

static STR_CTRL2HUB_MSG controlData;
static STR_HUB2CTRL_MSG hubData;

void setup() {
  delay(250);  // power-up safety delay
  Serial.begin(115200);
  Serial.setTimeout(5);

  SerialUSB.begin(115200);
  SerialUSB.println(F("Booting up (USB) V2.1"));

  pinMode(LED_SW, OUTPUT);      // set up the external LED pin
  pinMode(LED_2, OUTPUT);       // set up the internal LED2 pin
  pinMode(LED_3, OUTPUT);       // set up the internal LED3 pin
  pinMode(RX_TX_TOGGLE, OUTPUT);

  analogReadResolution(12);     // M0 chip provides 12bit resolution
  pot.setAnalogResolution(4096);
  analogBatt.setAnalogResolution(4096);
  analogBatt.setSnapMultiplier(0.01);  // more smoothing
  unsigned int startup_vibes[] = { 29, 29, 0 };
  runVibe(startup_vibes, 3);
  digitalWrite(RX_TX_TOGGLE, LOW);

  initButtons();
  initDisplay();

  ledBlinkThread.onRun(blinkLED);
  ledBlinkThread.setInterval(500);

  displayThread.onRun(updateDisplay);
  displayThread.setInterval(100);

  butttonThread.onRun(checkButtons);
  butttonThread.setInterval(25);

  throttleThread.onRun(handleThrottle);
  throttleThread.setInterval(22);

  int countdownMS = Watchdog.enable(4000);
}

void loop() {
  Watchdog.reset();
  threads.run();
}

void checkButtons() {
  button_side.check();
  button_top.check();
}

byte getBatteryPercent() {
  float voltage = hubData.voltage /1000;
  // TODO(zach): LiPo curve
  float percent = mapf(voltage, 42, 50, 1, 100);
  percent = constrain(percent, 0, 100);

  return round(percent);
}

void disarmSystem() {
  unsigned int disarm_melody[] = { 2093, 1976, 880};
  unsigned int disarm_vibes[] = { 70, 33, 0 };

  armed = false;
  ledBlinkThread.enabled = true;
  updateDisplay();
  runVibe(disarm_vibes, 3);
  playMelody(disarm_melody, 3);
  delay(1500);  // dont allow immediate rearming
}

void initButtons() {
  pinMode(BUTTON_TOP, INPUT_PULLUP);
  pinMode(BUTTON_SIDE, INPUT_PULLUP);

  button_side.setButtonConfig(&buttonConfig);
  button_top.setButtonConfig(&buttonConfig);
  buttonConfig.setEventHandler(handleButtonEvent);
  buttonConfig.setFeature(ButtonConfig::kFeatureDoubleClick);
  buttonConfig.setFeature(ButtonConfig::kFeatureSuppressAfterDoubleClick);
}

void initDisplay() {
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.clearDisplay();
  display.setRotation(2);  // for right hand throttle
  display.setTextSize(3);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.println(F("OpenPPG"));
  display.println(F("V2.1.0"));
  display.display();
  display.clearDisplay();
}

void handleThrottle() {
  handleHubResonse();
  pot.update();
  int rawval = pot.getValue();
  int val = map(rawval, 0, 4095, 0, 1000);  // mapping val to min and max
  sendToHub(val);
}

void sendToHub(int throttle_val) {
  memset((uint8_t*)&controlData, 0, sizeof(STR_CTRL2HUB_MSG));

  controlData.version = CTRL_VER;
  controlData.id = CTRL2HUB_ID;
  controlData.length = sizeof(STR_CTRL2HUB_MSG);
  controlData.armed = armed;
  controlData.throttlePercent = throttle_val;
  controlData.crc = crc16((uint8_t*)&controlData, sizeof(STR_CTRL2HUB_MSG) - 2);

  digitalWrite(RX_TX_TOGGLE, HIGH);
  Serial.write((uint8_t*)&controlData, 8);  // send to hub
  Serial.flush();
  digitalWrite(RX_TX_TOGGLE, LOW);
}

void handleHubResonse() {
  uint8_t serialData[HUB2CTRL_SIZE];

  while (Serial.available() > 0) {
    memset(serialData, 0, sizeof(serialData));
    int size = Serial.readBytes(serialData, HUB2CTRL_SIZE);
    receiveControlData(serialData, size);
  }
  Serial.flush();
}

void receiveControlData(uint8_t *buf, uint32_t size) {
  if (size != sizeof(STR_HUB2CTRL_MSG)) {
    SerialUSB.print("wrong size ");
    SerialUSB.print(size);
    SerialUSB.print(" should be ");
    SerialUSB.println(sizeof(STR_HUB2CTRL_MSG));
    return;
  }

  memcpy((uint8_t*)&hubData, buf, sizeof(STR_HUB2CTRL_MSG));
  uint16_t crc = crc16((uint8_t*)&hubData, sizeof(STR_HUB2CTRL_MSG) - 2);
  if (crc != hubData.crc) {
    SerialUSB.print("wrong crc ");
    SerialUSB.print(crc);
    SerialUSB.println(" should be ");
    SerialUSB.print(hubData.crc);
    return;
  }
}

void armSystem() {
  unsigned int arm_melody[] = { 1760, 1976, 2093 };
  unsigned int arm_fail_melody[] = { 1560, 1560, 0 };
  unsigned int arm_vibes[] = { 83, 27, 0 };

  armed = true;
  sendToHub(0);
  delay(2);  // wait for response
  handleHubResonse();

  if (hubData.armed == 0) {
    playMelody(arm_fail_melody, 3);
    armed = false;
    return;
  }
  ledBlinkThread.enabled = false;
  armedAtMilis = millis();

  runVibe(arm_vibes, 3);
  playMelody(arm_melody, 3);

  setLEDs(HIGH);
}

// The event handler for the the buttons
void handleButtonEvent(AceButton *button, uint8_t eventType, uint8_t buttonState) {
  uint8_t pin = button->getPin();

  switch (eventType) {
  case AceButton::kEventReleased:
    if (pin == BUTTON_SIDE) nextPage();
    break;
  case AceButton::kEventDoubleClicked:
    if (pin == BUTTON_SIDE) {
    } else if (pin == BUTTON_TOP) {
      if (armed) {
        disarmSystem();
      } else if (throttleSafe()) {
        armSystem();
      }
    }
    break;
  }
}

// Returns true if the throttle/pot is below the safe threshold
bool throttleSafe() {
  pot.update();
  if (pot.getValue() < 100) {
    return true;
  }
  return false;
}

void updateDisplay() {
  float mamph = hubData.totalMah;
  float amph = mamph /1000;
  float voltage = hubData.voltage /1000;
  float current = hubData.totalCurrent /1000;
  float kw = voltage * current;
  byte percentage;
  String status;

  if (armed) {
    status = F("Armed");
    armedSecs = (millis() - armedAtMilis) / 1000;  // update time while armed
  } else {
    status = F("Disarmd");
  }

  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.println(status);
  display.setTextSize(3);

  switch (page) {
  case 0: // shows current voltage and amperage
    display.print(voltage, 1);
    display.setTextSize(2);
    display.println(F("V"));
    addLineSpace();
    display.setTextSize(3);
    display.print(current, 0);
    display.setTextSize(2);
    display.println(F("A"));
    break;
  case 1: // shows total amp hrs and timer
    display.print(amph, 1);
    display.setTextSize(2);
    display.println(F("ah"));
    addLineSpace();
    display.setTextSize(3);
    displayTime(armedSecs);
    break;
  case 2: // shows volts and kw
    display.print(voltage, 1);
    display.setTextSize(2);
    display.println(F("V"));
    addLineSpace();
    display.setTextSize(3);
    display.print(kw, 0);
    display.setTextSize(2);
    display.println(F("kw"));
    break;
  default:
    display.println(F("Dsp Err"));  // should never hit this
    break;
  }
  display.display();
  display.clearDisplay();
}

// displays number of minutes and seconds (since armed)
void displayTime(int val) {
  int minutes = val / 60;
  int seconds = numberOfSeconds(val);

  printDigits(minutes);
  display.print(":");
  printDigits(seconds);
}
