#include <Time.h>
#include <TimeLib.h>

// Zach Whitehead - 2018

#include <Servo.h> // to control ESCs
#include <ResponsiveAnalogRead.h> // smoothing for throttle
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_SSD1306.h> // screen
#include <AceButton.h>
#include <AdjustableButtonConfig.h>

using namespace ace_button;

// Arduino Pins
#define THROTTLE_PIN  A7 // throttle pot input
#define HAPTIC_PIN    3  // vibration motor - not used in V1
#define BUZZER_PIN    5  // output for buzzer speaker
#define LED_SW        4  // output for LED on button switch 
#define ESC_PIN       10 // the ESC signal output 
#define BATT_IN       A6 // Battery voltage in (5v max)
#define OLED_RESET    4  // ?
#define BUTTON_PIN    7  // arm/disarm button
#define FULL_BATT    920 // 60v/14s(max) = 1023(5v) and 50v/12s(max) = ~920

Adafruit_SSD1306 display(OLED_RESET);

Servo esc; //Creating a servo class with name as esc

ResponsiveAnalogRead analog(THROTTLE_PIN, false);
ResponsiveAnalogRead analogBatt(BATT_IN, false);
AceButton button(BUTTON_PIN);
AdjustableButtonConfig adjustableButtonConfig;

const long bgInterval = 500;  // background updates (milliseconds)

unsigned long previousMillis = 0; // will store last time LED was updated
unsigned long armedAtMilis = 0;
bool armed = false;
bool displayVolts = true;

char page = 'p';

#pragma message "Warning: OpenPPG software is in beta"

void setup() {
  delay(500); // power-up safety delay
  Serial.begin(9600);
  Serial.println(F("Booting up OpenPPG"));
  pinMode(LED_BUILTIN, OUTPUT); //onboard LED
  pinMode(LED_SW, OUTPUT); //setup the external LED pin
  pinMode(BUTTON_PIN, INPUT);

  button.setButtonConfig(&adjustableButtonConfig);
  adjustableButtonConfig.setEventHandler(handleEvent);
  adjustableButtonConfig.setFeature(ButtonConfig::kFeatureClick);
  adjustableButtonConfig.setFeature(ButtonConfig::kFeatureDoubleClick);
  adjustableButtonConfig.setDebounceDelay(55);
  adjustableButtonConfig.setLongPressDelay(2500);

  //pinMode(HAPTIC_PIN, OUTPUT);
  initDisplay();

  esc.attach(ESC_PIN);
  esc.writeMicroseconds(0); //make sure off
  armedAtMilis = millis(); //TODO move to arm
  //handleBattery();
}

void blinkLED() {
  int ledState = !digitalRead(LED_BUILTIN);

  digitalWrite(LED_BUILTIN, ledState);
  digitalWrite(LED_SW, ledState);
}

void loop() {
  button.check();
  if(armed){
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

float getBatteryVolts(){
  analogBatt.update();
  int sensorValue = analogBatt.getValue();
  //Serial.print(sensorValue);
  //Serial.println(" sensor");
  float converted = sensorValue * (5.0 / FULL_BATT);
  return converted *10;
}

int getBatteryPercent() {
  float volts = getBatteryVolts();
  //Serial.print(voltage);
  //Serial.println(" volts");
  float percent = mapf(volts, 42, 50, 1, 100);
  //Serial.print(percent);
  //Serial.println(" percentage");
  if (percent < 0) {percent = 0;}
  else if (percent > 100) {percent = 100;}

  return round(percent);

  // TODO handle low battery
}

void disarmSystem(){
  int melody[] = { 2093, 1976, 880};
  esc.writeMicroseconds(0);
  armed = false;
  updateDisplay();
  Serial.println(F("disarmed"));
  playMelody(melody, 3);
  delay(2000); // dont allow immediate rearming
  return;
}

void initDisplay(){
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);  // initialize with the I2C addr 0x3C (for the 128x32)
  //float volts = getBatteryVolts();
  // Clear the buffer.
  display.clearDisplay();
  display.setRotation(2); // for right hand throttle

  display.setTextSize(3);
  display.setTextColor(WHITE);
  display.setCursor(0,0);
  display.println(F("OpenPPG"));
  display.display();
  display.clearDisplay();
}

void handleThrottle() {
  analog.update();
  int rawval = analog.getValue();
  int val = map(rawval, 0, 1023, 1110, 2000); //mapping val to minimum and maximum
  // Serial.println(val);
  esc.writeMicroseconds(val); //using val as the signal to esc
}

void armSystem(){
  int melody[] = { 1760, 1976, 2093 };

  Serial.println(F("Sending Arm Signal"));
  esc.writeMicroseconds(1000); //initialize the signal to 1000

  armed = true;
  playMelody(melody, 3);
  digitalWrite(LED_SW, HIGH);
  digitalWrite(LED_BUILTIN, HIGH);
}

double mapf(double x, double in_min, double in_max, double out_min, double out_max)
{
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

// The event handler for the button.
void handleEvent(AceButton* button, uint8_t eventType, uint8_t buttonState) {

  switch (eventType) {
    case AceButton::kEventClicked:
     Serial.println(F("double clicked"));
     if(armed){ 
      disarmSystem();
     }else if(throttleSafe()){
      armSystem();
     }
     break;
  }
}

bool throttleSafe(){
  analog.update();
  if(analog.getValue() < 100) {
    return true;
  }
  return false; 
}

void playMelody(int melody[], int siz){
  for (int thisNote = 0; thisNote < siz; thisNote++) {
    //quarter note = 1000 / 4, eighth note = 1000/8, etc.
    int noteDuration = 125;
    tone(BUZZER_PIN, melody[thisNote], noteDuration);

    // to distinguish the notes, set a minimum time between them.
    delay(noteDuration);
    // stop the tone playing:
    noTone(BUZZER_PIN);
  }
}

void updateDisplay(){
  String status = (armed) ? "Armed" : "Disarmd";
 
  display.setTextSize(3);
  display.setTextColor(WHITE);
  display.setCursor(0,0);
  display.println(status);
  display.setTextSize(4);

  float voltage = getBatteryVolts();
  int percentage = getBatteryPercent();
  long elapsedSec = (millis() - armedAtMilis) / 1000;
  switch (page) {
    case 'v':
      display.print(voltage, 1); 
      display.println(F("V"));
      page = 'p';
      break;
    case 'p':
      display.print(percentage, 1);
      display.println(F("%"));
      page = 't';
      break;
    case 't':
      displayTime(elapsedSec);
      page = 'v';
      break;
     default:
      display.println(F("Error"));
      break;
  }
  display.display();
  display.clearDisplay();
}

void displayTime(long val){   
  int minutes = val/60;
  int seconds = numberOfSeconds(val);
  
   // digital clock display of current time
   printDigits(minutes);
   display.print(":");
   printDigits(seconds);
   Serial.println();  
}

void printDigits(byte digits){
 // utility function for digital clock display: printsleading 0
 if(digits < 10){
   Serial.print("0");
   display.print("0");
  }
   Serial.print(digits,DEC); 
   display.print(digits);
}
