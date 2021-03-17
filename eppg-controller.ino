// Copyright 2020 <Zach Whitehead>
// OpenPPG

#include "libraries/crc.c"       // packet error checking
#include "inc/config.h"          // device config
#include "inc/structs.h"         // data structs
#include <AceButton.h>           // button clicks
#include <Adafruit_DRV2605.h>    // haptic controller
#include <Adafruit_ST7735.h>    // screen
#include <Adafruit_SleepyDog.h>  // watchdog
#include "Adafruit_TinyUSB.h"
#include <AdjustableButtonConfig.h>
#include <ArduinoJson.h>
#include <ResponsiveAnalogRead.h>  // smoothing for throttle
#include <SPI.h>
#include <StaticThreadController.h>
#include <Thread.h>   // run tasks at different intervals
#include <TimeLib.h>  // convert time to hours mins etc
#include <Wire.h>
#include <extEEPROM.h>  // https://github.com/PaoloP74/extEEPROM

#include <Adafruit_BMP3XX.h>
#include <Servo.h> // to control ESCs

#include "inc/sp140-globals.h" // device config

using namespace ace_button;

Adafruit_ST7735 display = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

Adafruit_DRV2605 vibe;

// USB WebUSB object
Adafruit_USBD_WebUSB usb_web;
WEBUSB_URL_DEF(landingPage, 1 /*https*/, "openppg.github.io/openppg-config");

ResponsiveAnalogRead pot(THROTTLE_PIN, false);
AceButton button_top(BUTTON_TOP);
AdjustableButtonConfig buttonConfig;
extEEPROM eep(kbits_64, 1, 64);

const int bgInterval = 100;  // background updates (milliseconds)

Thread ledBlinkThread = Thread();
Thread displayThread = Thread();
Thread throttleThread = Thread();
Thread buttonThread = Thread();
Thread telemetryThread = Thread();
StaticThreadController<5> threads(&ledBlinkThread, &displayThread, &throttleThread,
                                  &buttonThread, &telemetryThread);

bool armed = false;
bool use_hub_v2 = true;
int page = 0;
float armAltM = 0;
uint32_t armedAtMilis = 0;
uint32_t cruisedAtMilis = 0;
unsigned int armedSecs = 0;
unsigned int last_throttle = 0;

#pragma message "Warning: OpenPPG software is in beta"

// the setup function runs once when you press reset or power the board
void setup() {
  pinMode(LED_SW, OUTPUT);

  usb_web.begin();
  usb_web.setLandingPage(&landingPage);
  usb_web.setLineStateCallback(line_state_callback);

  Serial.begin(115200);
  Serial5.begin(ESC_BAUD_RATE);
  Serial5.setTimeout(ESC_TIMEOUT);

  Serial.print(F("Booting up (USB) V"));
  Serial.print(VERSION_MAJOR + "." + VERSION_MINOR);

  pinMode(LED_SW, OUTPUT);      // set up the external LED pin
  pinMode(LED_2, OUTPUT);       // set up the internal LED2 pin

  analogReadResolution(12);     // M0 chip provides 12bit resolution
  pot.setAnalogResolution(4096);
  unsigned int startup_vibes[] = { 27, 27, 0 };
  runVibe(startup_vibes, 3);

  initButtons();
  initDisplay();

  ledBlinkThread.onRun(blinkLED);
  ledBlinkThread.setInterval(500);

  displayThread.onRun(updateDisplay);
  displayThread.setInterval(100);

  buttonThread.onRun(checkButtons);
  buttonThread.setInterval(25);

  throttleThread.onRun(handleThrottle);
  throttleThread.setInterval(22);

  telemetryThread.onRun(handleTelemetry);
  telemetryThread.setInterval(50);

  int countdownMS = Watchdog.enable(4000);
  uint8_t eepStatus = eep.begin(eep.twiClock100kHz);
  refreshDeviceData();
  setup140();
}

