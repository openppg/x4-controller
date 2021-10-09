# 1 "C:\\Users\\zwhit\\AppData\\Local\\Temp\\tmp12rnhs73"
#include <Arduino.h>
# 1 "C:/Users/zwhit/Documents/repos/eppg-controller/eppg-controller.ino"



#include "libraries/crc.c"
#include "inc/config.h"
#include "inc/structs.h"
#include <AceButton.h>
#include "Adafruit_TinyUSB.h"
#include <Adafruit_BMP3XX.h>
#include <Adafruit_DRV2605.h>
#include <Adafruit_ST7735.h>
#include <Adafruit_SleepyDog.h>
#include <ArduinoJson.h>
#include <CircularBuffer.h>
#include <ResponsiveAnalogRead.h>
#include <Servo.h>
#include <SPI.h>
#include <StaticThreadController.h>
#include <Thread.h>
#include <TimeLib.h>
#include <Wire.h>
#include <extEEPROM.h>

#include "inc/sp140-globals.h"

using namespace ace_button;

Adafruit_ST7735 display = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);
Adafruit_DRV2605 vibe;


Adafruit_USBD_WebUSB usb_web;
WEBUSB_URL_DEF(landingPage, 1 , "config.openppg.com");

ResponsiveAnalogRead pot(THROTTLE_PIN, false);
AceButton button_top(BUTTON_TOP);
ButtonConfig* buttonConfig = button_top.getButtonConfig();
extEEPROM eep(kbits_64, 1, 64);
CircularBuffer<float, 50> voltageBuffer;
CircularBuffer<int, 5> potBuffer;

Thread ledBlinkThread = Thread();
Thread displayThread = Thread();
Thread throttleThread = Thread();
Thread buttonThread = Thread();
Thread telemetryThread = Thread();
Thread counterThread = Thread();
StaticThreadController<6> threads(&ledBlinkThread, &displayThread, &throttleThread,
                                  &buttonThread, &telemetryThread, &counterThread);

bool armed = false;
bool use_hub_v2 = true;
int page = 0;
float armAltM = 0;
uint32_t armedAtMilis = 0;
uint32_t cruisedAtMilisMilis = 0;
unsigned int armedSecs = 0;
unsigned int last_throttle = 0;

        
# 60 "C:/Users/zwhit/Documents/repos/eppg-controller/eppg-controller.ino"
#pragma message "Warning: OpenPPG software is in beta"
# 60 "C:/Users/zwhit/Documents/repos/eppg-controller/eppg-controller.ino"
void setup();
void setup140();
void loop();
void checkButtons();
void disarmSystem();
void handleButtonEvent(AceButton* , uint8_t eventType, uint8_t );
void initButtons();
void initDisplay();
void handleThrottle();
bool armSystem();
bool throttleSafe();
float getAltitudeM();
void updateDisplay();
void displayPage0();
void displayPage1();
void displayTime(int val);
void displayAlt();
void displayTemp();
void displayVersions();
void displayMessage(char *message);
void setCruise();
void removeCruise(bool alert);
void trackPower();
void refreshDeviceData();
void resetDeviceData();
void writeDeviceData();
void line_state_callback(bool connected);
void parse_usb_serial();
void send_usb_serial();
void handleFlightTime();
void displayTime(int val, int x, int y, uint16_t bg_color);
void dispValue(float value, float &prevVal, int maxDigits, int precision, int x, int y, int textSize, int textColor, int background);
void initBmp();
void buzzInit(bool enableBuz);
void prepareSerialRead();
void handleTelemetry();
bool enforceFletcher16();
void enforceChecksum();
void printRawSentence();
void parseData();
void vibrateAlert();
void vibrateNotify();
int limitedThrottle(int current, int last, int threshold);
float getBatteryVoltSmoothed();
float getBatteryPercent(float voltage);
uint8_t battery_sigmoidal(float voltage, uint16_t minVoltage, uint16_t maxVoltage);
double mapf(double x, double in_min, double in_max, double out_min, double out_max);
String convertToDigits(byte digits);
int nextPage();
void addVSpace();
void setLEDs(byte state);
void blinkLED();
bool runVibe(unsigned int sequence[], int siz);
bool playMelody(unsigned int melody[], int siz);
void handleArmFail();
void printDeviceData();
String chipId();
void rebootBootloader();
#line 63 "C:/Users/zwhit/Documents/repos/eppg-controller/eppg-controller.ino"
void setup() {
  usb_web.begin();
  usb_web.setLandingPage(&landingPage);
  usb_web.setLineStateCallback(line_state_callback);

  Serial.begin(115200);
  Serial5.begin(ESC_BAUD_RATE);
  Serial5.setTimeout(ESC_TIMEOUT);





  pinMode(LED_SW, OUTPUT);

  analogReadResolution(12);
  pot.setAnalogResolution(4096);
  unsigned int startup_vibes[] = { 27, 27, 0 };
  runVibe(startup_vibes, 3);

  initButtons();

  ledBlinkThread.onRun(blinkLED);
  ledBlinkThread.setInterval(500);

  displayThread.onRun(updateDisplay);
  displayThread.setInterval(100);

  buttonThread.onRun(checkButtons);
  buttonThread.setInterval(5);

  throttleThread.onRun(handleThrottle);
  throttleThread.setInterval(22);

  telemetryThread.onRun(handleTelemetry);
  telemetryThread.setInterval(50);

  counterThread.onRun(trackPower);
  counterThread.setInterval(500);

  int countdownMS = Watchdog.enable(5000);
  uint8_t eepStatus = eep.begin(eep.twiClock100kHz);
  refreshDeviceData();
  setup140();
  initDisplay();
}

