// Copyright 2019 <Zach Whitehead>
// OpenPPG

#include "libraries/crc.c"      // packet error checking
#include <AceButton.h>
#include <Adafruit_DRV2605.h>   // haptic controller
#include <Adafruit_SSD1306.h>   // screen
#include <Adafruit_SleepyDog.h> // watchdog
#include <AdjustableButtonConfig.h>
#include <ResponsiveAnalogRead.h>  // smoothing for throttle
#include <SPI.h>
#include <StaticThreadController.h>
#include <Thread.h>   // run tasks at different intervals
#include <TimeLib.h>  // convert time to hours mins etc
#include <Wire.h>
#include <extEEPROM.h>  // https://github.com/PaoloP74/extEEPROM

using namespace ace_button;

// Arduino Pins
#define BATT_IN       A1  // Battery voltage in (3.3v max)
#define BUTTON_TOP    6   // arm/disarm button_top
#define BUTTON_SIDE   7   // secondary button_top
#define BUZZER_PIN    5   // output for buzzer speaker
#define LED_SW        9   // output for LED on button_top switch
#define LED_2         0   // output for LED 2
#define LED_3         38  // output for LED 3
#define THROTTLE_PIN  A0  // throttle pot input
#define RX_TX_TOGGLE  11  // rs485

#define CTRL_VER 0x00
#define CTRL2HUB_ID 0x10
#define HUB2CTRL_ID 0x20

#define ARM_VERIFY false
#define CURRENT_DIVIDE 100.0
#define VOLTAGE_DIVIDE 1000.0

#define BATT_MIN_V 49    // 42v for 6S
#define BATT_MAX_V 58.8  // 50v for 6S

// Calibration
#define MAMP_OFFSET 200

#define VERSION_MAJOR 4
#define VERSION_MINOR 0

#define CRUISE_GRACE 2  // 2 sec period to get off throttle
#define CRUISE_MAX 300  // 5 min max cruising

Adafruit_SSD1306 display(128, 64, &Wire, 4);
Adafruit_DRV2605 vibe;

ResponsiveAnalogRead pot(THROTTLE_PIN, false);
ResponsiveAnalogRead analogBatt(BATT_IN, false);
AceButton button_top(BUTTON_TOP);
AceButton button_side(BUTTON_SIDE);
AdjustableButtonConfig buttonConfig;
extEEPROM eep(kbits_64, 1, 64);

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

typedef struct {
  uint8_t version_major;
  uint8_t version_minor;
  uint16_t armed_time;
  uint16_t crc;
}STR_DEVICE_DATA;
#pragma pack(pop)

static STR_CTRL2HUB_MSG controlData;
static STR_HUB2CTRL_MSG hubData;
static STR_DEVICE_DATA deviceData;

void setup() {
  delay(250);  // power-up safety delay
  Serial5.begin(115200);
  Serial5.setTimeout(5);

  uint8_t eepStatus = eep.begin(eep.twiClock400kHz);  // go fast

  Serial.begin(115200);
  Serial.print(F("Booting up (USB) V"));
  Serial.print(VERSION_MAJOR + "." + VERSION_MINOR);

  pinMode(LED_SW, OUTPUT);      // set up the external LED pin
  pinMode(LED_2, OUTPUT);       // set up the internal LED2 pin
  pinMode(LED_3, OUTPUT);       // set up the internal LED3 pin
  pinMode(RX_TX_TOGGLE, OUTPUT);

  analogReadResolution(12);     // M0 chip provides 12bit resolution
  pot.setAnalogResolution(4096);
  analogBatt.setAnalogResolution(4096);
  analogBatt.setSnapMultiplier(0.01);   // more smoothing
  unsigned int startup_vibes[] = { 27, 27, 0 };
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
  refreshDeviceData();
}

void refreshDeviceData() {
  uint8_t tempBuf[sizeof(STR_DEVICE_DATA)];
  if (0 != eep.read(0, tempBuf, sizeof(STR_DEVICE_DATA))) {
    Serial.println(F("error reading EEPROM"));
  }
  memcpy((uint8_t*)&deviceData, tempBuf, sizeof(STR_DEVICE_DATA));
  uint16_t crc = crc16((uint8_t*)&deviceData, sizeof(STR_DEVICE_DATA) - 2);
  if (crc != deviceData.crc) {
    Serial.print(F("Memory CRC mismatch. Resetting"));

    deviceData = STR_DEVICE_DATA();
    deviceData.version_major = VERSION_MAJOR;
    deviceData.version_minor = VERSION_MINOR;
    writeDeviceData();
    return;
  }
}

void writeDeviceData() {
  deviceData.crc = crc16((uint8_t*)&deviceData, sizeof(STR_DEVICE_DATA) - 2);

  if (0 != eep.write(0, (uint8_t*)&deviceData, sizeof(STR_DEVICE_DATA))){
    Serial.println(F("error writing EEPROM"));
  }
}

// main loop - everything runs in threads
void loop() {
  Watchdog.reset();
  threads.run();
}

void checkButtons() {
  button_side.check();
  button_top.check();
}

byte getBatteryPercent() {
  float voltage = hubData.voltage / VOLTAGE_DIVIDE;
  // TODO(zach): LiPo curve
  float percent = mapf(voltage, BATT_MIN_V, BATT_MAX_V, 0, 100);
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
  // update armed_time
  refreshDeviceData();
  deviceData.armed_time += round(armedSecs / 60);  // convert to mins
  writeDeviceData();

  delay(1500);  // dont allow immediate rearming
}