void setup140() {
  esc.attach(ESC_PIN);
  esc.writeMicroseconds(0); // make sure motors off

  buzzInit(ENABLE_BUZ);
  tftInit();
  bmpInit();
  vibe.begin();
  vibe.selectLibrary(1);
  vibe.setMode(DRV2605_MODE_INTTRIG);

  vibrateNotify();

  //eepInit();

  if(!digitalRead(BUTTON_TOP)){
    // Switch modes
    bool mode = eep.read(6);
    //eep.write(6, !mode); // 0=BEGINNER 1=EXPERT
  }

  beginner = false; // TODO read from eep

  //setFlightHours(0);    // uncomment to set flight log hours (0.0 to 9999.9)... MUST re-comment and re-upload!
  delay(10);

  if(!digitalRead(BUTTON_TOP) && beginner){
    display.setCursor(10, 20);
    display.setTextSize(2);
    display.setTextColor(BLUE);
    display.print("BEGINNER");
    display.setCursor(10, 36);
    display.print("MODE");
    display.setCursor(10, 52);
    display.print("ACTIVATED");
    display.setTextColor(BLACK);
  }
  if(!digitalRead(BUTTON_TOP) && !beginner){
    display.setCursor(10, 20);
    display.setTextSize(2);
    display.setTextColor(RED);
    display.print("EXPERT");
    display.setCursor(10, 36);
    display.print("MODE");
    display.setCursor(10, 52);
    display.print("ACTIVATED");
    display.setTextColor(BLACK);
  }
  while(!digitalRead(BUTTON_TOP));
  if(beginner){  // Erase Text
    display.setCursor(10, 20);
    display.setTextSize(2);
    display.setTextColor(WHITE);
    display.print("BEGINNER");
    display.setCursor(10, 36);
    display.print("MODE");
    display.setCursor(10, 52);
    display.print("ACTIVATED");
    display.setTextColor(BLACK);
  }
  if(!beginner){  // Erase Text
    display.setCursor(10, 20);
    display.setTextSize(2);
    display.setTextColor(WHITE);
    display.print("EXPERT");
    display.setCursor(10, 36);
    display.print("MODE");
    display.setCursor(10, 52);
    display.print("ACTIVATED");
    display.setTextColor(BLACK);
  }
}

// main loop - everything runs in threads
void loop() {
  Watchdog.reset();
  // from WebUSB to both Serial & webUSB
  if (usb_web.available()) parse_usb_serial();
  threads.run();
}

void checkButtons() {
  button_top.check();
}

byte getBatteryPercent() {
  float voltage = hubData.voltage / VOLTAGE_DIVIDE;
  // TODO(zach): LiPo curve
  float percent = mapf(voltage, deviceData.min_batt_v, deviceData.max_batt_v, 0, 100);
  percent = constrain(percent, 0, 100);

  return round(percent);
}

void disarmSystem() {
  esc.writeMicroseconds(0);
  Serial.println(F("disarmed"));

  unsigned int disarm_melody[] = { 2093, 1976, 880 };
  unsigned int disarm_vibes[] = { 70, 33, 0 };

  armed = false;
  ledBlinkThread.enabled = true;
  updateDisplay();
  if(ENABLE_VIB) runVibe(disarm_vibes, 3);
  if(ENABLE_BUZ){
    playMelody(disarm_melody, 3);
  }
  // update armed_time
  // refreshDeviceData();
  // deviceData.armed_time += round(armedSecs / 60);  // convert to mins
  // writeDeviceData();
  // recordFlightHours(); TODO
  delay(1500);  // dont allow immediate rearming
}

// inital button setup and config
void initButtons() {
  pinMode(BUTTON_TOP, INPUT_PULLUP);

  button_top.setButtonConfig(&buttonConfig);
  buttonConfig.setEventHandler(handleButtonEvent);
  buttonConfig.setFeature(ButtonConfig::kFeatureDoubleClick);
  buttonConfig.setFeature(ButtonConfig::kFeatureLongPress);
  buttonConfig.setFeature(ButtonConfig::kFeatureSuppressAfterDoubleClick);
}

// inital screen setup and config
void initDisplay() {
  display.initR(INITR_BLACKTAB);    // Init ST7735S chip, black tab
  display.fillScreen(WHITE);
  display.setTextColor(BLACK);
  display.setCursor(0, 0);
  display.setTextSize(1);
  display.setTextWrap(true);

  int rotation = 1;
  if (LEFT_HAND_THROTTLE) rotation = 3;
  display.setRotation(rotation);  // 1=right hand, 3=left hand
  pinMode(TFT_LITE, OUTPUT);
  digitalWrite(TFT_LITE, HIGH);  // Backlight on
}

