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

#define CTRL_VER 0x00
#define CTRL2HUB_ID 0x10
#define HUB2CTRL_ID 0x20

#define FEATURE_AUTO_PAGING false  // use button by default to change page
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

Thread ledThread = Thread();
Thread displayThread = Thread();

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
  SerialUSB.begin(115200);
  SerialUSB.println(F("Booting up (USB) V2.1"));

  pinMode(LED_SW, OUTPUT);      // set up the external LED pin
  pinMode(LED_2, OUTPUT);       // set up the internal LED2 pin
  pinMode(LED_3, OUTPUT);       // set up the internal LED3 pin
  analogReadResolution(12);     // M0 chip provides 12bit resolution
  pot.setAnalogResolution(4096);
  analogBatt.setAnalogResolution(4096);
  analogBatt.setSnapMultiplier(0.01);  // more smoothing
  unsigned int startup_vibes[] = { 29, 29, 0 };
  runVibe(startup_vibes, 3);

  initButtons();
  initDisplay();

  ledThread.onRun(blinkLED);
  ledThread.setInterval(500);

  displayThread.onRun(updateDisplay);
  displayThread.setInterval(100);

  int countdownMS = Watchdog.enable(4000);
}

void loop() {
  Watchdog.reset();
  button_side.check();
  button_top.check();
  handleHubResonse();
  handleThrottle();

  if (ledThread.shouldRun())
      ledThread.run();
  if (displayThread.shouldRun())
      displayThread.run();
}

float getBatteryVolts() {
  return 50;  // TODO(zach) get from hub
}

byte getBatteryPercent() {
  float volts = getBatteryVolts();
  // Serial.print(voltage);
  // Serial.println(" volts");
  // TODO(zach): LiPo curve
  float percent = mapf(volts, 42, 50, 1, 100);
  // Serial.print(percent);
  // Serial.println(" percentage");
  percent = constrain(percent, 0, 100);

  return round(percent);
}

void disarmSystem() {
  unsigned int disarm_melody[] = { 2093, 1976, 880};
  unsigned int disarm_vibes[] = { 70, 33, 0 };

  armed = false;
  ledThread.enabled = true;
  updateDisplay();
  // Serial.println(F("disarmed"));
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
  // Clear the buffer.
  display.clearDisplay();
  display.setRotation(2);  // line required for right hand throttle

  display.setTextSize(3);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.println(F("OpenPPG"));
  display.println(F("V2.1.0"));
  display.display();
  display.clearDisplay();
}

void handleThrottle() {
  pot.update();
  int rawval = pot.getValue();
  int val = map(rawval, 0, 4095, 0, 1000);  // mapping val to min and max
  memset((uint8_t*)&controlData, 0, sizeof(STR_CTRL2HUB_MSG));

  controlData.version = CTRL_VER;
  controlData.id = CTRL2HUB_ID;
  controlData.length = sizeof(STR_CTRL2HUB_MSG);
  controlData.armed = armed;
  controlData.throttlePercent = val;
  controlData.crc = crc16((uint8_t*)&controlData, sizeof(STR_CTRL2HUB_MSG) - 2);

  Serial.write((uint8_t*)&controlData, 8);  // send to hub
  Serial.flush();

  delay(5);
  // handleHubResonse();
}

void handleHubResonse() {
  Serial.setTimeout(5);
  uint8_t serialData[21];

  while (Serial.available() > 0) {
    // get the new byte:
    memset(serialData, 0, sizeof(serialData));
    int size = Serial.readBytes(serialData, 21);
    receiveControlData(serialData, size);
  }
  Serial.flush();
}

void receiveControlData(uint8_t *buf, uint32_t size) {
  if (size != sizeof(STR_HUB2CTRL_MSG)) {
    SerialUSB.print("wrong size ");
    SerialUSB.print(size);
    SerialUSB.println(" should be ");
    SerialUSB.print(sizeof(STR_HUB2CTRL_MSG));

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
  SerialUSB.print("valid! RPM ");
  SerialUSB.println(hubData.avgRpm, DEC);
  // do stuff
}

void armSystem() {
  unsigned int arm_melody[] = { 1760, 1976, 2093 };
  unsigned int arm_vibes[] = { 83, 27, 0 };
  // Serial.println(F("Sending Arm Signal"));

  armed = true;
  ledThread.enabled = false;

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
    //  Serial.println(F("normal clicked"));
    if (pin == BUTTON_SIDE) nextPage();
    break;
  case AceButton::kEventDoubleClicked:
    // Serial.print(F("double clicked "));
    if (pin == BUTTON_SIDE) {
    // Serial.println(F("side"));
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

void runVibe(unsigned int sequence[], int siz) {
  vibe.begin();
  for (int thisNote = 0; thisNote < siz; thisNote++) {
    vibe.setWaveform(thisNote, sequence[thisNote]);
  }
  vibe.go();
}

void playMelody(unsigned int melody[], int siz) {
  for (int thisNote = 0; thisNote < siz; thisNote++) {
    // quarter note = 1000 / 4, eigth note = 1000/8, etc.
    int noteDuration = 125;
    tone(BUZZER_PIN, melody[thisNote], noteDuration);
    delay(noteDuration);  // to distinguish the notes, delay between them
  }
  noTone(BUZZER_PIN);
}

void updateDisplay() {
  float voltage;
  byte percentage;
  String status;

  if (armed) {
    status = F("Armed");
    armedSecs = (millis() - armedAtMilis) / 1000;  // update time while armed
  } else {
    status = F("Disarmd");
  }

  display.setTextSize(3);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.println(status);
  display.setTextSize(4);

  switch (page) {
  case 0:
    voltage = getBatteryVolts();
    display.print(voltage, 1);
    display.println(F("V"));
    break;
  case 1:
    percentage = getBatteryPercent();
    display.print(percentage, 1);
    display.println(F("%"));
    break;
  case 2:
    displayTime(armedSecs);
    break;
  default:
    display.println(F("Dsp Err"));  // should never hit this
    break;
  }
  if (FEATURE_AUTO_PAGING) nextPage();
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
