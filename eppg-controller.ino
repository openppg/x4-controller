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
#define BUZZER_PIN    8  // output for buzzer speaker
#define LED_SW        4  // output for LED on button switch 
#define ESC_PIN       10 // the ESC signal output 
#define BATT_IN       A6 // Battery voltage in (5v max)
#define OLED_RESET    4  // ?
#define BUTTON_PIN    6  // arm/disarm button

Adafruit_SSD1306 display(OLED_RESET);

Servo esc; //Creating a servo class with name as esc

ResponsiveAnalogRead analog(THROTTLE_PIN, false);
AceButton button(BUTTON_PIN);
AdjustableButtonConfig adjustableButtonConfig;

const long initInterval = 750; // throttle check (milliseconds)
const long bgInterval = 1500;  // background updates (milliseconds)

unsigned long previousMillis = 0; // will store last time LED was updated
bool armed = false;

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
  adjustableButtonConfig.setFeature(ButtonConfig::kFeatureDoubleClick);
  adjustableButtonConfig.setFeature(ButtonConfig::kFeatureLongPress);
  adjustableButtonConfig.setDebounceDelay(55);
  adjustableButtonConfig.setLongPressDelay(2500);

  //pinMode(HAPTIC_PIN, OUTPUT);
  initDisplay();

  esc.attach(ESC_PIN);
  esc.writeMicroseconds(0); //make sure off

  //handleBattery();
}

void blinkLED() {
  unsigned long currentMillis = millis(); 
  if (currentMillis - previousMillis >= initInterval) {
    // save the last time LED blinked
    previousMillis = currentMillis;
    int ledState = !digitalRead(LED_BUILTIN);

    digitalWrite(LED_BUILTIN, ledState);
    digitalWrite(LED_SW, ledState);
  }
}

void loop() {
  button.check();
  if(armed){
    handleThrottle();
    
    unsigned long currentMillis = millis();
    if (currentMillis - previousMillis >= bgInterval) {
      // handle background tasks
      previousMillis = currentMillis; // reset
      //handleBattery();
    }
  }else{
    blinkLED();
  }
}

void handleBattery() {
  int sensorValue = analogRead(BATT_IN);
  float voltage = sensorValue * (5.0 / 1023.0);
  float percent = mapf(voltage, 4.2, 5, 1, 10);
  int numLedsToLight = 1;
  
  if (percent < 0) {percent = 0;}
  
  numLedsToLight = round(percent);
  if (numLedsToLight < 1) {numLedsToLight =1;}
  
  //Serial.print(voltage);

  // TODO display battery info
  // TODO handle low battery
}

void disarmSystem(){
  int melody[] = { 2093, 1976, 880};
  Serial.println(F("disarming"));

  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.setCursor(0,0);
  display.println(F("Disarmed"));
  display.display();
  display.clearDisplay();

  esc.writeMicroseconds(0);
  armed = false;
  Serial.println(F("disarmed"));
  playMelody(melody, 3);
  delay(2000); // dont allow immediate rearming
  return;
}

void initDisplay(){
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);  // initialize with the I2C addr 0x3C (for the 128x32)

  // Clear the buffer.
  display.clearDisplay();
  display.setRotation(2); // for right hand throttle

  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.setCursor(0,0);
  display.println(F("OpenPPG"));
  display.println(F("Disarmed"));
  display.setTextSize(4);
  display.println(F("B:69%"));  //placeholder
  display.display();
  display.clearDisplay();
}

void handleThrottle() {
  analog.update();
  int rawval = analog.getValue();
  int val = map(rawval, 0, 1023, 1160, 2000); //mapping val to minimum and maximum
  // Serial.println(val);
  esc.writeMicroseconds(val); //using val as the signal to esc
}

void armSystem(){
  int melody[] = { 1760, 1976, 2093 };

  digitalWrite(LED_BUILTIN, HIGH);
  digitalWrite(LED_SW, HIGH);

  Serial.println(F("Sending Arm Signal"));
  esc.writeMicroseconds(1000); //initialize the signal to 1000

  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.setCursor(0,0);
  display.println("Armed");
  display.println("Batt: 0%");
  display.display();
  display.clearDisplay();

  digitalWrite(LED_SW, LOW);
  digitalWrite(LED_BUILTIN, HIGH);
  armed = true;
  // iterate over the notes of the melody:
  playMelody(melody, 3);
}

double mapf(double x, double in_min, double in_max, double out_min, double out_max)
{
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

// The event handler for the button.
void handleEvent(AceButton* button, uint8_t eventType, uint8_t buttonState) {

  switch (eventType) {
    case AceButton::kEventDoubleClicked:
     Serial.println(F("double clicked"));
     if(armed){ 
      disarmSystem();
     }else if(throttleSafe()){
      armSystem();
     }
     break;
    case AceButton::kEventLongPressed:
     Serial.println(F("long clicked"));
     //if(!armed && throttleSafe()){ armSystem();}
     break;
  }
}

bool throttleSafe(){
  analog.update();

  if(analog.getValue() < 100) {
    return true;
  }else{
    return false;  
  } 
}

void playMelody(int melody[], int siz){
  for (int thisNote = 0; thisNote < siz; thisNote++) {

    //quarter note = 1000 / 4, eighth note = 1000/8, etc.
    int noteDuration = 1000 / 8;
    tone(BUZZER_PIN, melody[thisNote], noteDuration);

    // to distinguish the notes, set a minimum time between them.
    delay(noteDuration);
    // stop the tone playing:
    noTone(BUZZER_PIN);
  }
}