// read throttle and send to hub
void handleThrottle() {
  if (!armed) return; //safe

  int maxPWM = 2000;
  pot.update();
  potLvl = pot.getValue();
  if(beginner){
    potLvl = limitedThrottle(potLvl, prevPotLvl, 300);
    maxPWM = 1778;  // 75% interpolated from 1112 to 2000
  }
  handleCruise();  // activate or deactivate cruise
  if (cruising) { throttlePWM = mapf(cruiseLvl, 0, 4095, 1110, maxPWM); }
  else { throttlePWM = mapf(potLvl, 0, 4095, 1110, maxPWM); } // mapping val to minimum and maximum
  throttlePercent = mapf(throttlePWM, 1112,2000, 0,100);
  if (throttlePercent<0) {
    throttlePercent = 0;
  }
  esc.writeMicroseconds(throttlePWM); // using val as the signal to esc
  //Serial.print(F("WRITING: "));
  //Serial.println(throttlePWM);
}


// get the PPG ready to fly
bool armSystem() {
  unsigned int arm_melody[] = { 1760, 1976, 2093 };
  unsigned int arm_vibes[] = { 70, 33, 0 };

  armed = true;
  esc.writeMicroseconds(1000);  // initialize the signal to 1000

  ledBlinkThread.enabled = false;
  armedAtMilis = millis();
  armAltM = getAltitudeM();

  setLEDs(HIGH);
  if (ENABLE_VIB) runVibe(arm_vibes, 3);
  if (ENABLE_BUZ) playMelody(arm_melody, 3);
  Serial.println(F("Sending Arm Signal"));
  return true;
}