void setup140() {
  esc.attach(ESC_PIN);
  esc.writeMicroseconds(ESC_DISARMED_PWM);

  buzzInit(ENABLE_BUZ);
  initBmp();
  vibe.begin();
  vibe.selectLibrary(1);
  vibe.setMode(DRV2605_MODE_INTTRIG);

  vibrateNotify();

  if (button_top.isPressedRaw()) {


    if (deviceData.performance_mode == 0) {
      deviceData.performance_mode = 1;
    } else {
      deviceData.performance_mode = 0;
    }
    writeDeviceData();
    unsigned int notify_melody[] = { 900, 1976 };
    playMelody(notify_melody, 2);
  }
}


void loop() {
  Watchdog.reset();

  if (usb_web.available()) parse_usb_serial();
  threads.run();
}

void checkButtons() {
  button_top.check();
}


void disarmSystem() {
  esc.writeMicroseconds(ESC_DISARMED_PWM);


  unsigned int disarm_melody[] = { 2093, 1976, 880 };
  unsigned int disarm_vibes[] = { 70, 33, 0 };

  armed = false;
  removeCruise(false);

  ledBlinkThread.enabled = true;
  updateDisplay();
  runVibe(disarm_vibes, 3);
  playMelody(disarm_melody, 3);

  bottom_bg_color = DEFAULT_BG_COLOR;
  display.fillRect(0, 93, 160, 40, bottom_bg_color);


  refreshDeviceData();
  deviceData.armed_time += round(armedSecs / 60);
  writeDeviceData();
  delay(1000);
}


void handleButtonEvent(AceButton* , uint8_t eventType, uint8_t ) {
  switch (eventType) {
  case AceButton::kEventDoubleClicked:
    if (armed) {
      disarmSystem();
    } else if (throttleSafe()) {
      armSystem();
    } else {
      handleArmFail();
    }
    break;
  case AceButton::kEventLongPressed:
    if (armed) {
      if (cruising) {
        removeCruise(true);
      } else {
        setCruise();
      }
    } else {

    }
    break;
  case AceButton::kEventLongReleased:
    break;
  }
}


void initButtons() {
  pinMode(BUTTON_TOP, INPUT_PULLUP);

  buttonConfig->setEventHandler(handleButtonEvent);
  buttonConfig->setFeature(ButtonConfig::kFeatureDoubleClick);
  buttonConfig->setFeature(ButtonConfig::kFeatureLongPress);
  buttonConfig->setFeature(ButtonConfig::kFeatureSuppressAfterDoubleClick);
  buttonConfig->setFeature(ButtonConfig::kFeatureSuppressAfterLongPress);
  buttonConfig->setLongPressDelay(2500);
  buttonConfig->setDoubleClickDelay(600);
}


