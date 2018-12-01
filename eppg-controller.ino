// Copyright 2018 <Zach Whitehead>
// OpenPPG


#include <AceButton.h>
#include <Adafruit_DRV2605.h> // haptic controller
#include <Adafruit_SSD1306.h> // screen
#include <AdjustableButtonConfig.h>
#include <ResponsiveAnalogRead.h> // smoothing for throttle
#include <Servo.h> // to control ESCs
#include <SPI.h>
#include <TimeLib.h>
#include <Wire.h>

using namespace ace_button;

// Arduino Pins
#define BATT_IN       A2  // Battery voltage in (3.3v max)
#define BUTTON_TOP    6   // arm/disarm button_top
#define BUTTON_SIDE   7   // secondary button_top
#define BUZZER_PIN    5   // output for buzzer speaker
#define ESC_PIN       10  // the ESC signal output
#define FULL_BATT     3430 // 60v/14s(max) = 4095(3.3v) and 50v/12s(max) = ~3430
#define LED_SW        9  // output for LED on button_top switch 
#define LED_2         0  // output for LED 2 
#define LED_3         38  // output for LED 3
#define THROTTLE_PIN  A0  // throttle pot input

Adafruit_SSD1306 display(128, 64, &Wire, 4);
Adafruit_DRV2605 drv;

Servo esc; // Creating a servo class with name of esc

ResponsiveAnalogRead pot(THROTTLE_PIN, false);
ResponsiveAnalogRead analogBatt(BATT_IN, false);
AceButton button_top(BUTTON_TOP, INPUT_PULLUP);
AceButton button_side(BUTTON_SIDE, INPUT_PULLUP);
AdjustableButtonConfig adjustableButtonConfig;

const int bgInterval = 750;  // background updates (milliseconds)

bool armed = false;
bool displayVolts = true;
char page = 'p';
unsigned long armedAtMilis = 0;
unsigned int armedSecs = 0;
unsigned long previousMillis = 0; // stores last time background tasks done

#pragma message "Warning: OpenPPG software is in beta"

void setup() {
  delay(500); // power-up safety delay
  Serial.begin(115200);
  Serial.println(F("Booting up OpenPPG V2"));
  pinMode(BUTTON_TOP, INPUT);
  pinMode(LED_SW, OUTPUT);      // set up the external LED pin
  pinMode(LED_2, OUTPUT);       // set up the internal LED2 pin
  pinMode(LED_3, OUTPUT);       // set up the internal LED3 pin
  analogReadResolution(12);     // M0 chip provides 12bit resolution
  pot.setAnalogResolution(4096);
  analogBatt.setAnalogResolution(4096);

  button_top.setButtonConfig(&adjustableButtonConfig);
  button_side.setButtonConfig(&adjustableButtonConfig);
  adjustableButtonConfig.setDebounceDelay(55);
  adjustableButtonConfig.setEventHandler(handleEvent);
  adjustableButtonConfig.setFeature(ButtonConfig::kFeatureClick);
  adjustableButtonConfig.setFeature(ButtonConfig::kFeatureDoubleClick);
  adjustableButtonConfig.setLongPressDelay(2500);
  
  initDisplay();
  
  drv.begin();
  drv.setWaveform(0, 29); // play effect 1
  drv.setWaveform(1, 29); // play effect 1
  drv.setWaveform(2, 0);  // end waveform

  // play the effect!
  drv.go();

  esc.attach(ESC_PIN);
  esc.writeMicroseconds(0); // make sure motors off
}

void blinkLED() {
  byte ledState = !digitalRead(LED_2);
  setLED(ledState);
}

void setLED(byte state) {
  digitalWrite(LED_2, state);
  digitalWrite(LED_SW, state);
}

void loop() {
  button_top.check();
  button_side.check();
  if (armed) {
    handleThrottle();
  }
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= bgInterval) {
    // handle background tasks
    previousMillis = currentMillis; // reset
    updateDisplay();
    if(!armed){ blinkLED();}
  }
}

float getBatteryVolts() {
  analogBatt.update();
  int sensorValue = analogBatt.getValue();
  float converted = sensorValue * (3.3 / FULL_BATT);
  return converted * 10;
}