// The event handler for the the buttons
void handleButtonEvent(AceButton *button, uint8_t eventType, uint8_t btnState) {
  uint8_t pin = button->getPin();

  switch (eventType) {
  case AceButton::kEventDoubleClicked:
    if (pin == BUTTON_TOP) {
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
    int top_state = digitalRead(BUTTON_TOP);

    if (top_state == LOW) {
      page = 4;
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

// convert barometer data to altitude in meters
float getAltitudeM() {
  bmp.performReading();
  ambientTempC = bmp.temperature;
  float altitudeM = bmp.readAltitude(deviceData.sea_pressure);
  aglM = altitudeM - armAltM;

  Serial.print("alt: ");
  Serial.println(altitudeM);
  return altitudeM;
}

/********
 *
 * Display logic
 *
 *******/

// show data on screen and handle different pages
void updateDisplay() {
  dispValue(volts, prevVolts, 5, 1, 84, 42, 2, BLACK, WHITE);
  display.print("V");

  dispValue(amps, prevAmps, 3, 0, 108, 70, 2, BLACK, WHITE);
  display.print("A");

  dispValue(kilowatts, prevKilowatts, 4, 1, 10, /*42*/55, 2, BLACK, WHITE);
  display.print("kW");

  if(cruising) {
    display.setCursor(10, 80);
    display.setTextSize(1);
    display.setTextColor(RED);
    display.print("CRUISE");
    display.setTextColor(BLACK);
  }
  else{
    display.setCursor(10, 80);
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.print("CRUISE");
    display.setTextColor(BLACK);
  }

  if(beginner){
    display.setCursor(10, 40);
    display.setTextSize(1);
    display.setTextColor(BLUE);
    display.print("BEGINNER");
    display.setTextColor(BLACK);
  }
  else {
    display.setCursor(10, 40);
    display.setTextSize(1);
    display.setTextColor(RED);
    display.print("EXPERT");
    display.setTextColor(BLACK);
  }

  if (batteryPercent > 66){
    display.fillRect(0, 0, map(batteryPercent, 0, 100, 0, 108), 36, GREEN);
  }
  else if (batteryPercent > 33) {
    display.fillRect(0, 0, map(batteryPercent, 0, 100, 0, 108), 36, YELLOW);
  } else {
    display.fillRect(0, 0, map(batteryPercent, 0, 100, 0, 108), 36, RED);
  }
  if (volts < BATT_MIN_V) {
    if(batteryFlag) {
      batteryFlag = false;
      display.fillRect(0, 0, 108, 36, WHITE);
    }
    display.setCursor(0, 3);
    display.setTextSize(2);
    display.setTextColor(RED);
    display.println(" BATTERY");
    display.print(" DEAD/NC ");
  }
  else{
    batteryFlag = true;
    display.fillRect(map(batteryPercent, 0,100, 0,108), 0, map(batteryPercent, 0,100, 108,0), 36, WHITE);
  }
  dispValue(batteryPercent, prevBatteryPercent, 3, 0, 108, 10, 2, BLACK, WHITE);
  display.print("%");

  // For Debugging Throttle:
  //  display.fillRect(0, 0, map(throttlePercent, 0,100, 0,108), 36, BLUE);
  //  display.fillRect(map(throttlePercent, 0,100, 0,108), 0, map(throttlePercent, 0,100, 108,0), 36, WHITE);
  //  //dispValue(throttlePWM, prevThrotPWM, 4, 0, 108, 10, 2, BLACK, WHITE);
  //  dispValue(throttlePercent, prevThrotPercent, 3, 0, 108, 10, 2, BLACK, WHITE);
  //  display.print("%");

  display.fillRect(0, 36, 160, 1, BLACK);
  display.fillRect(108, 0, 1, 36, BLACK);
  display.fillRect(0, 92, 160, 1, BLACK);

  displayAlt();

  //dispValue(ambientTempF, prevAmbTempF, 3, 0, 10, 100, 2, BLACK, WHITE);
  //display.print("F");

  handleFlightTime();
  displayTime(throttleSecs, 8, 102);

  //dispPowerCycles(104,100,2);
}

// displays number of minutes and seconds (since armed)
void displayTime(int val) {
  int minutes = val / 60;  // numberOfMinutes(val);
  int seconds = numberOfSeconds(val);

  display.print(convertToDigits(minutes));
  display.print(F(":"));
  display.print(convertToDigits(seconds));
}

// display altitude data on screen
void displayAlt() {
  float altiudeM = 0;
  if (armAltM > 0 && deviceData.sea_pressure != DEFAULT_SEA_PRESSURE) {  // MSL
    altiudeM = getAltitudeM();
  } else {  // AGL
    altiudeM = getAltitudeM() - armAltM;
  }

  // convert to ft if not using metric
  float alt = deviceData.metric_alt ? altiudeM : (round(altiudeM * 3.28084));

  dispValue(alt, lastAltM, 5, 0, 70, 102, 2, BLACK, WHITE);

  display.print(deviceData.metric_alt ? F("m") : F("ft"));
  lastAltM = alt;
}

// display temperature data on screen
void displayTemp() {
  int tempC = hubData.baroTemp / 100.0F;
  int tempF = tempC * 9/5 + 32;

  display.print(deviceData.metric_temp ? tempC : tempF, 1);
  display.println(deviceData.metric_temp ? F("c") : F("f"));
}

// display first page (voltage and current)
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

// display second page (mAh and armed time)
void displayPage1() {
  float amph = hubData.totalMah / 10;
  display.print(amph, 1);
  display.setTextSize(2);
  display.println(F("ah"));
  addVSpace();
  display.setTextSize(3);
  displayTime(armedSecs);
}

// display third page (battery percent and kw)
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

// display fourth page (if compatible) (temperature and altitude)
void displayPage3() {
  if (!use_hub_v2) {
    display.setTextSize(3);
    display.println(F("update"));
    return;
  }
  display.setTextSize(2);
  displayTemp();
  addVSpace();
  display.setTextSize(3);
  displayAlt();
}

// display hidden page (firmware version and total armed time)
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
  // addVSpace();
  // display.print(chipId()); // TODO: trim down
}

// display hidden page (firmware version and total armed time)
void displayMessage(char *message) {
  //display.clearDisplay();
  display.setCursor(0, 0);
  display.setTextSize(2);
  display.println(message);
}