void initButtons() {
  pinMode(BUTTON_TOP, INPUT_PULLUP);
  pinMode(BUTTON_SIDE, INPUT_PULLUP);

  button_side.setButtonConfig(&buttonConfig);
  button_top.setButtonConfig(&buttonConfig);
  buttonConfig.setEventHandler(handleButtonEvent);
  buttonConfig.setFeature(ButtonConfig::kFeatureDoubleClick);
  buttonConfig.setFeature(ButtonConfig::kFeatureLongPress);
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
  display.print(F("V"));
  display.print(VERSION_MAJOR);
  display.print(F("."));
  display.print(VERSION_MINOR);
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
  Serial5.write((uint8_t*)&controlData, 8);  // send to hub
  Serial5.flush();
  digitalWrite(RX_TX_TOGGLE, LOW);
}

void handleHubResonse() {
  uint8_t serialData[HUB2CTRL_SIZE];

  while (Serial5.available() > 0) {
    memset(serialData, 0, sizeof(serialData));
    int size = Serial5.readBytes(serialData, HUB2CTRL_SIZE);
    receiveHubData(serialData, size);
  }
  Serial5.flush();
}

void receiveHubData(uint8_t *buf, uint32_t size) {
  if (size != sizeof(STR_HUB2CTRL_MSG)) {
    Serial.print("wrong size ");
    Serial.print(size);
    Serial.print(" should be ");
    Serial.println(sizeof(STR_HUB2CTRL_MSG));
    return;
  }

  memcpy((uint8_t*)&hubData, buf, sizeof(STR_HUB2CTRL_MSG));
  uint16_t crc = crc16((uint8_t*)&hubData, sizeof(STR_HUB2CTRL_MSG) - 2);
  if (crc != hubData.crc) {
    Serial.print(F("hub crc mismatch"));
    return;
  }
  if (hubData.totalCurrent > MAMP_OFFSET) { hubData.totalCurrent -= MAMP_OFFSET;}
}

void armSystem() {
  unsigned int arm_melody[] = { 1760, 1976, 2093 };
  unsigned int arm_vibes[] = { 70, 33, 0 };
  unsigned int arm_fail_vibes[] = { 14, 3, 0 };

  armed = true;
  sendToHub(0);
  delay(2);  // wait for response
  handleHubResonse();

  if (hubData.armed == 0 && ARM_VERIFY) {
    runVibe(arm_fail_vibes, 3);
    handleArmFail();
    armed = false;
    return;
  }
  ledBlinkThread.enabled = false;
  armedAtMilis = millis();

  runVibe(arm_vibes, 3);
  setLEDs(HIGH);
  playMelody(arm_melody, 3);
}

// The event handler for the the buttons
void handleButtonEvent(AceButton *button, uint8_t eventType, uint8_t btnState) {
  uint8_t pin = button->getPin();

  switch (eventType) {
  case AceButton::kEventReleased:
    if (pin == BUTTON_SIDE) nextPage();
    break;
  case AceButton::kEventDoubleClicked:
    if (pin == BUTTON_SIDE) {}
    else if (pin == BUTTON_TOP) {
      if (armed) {
        disarmSystem();
      } else if (throttleSafe()) {
        armSystem();
      } else {
        handleArmFail();
      }
    }
    break;
  case AceButton::kEventLongPressed:
    int side_state = digitalRead(BUTTON_SIDE);
    int top_state = digitalRead(BUTTON_TOP);

    if (top_state == LOW && side_state == LOW) {
      page = 3;
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
  byte percentage;
  String status;

  if (armed) {
    status = F("Armed");
    display.fillCircle(122, 5, 5, WHITE);
    armedSecs = (millis() - armedAtMilis) / 1000;  // update time while armed
  } else {
    status = F("Disarmed");
    display.drawCircle(122, 5, 5, WHITE);
  }

  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.println(status);
  display.setTextSize(3);

  switch (page) {
  case 0:  // shows current voltage and amperage
    displayPage0();
    break;
  case 1:  // shows total amp hrs and timer
    displayPage1();
    break;
  case 2:  // shows volts and kw
    displayPage2();
    break;
  case 3:  // shows version and hour meter
    displayVersions();
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
  int minutes = val / 60;  // numberOfMinutes(val);
  int seconds = numberOfSeconds(val);

  printDigits(minutes);
  display.print(F(":"));
  printDigits(seconds);
}

void displayPage0() {
  float voltage = hubData.voltage / VOLTAGE_DIVIDE;
  float current = hubData.totalCurrent / CURRENT_DIVIDE;
  display.print(voltage, 1);
  display.setTextSize(2);
  display.println(F("V"));
  addVSpace();
  display.setTextSize(3);
  display.print(current, 0);
  display.setTextSize(2);
  display.println(F("A"));
}

void displayPage1() {
  float amph = hubData.totalMah / 10;
  display.print(amph, 1);
  display.setTextSize(2);
  display.println(F("ah"));
  addVSpace();
  display.setTextSize(3);
  displayTime(armedSecs);
}

void displayPage2() {
  float voltage = hubData.voltage / VOLTAGE_DIVIDE;
  float current = hubData.totalCurrent / CURRENT_DIVIDE;
  float kw = (voltage * current) / 1000;
  display.print(getBatteryPercent());
  display.setTextSize(2);
  display.println(F("%"));
  addVSpace();
  display.setTextSize(3);
  display.print(kw, 2);
  display.setTextSize(2);
  display.println(F("kw"));
}

void displayVersions() {
  display.setTextSize(2);
  display.print(F("v"));
  display.print(VERSION_MAJOR);
  display.print(F("."));
  display.println(VERSION_MINOR);
  addVSpace();
  display.setTextSize(2);
  displayTime(deviceData.armed_time);
  display.print(F(" h:m"));
}
