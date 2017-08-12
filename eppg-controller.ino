// Zach Whitehead - 2017

#include <Servo.h> //To control ESCs
#include <FastLED.h> //For RGB lights

// Arduino Pins
#define THROTTLE_PIN  A0 // throttle pot input
#define HAPTIC_PIN    3 // Vibration motor
#define LED_PIN       5 // LED strip
#define LED_SW        8 // output for LED on button switch 
#define ESC_PIN       9 // the ESC signal output 

#define NUM_LEDS    10
#define BRIGHTNESS  200
#define LED_TYPE    WS2811
#define COLOR_ORDER GRB

CRGB leds[NUM_LEDS];
Servo esc; //Creating a servo class with name as esc

const long interval = 750; // interval at which to blink (milliseconds)
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
  checkArmRange();
  // Arming range check exited so continue

  digitalWrite(LED_SW, HIGH);
  digitalWrite(LED_BUILTIN, HIGH);

  analogWrite(HAPTIC_PIN, 200);
  delay(1500); // TODO move to timer with callback
  analogWrite(HAPTIC_PIN, 0);

  // TODO indicate armed on LED strip
  Serial.println("Sending Arm Signal");
  esc.writeMicroseconds(1000); //initialize the signal to 1000
  fill_solid(currentPalette, 16, CRGB::Green);
  FillLEDsFromPaletteColors(0);
  FastLED.show();

}

void checkArmRange() {
  bool throttle_high = true;
  unsigned long currentMillis = millis();
  int ledState = LOW;
  Serial.println("Checking throttle");
  fill_solid(currentPalette, 16, CRGB::Orange);
  FillLEDsFromPaletteColors(0);
  FastLED.show();

  while (throttle_high) {
    // TODO indicate unarmed on LED strip
    unsigned long currentMillis = millis();
    if (currentMillis - previousMillis >= interval) {
      // save the last time throttle checked and LED blinked

      previousMillis = currentMillis;
      Serial.println(analogRead(THROTTLE_PIN));
      if (analogRead(THROTTLE_PIN) < 100) {
        throttle_high = false;
      }
      // if the LED is off turn it on and vice-versa:
      if (ledState == LOW) {
        ledState = HIGH;
      } else {
        ledState = LOW;
      }

      // set the LED with the ledState of the variable:
      digitalWrite(LED_BUILTIN, ledState);
      digitalWrite(LED_SW, ledState);
    }
  }
}

void loop() {
  // TODO check and display battery voltage
  int val;
  val = analogRead(THROTTLE_PIN);
  if (val < 100) {
    val = 0;
  } //deadband
  val = map(val, 100, 1023, 1000, 2000); //mapping val to minimum and maximum 
  esc.writeMicroseconds(val); //using val as the signal to esc
}

void FillLEDsFromPaletteColors(uint8_t colorIndex) {
  uint8_t brightness = 255;

  for (int i = 0; i < NUM_LEDS; i++) {
    leds[i] = ColorFromPalette(currentPalette, colorIndex, brightness, currentBlending);
    colorIndex += 3;
  }
}