void initDisplay() {
  display.initR(INITR_BLACKTAB);
  display.fillScreen(DEFAULT_BG_COLOR);
  display.setTextColor(BLACK);
  display.setCursor(0, 0);
  display.setTextSize(1);
  display.setTextWrap(true);

  display.setRotation(deviceData.screen_rotation);

  pinMode(TFT_LITE, OUTPUT);
  digitalWrite(TFT_LITE, HIGH);
}


void handleThrottle() {
  if (!armed) return;

  static int maxPWM = ESC_MAX_PWM;
  pot.update();
  int potRaw = pot.getValue();
  potBuffer.push(potRaw);

  int potLvl = 0;
  for (decltype(potBuffer)::index_t i = 0; i < potBuffer.size(); i++) {
    potLvl += potBuffer[i] / potBuffer.size();
  }




  if (deviceData.performance_mode == 0) {
    potLvl = limitedThrottle(potLvl, prevPotLvl, 300);
    maxPWM = 1750;
  }
  armedSecs = (millis() - armedAtMilis) / 1000;

  unsigned long cruisingSecs = (millis() - cruisedAtMilis) / 1000;

  if (cruising) {
    if (cruisingSecs >= CRUISE_GRACE && potLvl > POT_SAFE_LEVEL) {
      removeCruise(true);
    } else {
      throttlePWM = mapf(cruiseLvl, 0, 4095, ESC_MIN_PWM, maxPWM);
    }
  } else {
    throttlePWM = mapf(potLvl, 0, 4095, ESC_MIN_PWM, maxPWM);
  }
  throttlePercent = mapf(throttlePWM, ESC_MIN_PWM, ESC_MAX_PWM, 0, 100);
  throttlePercent = constrain(throttlePercent, 0, 100);

  esc.writeMicroseconds(throttlePWM);
}


bool armSystem() {
  unsigned int arm_melody[] = { 1760, 1976, 2093 };
  unsigned int arm_vibes[] = { 70, 33, 0 };

  armed = true;
  esc.writeMicroseconds(ESC_DISARMED_PWM);

  ledBlinkThread.enabled = false;
  armedAtMilis = millis();
  armAltM = getAltitudeM();

  setLEDs(HIGH);
  runVibe(arm_vibes, 3);
  playMelody(arm_melody, 3);

  bottom_bg_color = ARMED_BG_COLOR;
  display.fillRect(0, 93, 160, 40, bottom_bg_color);


  return true;
}



bool throttleSafe() {
  pot.update();
  if (pot.getValue() < POT_SAFE_LEVEL) {
    return true;
  }
  return false;
}


float getAltitudeM() {
  bmp.performReading();
  ambientTempC = bmp.temperature;
  float altitudeM = bmp.readAltitude(deviceData.sea_pressure);
  aglM = altitudeM - armAltM;
  return altitudeM;
}
# 319 "C:/Users/zwhit/Documents/repos/eppg-controller/eppg-controller.ino"
void updateDisplay() {



  displayPage0();




  display.setTextColor(BLACK);
  float avgVoltage = getBatteryVoltSmoothed();
  batteryPercent = getBatteryPercent(avgVoltage);

  if (batteryPercent >= 30) {
    display.fillRect(0, 0, mapf(batteryPercent, 0, 100, 0, 108), 36, GREEN);
  } else if (batteryPercent >= 15) {
    display.fillRect(0, 0, mapf(batteryPercent, 0, 100, 0, 108), 36, YELLOW);
  } else {
    display.fillRect(0, 0, mapf(batteryPercent, 0, 100, 0, 108), 36, RED);
  }

  if (avgVoltage < BATT_MIN_V) {
    if (batteryFlag) {
      batteryFlag = false;
      display.fillRect(0, 0, 108, 36, DEFAULT_BG_COLOR);
    }
    display.setCursor(12, 3);
    display.setTextSize(2);
    display.setTextColor(RED);
    display.println("BATTERY");

    if ( avgVoltage < 10 ) {
      display.print(" ERROR");
    } else {
      display.print(" DEAD");
    }
  } else {
    batteryFlag = true;
    display.fillRect(map(batteryPercent, 0,100, 0,108), 0, map(batteryPercent, 0,100, 108,0), 36, DEFAULT_BG_COLOR);
  }

  if (batteryPercent <= 5) {
    display.drawLine(0, 0, 108, 36, RED);
  }
  dispValue(batteryPercent, prevBatteryPercent, 3, 0, 108, 10, 2, BLACK, DEFAULT_BG_COLOR);
  display.print("%");
# 376 "C:/Users/zwhit/Documents/repos/eppg-controller/eppg-controller.ino"
  display.fillRect(0, 36, 160, 1, BLACK);
  display.fillRect(108, 0, 1, 36, BLACK);
  display.fillRect(0, 92, 160, 1, BLACK);

  displayAlt();




  handleFlightTime();
  displayTime(throttleSecs, 8, 102, bottom_bg_color);


}