byte getBatteryPercent() {
  float volts = getBatteryVolts();
  // Serial.print(voltage);
  // Serial.println(" volts");
  float percent = mapf(volts, 42, 50, 1, 100);
  // Serial.print(percent);
  // Serial.println(" percentage");
  if (percent < 0) {percent = 0;}
  else if (percent > 100) {percent = 100;}

  return round(percent);
}

void disarmSystem() {
  unsigned int disarm_melody[] = { 2093, 1976, 880};
  esc.writeMicroseconds(0);
  armed = false;
  updateDisplay();
  drv.setWaveform(0, 70); // play effect
  drv.setWaveform(1, 70); // play effect
  drv.setWaveform(2, 33);
  drv.setWaveform(3, 0);   // end waveform
  drv.go();
  // Serial.println(F("disarmed"));
  playMelody(disarm_melody, 3);
  delay(1500); // dont allow immediate rearming
}

void initDisplay() {
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C); // initialize with the I2C addr 0x3C (for the 128x32)
  // Clear the buffer.
  display.clearDisplay();
  display.setRotation(2); // for right hand throttle

  display.setTextSize(3);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.println(F("OpenPPG"));
  display.display();
  display.clearDisplay();
}

void handleThrottle() {
  pot.update();
  int rawval = pot.getValue();
  int val = map(rawval, 0, 4095, 1110, 2000); // mapping val to minimum and maximum
  esc.writeMicroseconds(val); // using val as the signal to esc
}

void armSystem(){
  unsigned int arm_melody[] = { 1760, 1976, 2093 };
  // Serial.println(F("Sending Arm Signal"));
  esc.writeMicroseconds(1000); // initialize the signal to 1000

  armed = true;
  armedAtMilis = millis();

  drv.setWaveform(0, 83); // play effect
  drv.setWaveform(1, 83); // play effect
  drv.setWaveform(2, 27);
  drv.setWaveform(3, 0);   // end waveform
  drv.go();
  
  playMelody(arm_melody, 3);

  setLED(HIGH);
}

// Utility to map float values
double mapf(double x, double in_min, double in_max, double out_min, double out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

// The event handler for the the buttons
void handleEvent(AceButton *button, uint8_t eventType, uint8_t buttonState)
{
  Serial.println(eventType);
  // TODO handle multiple buttons
  switch (eventType){
  case AceButton::kEventClicked:
    // Serial.println(F("double clicked"));
    if (armed) {
      disarmSystem();
    } else if (throttleSafe()) {
      armSystem();
    }
    break;
  }
}

// Returns true if the throttle/pot is below the safe threshold
bool throttleSafe() {
  pot.update();
  Serial.println(pot.getValue());
  if (pot.getValue() < 100) {
    return true;
  }
  return false;
}

void playMelody(unsigned int melody[], int siz) {
  for (int thisNote = 0; thisNote < siz; thisNote++) {
    // quarter note = 1000 / 4, eighth note = 1000/8, etc.
    int noteDuration = 125;
    tone(BUZZER_PIN, melody[thisNote], noteDuration);
    delay(noteDuration); // to distinguish the notes, delay a minimal time between them.    
  }
  noTone(BUZZER_PIN);
}

void updateDisplay() {
  float voltage;
  byte percentage;
  String status;

  if (armed) {
    status = F("Armed");
    armedSecs = (millis() - armedAtMilis) / 1000; // update time while armed
  } else {
    status = F("Disarmd");
  }

  display.setTextSize(3);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.println(status);
  display.setTextSize(4);

  switch (page) {
  case 'v':
    voltage = getBatteryVolts();
    display.print(voltage, 1);
    display.println(F("V"));
    page = 'p';
    break;
  case 'p':
    percentage = getBatteryPercent();
    display.print(percentage, 1);
    display.println(F("%"));
    page = 't';
    break;
  case 't':
    displayTime(armedSecs);
    page = 'v';
    break;
  default:
    display.println(F("Error"));
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
  // Serial.println();
}

// utility function for digital time display - prints leading 0
void printDigits(byte digits) {
  if (digits < 10) {
    display.print("0");
  }
  // Serial.print(digits, DEC);
  display.print(digits);
}
