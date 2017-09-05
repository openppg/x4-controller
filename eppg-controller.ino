// Zach Whitehead - 2017

#include <Servo.h> //To control ESCs
#include <FastLED.h> //For RGB lights
#include <ResponsiveAnalogRead.h>

// Arduino Pins
#define THROTTLE_PIN  A7 // throttle pot input
#define HAPTIC_PIN    3  // Vibration motor
#define LED_PIN       5  // LED strip
#define LED_SW        8  // output for LED on button switch 
#define ESC_PIN       2  // the ESC signal output 
#define BATT_IN       A6 //Battery voltage in (5v max)

#define NUM_LEDS    10
#define BRIGHTNESS  225
#define LED_TYPE    WS2811
#define COLOR_ORDER GRB

CRGB leds[NUM_LEDS];
Servo esc; //Creating a servo class with name as esc

ResponsiveAnalogRead analog(THROTTLE_PIN, false);

const long initInterval = 750; // throttle check (milliseconds)
const long bgInterval = 1500;  // background updates (milliseconds)

unsigned long previousMillis = 0; // will store last time LED was updated

CRGBPalette16 currentPalette;
TBlendType currentBlending;

void setup() {
  delay(1500); // power-up safety delay
  Serial.begin(9600);
  pinMode(LED_BUILTIN, OUTPUT); //onboard LED
  pinMode(LED_SW, OUTPUT); //setup the external LED pin
  pinMode(HAPTIC_PIN, OUTPUT);
  FastLED.addLeds < LED_TYPE, LED_PIN, COLOR_ORDER > (leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
  FastLED.setBrightness(BRIGHTNESS);

  currentBlending = LINEARBLEND;

  esc.attach(ESC_PIN);
  esc.writeMicroseconds(0); //make sure off

  handleBattery();
  checkArmRange();
  // Arming range check exited so continue

  digitalWrite(LED_SW, HIGH);
  digitalWrite(LED_BUILTIN, HIGH);

  // analogWrite(HAPTIC_PIN, 200);
  // delay(1500); // TODO move to timer with callback
  // analogWrite(HAPTIC_PIN, 0);

  Serial.println(F("Sending Arm Signal"));
  setAllLeds(CRGB::Gray);
  esc.writeMicroseconds(1000); //initialize the signal to 1000
}

void checkArmRange() {
  bool throttle_high = true;
  unsigned long currentMillis = millis();
  int ledState = LOW;
  Serial.println(F("Checking throttle"));

  while (throttle_high) {
    unsigned long currentMillis = millis();
    if (currentMillis - previousMillis >= initInterval) {
      // save the last time throttle checked and LED blinked

      previousMillis = currentMillis;
      analog.update();

      if (analog.getValue() < 100) {
        throttle_high = false;
      }

      // if the LED is off turn it on and vice-versa:
      if (ledState == LOW) {
        setAllLeds(CRGB::Blue);
        ledState = HIGH;
      } else {
        setAllLeds(CRGB::Red);
        ledState = LOW;
      }
 
      // set the LED with the ledState of the variable:
      digitalWrite(LED_BUILTIN, ledState);
      digitalWrite(LED_SW, ledState);
    }
  }
}

void loop() {
  handleThrottle();
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= bgInterval) {
    // handle background tasks
    previousMillis = currentMillis; // reset
    handleBattery();
  }
}

void handleBattery() {
  CRGB ledColor;
  int sensorValue = analogRead(BATT_IN);
  float voltage = sensorValue * (5.0 / 1023.0);
  float percent = mapf(voltage, 4.2, 5, 1, NUM_LEDS);
  int numLedsToLight = 1;
  
  if (percent < 0) {percent = 0;}
  
  int lights = round(percent);
  numLedsToLight = round(percent);
  if (numLedsToLight < 1) {numLedsToLight =1;}
  
  Serial.print(voltage);

  if (numLedsToLight <= 2) {
    ledColor = CRGB::Red;
  } else if (numLedsToLight == 3) {
    ledColor = CRGB::Orange;
  } else {
    ledColor = CRGB::Green;
  }
  FastLED.clear();
  for (int led = 0; led < numLedsToLight; led++) {
    leds[led] = ledColor;
  }
  FastLED.show();
}

void handleThrottle() {
  analog.update();
  int rawval = analog.getValue();
  int val = map(rawval, 0, 1023, 1000, 2000); //mapping val to minimum and maximum
  esc.writeMicroseconds(val); //using val as the signal to esc
}

void setAllLeds(CRGB color) {
  FastLED.clear();
  fill_solid(leds, NUM_LEDS, color);
  FastLED.show();
}

double mapf(double x, double in_min, double in_max, double out_min, double out_max)
{
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}