void displayPage0() {
  float avgVoltage = getBatteryVoltSmoothed();

  dispValue(avgVoltage, prevVolts, 5, 1, 84, 42, 2, BLACK, DEFAULT_BG_COLOR);
  display.print("V");

  dispValue(telemetryData.amps, prevAmps, 3, 0, 108, 71, 2, BLACK, DEFAULT_BG_COLOR);
  display.print("A");

  float kWatts = watts / 1000.0;
  kWatts = constrain(kWatts, 0, 50);

  dispValue(kWatts, prevKilowatts, 4, 1, 10, 42, 2, BLACK, DEFAULT_BG_COLOR);
  display.print("kW");

  float kwh = wattsHoursUsed / 1000;
  dispValue(kwh, prevKwh, 4, 1, 10, 71, 2, BLACK, DEFAULT_BG_COLOR);
  display.print("kWh");

  display.setCursor(30, 60);
  display.setTextSize(1);
  if (deviceData.performance_mode == 0) {
    display.setTextColor(BLUE);
    display.print("CHILL");
  } else {
    display.setTextColor(RED);
    display.print("SPORT");
  }
}


void displayPage1() {
  dispValue(telemetryData.volts, prevVolts, 5, 1, 84, 42, 2, BLACK, DEFAULT_BG_COLOR);
  display.print("V");

  dispValue(telemetryData.amps, prevAmps, 3, 0, 108, 71, 2, BLACK, DEFAULT_BG_COLOR);
  display.print("A");

  float kwh = wattsHoursUsed / 1000;
  dispValue(kwh, prevKilowatts, 4, 1, 10, 71, 2, BLACK, DEFAULT_BG_COLOR);
  display.print("kWh");

  display.setCursor(30, 60);
  display.setTextSize(1);
  if (deviceData.performance_mode == 0) {
    display.setTextColor(BLUE);
    display.print("CHILL");
  } else {
    display.setTextColor(RED);
    display.print("SPORT");
  }
}


void displayTime(int val) {
  int minutes = val / 60;
  int seconds = numberOfSeconds(val);

  display.print(convertToDigits(minutes));
  display.print(":");
  display.print(convertToDigits(seconds));
}


void displayAlt() {
  float altiudeM = 0;

  if (armAltM > 0 && deviceData.sea_pressure != DEFAULT_SEA_PRESSURE) {
    altiudeM = getAltitudeM();
  } else {
    altiudeM = getAltitudeM() - armAltM;
  }


  float alt = deviceData.metric_alt ? altiudeM : (round(altiudeM * 3.28084));

  dispValue(alt, lastAltM, 5, 0, 70, 102, 2, BLACK, bottom_bg_color);

  display.print(deviceData.metric_alt ? F("m") : F("ft"));
  lastAltM = alt;
}


