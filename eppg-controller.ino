// Zach Whitehead - 2017

#include <Servo.h> //To control ESCs
#include <FastLED.h> //For RGB lights

// Arduino Pins
#define LED_SW        9 // output for LED on button switch
#define ESC_PIN       8 // the ESC signal output pin 
#define THROTTLE_PIN  A0 // throttle pot input 
#define LED_PIN       5 // LED strip

#define NUM_LEDS    10
#define BRIGHTNESS  64
#define LED_TYPE    WS2811
#define COLOR_ORDER GRB

CRGB leds[NUM_LEDS];
Servo esc; //Creating a servo class with name as esc

const long interval = 750;   // interval at which to blink (milliseconds)
unsigned long previousMillis = 0;     // will store last time LED was updated
void setup()
{
  delay( 1500 ); // power-up safety delay
  pinMode(LED_BUILTIN, OUTPUT); //onboard LED
  pinMode(LED_SW, OUTPUT); //setup the external LED pin
  FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection( TypicalLEDStrip );
  FastLED.setBrightness(  BRIGHTNESS );
  
  esc.attach(ESC_PIN);

  Serial.begin(9600);
  
  checkArmRange();
  // Arming range check exited so continue
  digitalWrite(LED_SW, HIGH);
  digitalWrite(LED_BUILTIN, HIGH);
  // TODO indicate armed on LED strip
  esc.writeMicroseconds(1000); //initialize the signal to 1000
}

void checkArmRange(){
  bool throttle_high = true;
  unsigned long currentMillis = millis();
  int ledState = LOW;
  
  while(throttle_high){
    // TODO indicate unarmed on LED strip
    unsigned long currentMillis = millis();

    if (currentMillis - previousMillis >= interval) {
      // save the last time throttle checked and LED blinked
      previousMillis = currentMillis;
      if (analogRead(A0) < 10){
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

void loop()
{
  // TODO check and display battery voltage
  int val; 
  val= analogRead(THROTTLE_PIN);
  
  val= map(val, 0, 1023,1000,2000); //mapping val to minimum and maximum(Change if needed) 
  esc.writeMicroseconds(val); //using val as the signal to esc
}