void displayTemp() {
  int tempC = hubData.baroTemp / 100.0F;
  int tempF = tempC * 9/5 + 32;

  display.print(deviceData.metric_temp ? tempC : tempF, 1);
  display.println(deviceData.metric_temp ? F("c") : F("f"));
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


void displayMessage(char *message) {

  display.setCursor(0, 0);
  display.setTextSize(2);
  display.println(message);
}

void setCruise() {

  if (!throttleSafe()) {
    cruiseLvl = pot.getValue();
    cruising = true;
    vibrateNotify();

    display.setCursor(70, 60);
    display.setTextSize(1);
    display.setTextColor(RED);
    display.print("CRUISE");

    unsigned int notify_melody[] = { 900, 900 };
    playMelody(notify_melody, 2);

    bottom_bg_color = YELLOW;
    display.fillRect(0, 93, 160, 40, bottom_bg_color);

    cruisedAtMilis = millis();
  }
}

void removeCruise(bool alert) {
  cruising = false;

  if (armed) {
    bottom_bg_color = ARMED_BG_COLOR;
  } else {
    bottom_bg_color = DEFAULT_BG_COLOR;
  }
  display.fillRect(0, 93, 160, 40, bottom_bg_color);

  display.setCursor(70, 60);
  display.setTextSize(1);
  display.setTextColor(DEFAULT_BG_COLOR);
  display.print("CRUISE");
  if (alert) {
    vibrateNotify();

    if (ENABLE_BUZ) {
      tone(BUZ_PIN, 500, 100);
      delay(250);
      tone(BUZ_PIN, 500, 100);
    }
  }
}

void trackPower() {

  if (armed) {
    wattsHoursUsed += watts/60/60/2;
  }
}
# 1 "C:/Users/zwhit/Documents/repos/eppg-controller/extra-data.ino"





void refreshDeviceData() {
  static int offset = 0;

  uint8_t tempBuf[sizeof(deviceData)];
  if (0 != eep.read(offset, tempBuf, sizeof(deviceData))) {

  }
  memcpy((uint8_t*)&deviceData, tempBuf, sizeof(deviceData));
  uint16_t crc = crc16((uint8_t*)&deviceData, sizeof(deviceData) - 2);

  if (crc != deviceData.crc) {

    resetDeviceData();
    return;
  }
}

void resetDeviceData() {
    deviceData = STR_DEVICE_DATA_140_V1();
    deviceData.version_major = VERSION_MAJOR;
    deviceData.version_minor = VERSION_MINOR;
    deviceData.screen_rotation = 3;
    deviceData.sea_pressure = DEFAULT_SEA_PRESSURE;
    deviceData.metric_temp = true;
    deviceData.metric_alt = true;
    deviceData.performance_mode = 0;
    deviceData.batt_size = 4000;
    writeDeviceData();
}

void writeDeviceData() {
  deviceData.crc = crc16((uint8_t*)&deviceData, sizeof(deviceData) - 2);
  int offset = 0;

  if (0 != eep.write(offset, (uint8_t*)&deviceData, sizeof(deviceData))) {
    Serial.println(F("error writing EEPROM"));
  }
}



void line_state_callback(bool connected) {
  digitalWrite(LED_2, connected);

  if ( connected ) send_usb_serial();
}


void parse_usb_serial() {
  const size_t capacity = JSON_OBJECT_SIZE(12) + 90;
  DynamicJsonDocument doc(capacity);
  deserializeJson(doc, usb_web);

  if (doc["command"] && doc["command"] == "rbl") {
    display.fillScreen(DEFAULT_BG_COLOR);
    displayMessage("BL - UF2");
    rebootBootloader();
    return;
  }

  if (doc["major_v"] < 5) return;

  deviceData.screen_rotation = doc["screen_rot"];
  deviceData.sea_pressure = doc["sea_pressure"];
  deviceData.metric_temp = doc["metric_temp"];
  deviceData.metric_alt = doc["metric_alt"];
  deviceData.performance_mode = doc["performance_mode"];
  deviceData.batt_size = doc["batt_size"];
  initDisplay();
  writeDeviceData();
  send_usb_serial();
}

void send_usb_serial() {
  const size_t capacity = JSON_OBJECT_SIZE(11) + 90;
  DynamicJsonDocument doc(capacity);

  doc["major_v"] = VERSION_MAJOR;
  doc["minor_v"] = VERSION_MINOR;
  doc["screen_rot"] = deviceData.screen_rotation;
  doc["armed_time"] = deviceData.armed_time;
  doc["metric_temp"] = deviceData.metric_temp;
  doc["metric_alt"] = deviceData.metric_alt;
  doc["performance_mode"] = deviceData.performance_mode;
  doc["sea_pressure"] = deviceData.sea_pressure;
  doc["device_id"] = chipId();

  char output[256];
  serializeJson(doc, output);
  usb_web.println(output);
}
# 1 "C:/Users/zwhit/Documents/repos/eppg-controller/sp140.ino"


void handleFlightTime() {
  if (!armed) {
    throttledFlag = true;
    throttled = false;
  }
  if (armed) {
    if (throttlePercent > 30 && throttledFlag) {
      throttledAtMillis = millis();
      throttledFlag = false;
      throttled = true;
    }
    if (throttled) {
      throttleSecs = (millis()-throttledAtMillis) / 1000.0;
    } else {
      throttleSecs = 0;
    }
  }
}


void displayTime(int val, int x, int y, uint16_t bg_color) {

  display.setCursor(x, y);
  display.setTextSize(2);
  display.setTextColor(BLACK);
  minutes = val / 60;
  seconds = numberOfSeconds(val);
  if (minutes < 10) {
    display.setCursor(x, y);
    display.print("0");
  }
  dispValue(minutes, prevMinutes, 2, 0, x, y, 2, BLACK, bg_color);
  display.setCursor(x+24, y);
  display.print(":");
  display.setCursor(x+36, y);
  if (seconds < 10) {
    display.print("0");
  }
  dispValue(seconds, prevSeconds, 2, 0, x+36, y, 2, BLACK, bg_color);
}
# 51 "C:/Users/zwhit/Documents/repos/eppg-controller/sp140.ino"
void dispValue(float value, float &prevVal, int maxDigits, int precision, int x, int y, int textSize, int textColor, int background){
  int numDigits = 0;
  char prevDigit[DIGIT_ARRAY_SIZE] = {};
  char digit[DIGIT_ARRAY_SIZE] = {};
  String prevValTxt = String(prevVal, precision);
  String valTxt = String(value, precision);
  prevValTxt.toCharArray(prevDigit, maxDigits+1);
  valTxt.toCharArray(digit, maxDigits+1);


  for(int i=0; i<maxDigits; i++){
    if (digit[i]) {
      numDigits++;
    }
  }

  display.setTextSize(textSize);
  display.setCursor(x, y);


  display.setTextColor(background);
  for(int i=0; i<(maxDigits-numDigits); i++){
    display.print(char(218));
  }
  display.setTextColor(textColor);


  for (int i=0; i<numDigits; i++) {
    if (digit[i]!=prevDigit[i]) {
      display.setTextColor(background);
      display.print(char(218));
    } else {
      display.setTextColor(textColor);
      display.print(digit[i]);
    }
  }


  display.setCursor(x, y);


  display.setTextColor(background);
  for (int i=0; i<(maxDigits-numDigits); i++) {
    display.print(char(218));
  }
  display.setTextColor(textColor);


  for(int i=0; i<numDigits; i++){
    display.print(digit[i]);
  }

  prevVal = value;
}

void initBmp() {
  bmp.begin();
  bmp.setTemperatureOversampling(BMP3_OVERSAMPLING_8X);
  bmp.setPressureOversampling(BMP3_OVERSAMPLING_4X);
  bmp.setIIRFilterCoeff(BMP3_IIR_FILTER_COEFF_3);
}

void buzzInit(bool enableBuz) {
  pinMode(BUZ_PIN, OUTPUT);
  return;

  if (enableBuz) {
    tone(BUZ_PIN, 500);
    delay(200);
    tone(BUZ_PIN, 700);
    delay(200);
    tone(BUZ_PIN, 900);
    delay(200);
    noTone(BUZ_PIN);
  }
}

void prepareSerialRead() {
  while (Serial5.available() > 0) {
    byte t = Serial5.read();
  }
}

void handleTelemetry() {
  prepareSerialRead();
  Serial5.readBytes(escData, ESC_DATA_SIZE);

  if (enforceFletcher16()) {
    parseData();
  }

}

bool enforceFletcher16() {

  word checksum = (unsigned short)(word(escData[19], escData[18]));
  unsigned char sum1 = 0;
  unsigned char sum2 = 0;
  unsigned short sum = 0;
  for (int i = 0; i < ESC_DATA_SIZE-2; i++) {
    sum1 = (unsigned char)(sum1 + escData[i]);
    sum2 = (unsigned char)(sum2 + sum1);
  }
  sum = (unsigned char)(sum1 - sum2);
  sum = sum << 8;
  sum |= (unsigned char)(sum2 - 2*sum1);







  if (sum != checksum) {

    failed++;
    if (failed >= 1000) {
      transmitted = 1;
      failed = 0;
    }
    return false;
  }
  return true;
}


void enforceChecksum() {

  word checksum = word(escData[19], escData[18]);
  int sum = 0;
  for (int i=0; i<ESC_DATA_SIZE-2; i++) {
    sum += escData[i];
  }
  Serial.print(F("     SUM: "));
  Serial.println(sum);
  Serial.print(F("CHECKSUM: "));
  Serial.println(checksum);
  if (sum != checksum) {
    Serial.println(F("__________________________CHECKSUM FAILED!"));
    failed++;
    if (failed >= 1000) {
      transmitted = 1;
      failed = 0;
    }
    for (int i=0; i<ESC_DATA_SIZE; i++) {
      escData[i] = prevData[i];
    }
  }
  for (int i=0; i<ESC_DATA_SIZE; i++) {
    prevData[i] = escData[i];
  }
}


void printRawSentence() {
  Serial.print(F("DATA: "));
  for (int i = 0; i < ESC_DATA_SIZE; i++) {
    Serial.print(escData[i], HEX);
    Serial.print(F(" "));
  }
  Serial.println();
}


void parseData() {



  _volts = word(escData[1], escData[0]);

  telemetryData.volts = _volts / 100.0;

  if (telemetryData.volts > BATT_MIN_V) {
    telemetryData.volts += 1.5;
  }

  voltageBuffer.push(telemetryData.volts);






  _temperatureC = word(escData[3], escData[2]);
  telemetryData.temperatureC = _temperatureC/100.0;




  _amps = word(escData[5], escData[4]);
  telemetryData.amps = _amps;




  watts = telemetryData.amps * telemetryData.volts;



  _eRPM = escData[11];
  _eRPM << 8;
  _eRPM += escData[10];
  _eRPM << 8;
  _eRPM += escData[9];
  _eRPM << 8;
  _eRPM += escData[8];
  telemetryData.eRPM = _eRPM/6.0/2.0;




  _inPWM = word(escData[13], escData[12]);
  telemetryData.inPWM = _inPWM/100.0;




  _outPWM = word(escData[15], escData[14]);
  telemetryData.outPWM = _outPWM/100.0;






  telemetryData.checksum = word(escData[19], escData[18]);







}

void vibrateAlert() {
  if (!ENABLE_VIB) { return; }
  int effect = 15;
  vibe.setWaveform(0, effect);
  vibe.setWaveform(1, 0);
  vibe.go();
}

void vibrateNotify() {
  if (!ENABLE_VIB) { return; }

  vibe.setWaveform(0, 15);
  vibe.setWaveform(1, 0);
  vibe.go();
}


int limitedThrottle(int current, int last, int threshold) {
  prevPotLvl = current;

  if (current > last) {
    if (current - last >= threshold) {
      int limitedThrottle = last + threshold;
      prevPotLvl = limitedThrottle;
      return limitedThrottle;
    } else {
      return current;
    }
  } else {
    return current;
  }
}

float getBatteryVoltSmoothed() {
  float avg = 0.0;

  if (voltageBuffer.isEmpty()) { return avg; }

  using index_t = decltype(voltageBuffer)::index_t;
  for (index_t i = 0; i < voltageBuffer.size(); i++) {
    avg += voltageBuffer[i] / voltageBuffer.size();
  }
  return avg;
}

float getBatteryPercent(float voltage) {



  int battPercent = 0;
  if (voltage < BATT_MIN_V) { return battPercent; }

  if (telemetryData.volts >= BATT_MID_V) {
    battPercent = mapf(voltage, BATT_MID_V, BATT_MAX_V, 50.0, 100.0);
  } else {
    battPercent = mapf(voltage, BATT_MIN_V, BATT_MID_V, 0.0, 50.0);
  }

  return constrain(battPercent, 0, 100);
}



uint8_t battery_sigmoidal(float voltage, uint16_t minVoltage, uint16_t maxVoltage) {
  uint8_t result = 105 - (105 / (1 + pow(1.724 * (voltage - minVoltage)/(maxVoltage - minVoltage), 5.5)));
  return result >= 100 ? 100 : result;
}
# 1 "C:/Users/zwhit/Documents/repos/eppg-controller/utilities.ino"



#define LAST_PAGE 1

#define DBL_TAP_PTR ((volatile uint32_t *)(HMCRAMC0_ADDR + HMCRAMC0_SIZE - 4))
#define DBL_TAP_MAGIC 0xf01669ef
#define DBL_TAP_MAGIC_QUICK_BOOT 0xf02669ef


double mapf(double x, double in_min, double in_max, double out_min, double out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}







String convertToDigits(byte digits) {
  String digits_string = "";
  if (digits < 10) digits_string.concat("0");
  digits_string.concat(digits);
  return digits_string;
}






int nextPage() {
  display.fillRect(0, 37, 160, 54, DEFAULT_BG_COLOR);

  if (page >= LAST_PAGE) {
    return page = 0;
  }
  return ++page;
}

void addVSpace() {
  display.setTextSize(1);
  display.println(" ");
}

void setLEDs(byte state) {


  digitalWrite(LED_SW, state);
}


void blinkLED() {
  byte ledState = !digitalRead(LED_SW);
  setLEDs(ledState);
}

bool runVibe(unsigned int sequence[], int siz) {
  if (!ENABLE_VIB) { return false; }

  vibe.begin();
  for (int thisNote = 0; thisNote < siz; thisNote++) {
    vibe.setWaveform(thisNote, sequence[thisNote]);
  }
  vibe.go();
  return true;
}

bool playMelody(unsigned int melody[], int siz) {
  if (!ENABLE_BUZ) { return false; }

  for (int thisNote = 0; thisNote < siz; thisNote++) {

    int noteDuration = 125;
    tone(BUZZER_PIN, melody[thisNote], noteDuration);
    delay(noteDuration);
  }
  noTone(BUZZER_PIN);
  return true;
}

void handleArmFail() {
  unsigned int arm_fail_melody[] = { 820, 640 };
  playMelody(arm_fail_melody, 2);
}


void printDeviceData() {
  Serial.print("version major ");
  Serial.println(deviceData.version_major);
  Serial.print("version minor ");
  Serial.println(deviceData.version_minor);
  Serial.print("armed_time ");
  Serial.println(deviceData.armed_time);
  Serial.print("crc ");
  Serial.println(deviceData.crc);
}


String chipId() {
  volatile uint32_t val1, val2, val3, val4;
  volatile uint32_t *ptr1 = (volatile uint32_t *)0x0080A00C;
  val1 = *ptr1;
  volatile uint32_t *ptr = (volatile uint32_t *)0x0080A040;
  val2 = *ptr;
  ptr++;
  val3 = *ptr;
  ptr++;
  val4 = *ptr;

  char id_buf[33];
  sprintf(id_buf, "%8x%8x%8x%8x", val1, val2, val3, val4);
  return String(id_buf);
}


void(* resetFunc) (void) = 0;


void rebootBootloader() {
  *DBL_TAP_PTR = DBL_TAP_MAGIC;

  resetFunc();
}