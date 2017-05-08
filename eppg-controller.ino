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

const long interval = 750;   // interval at which to blink (milliseconds)
unsigned long previousMillis = 0;     // will store last time LED was updated

CRGBPalette16 currentPalette;
TBlendType    currentBlending;

void setup()
{
  delay( 1500 ); // power-up safety delay
  Serial.begin(9600);
  pinMode(LED_BUILTIN, OUTPUT); //onboard LED
  pinMode(LED_SW, OUTPUT); //setup the external LED pin
  pinMode(HAPTIC_PIN, OUTPUT); 
  FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection( TypicalLEDStrip );
  FastLED.setBrightness(  BRIGHTNESS );

  currentBlending = LINEARBLEND;
  SetupBlackAndRedStripedPalette();
  
  esc.attach(ESC_PIN);
  
  checkArmRange();
  // Arming range check exited so continue
  digitalWrite(LED_SW, HIGH);
  digitalWrite(LED_BUILTIN, HIGH);
  
  analogWrite(HAPTIC_PIN, 200);
  delay( 1500 ); // TODO move to timer with callback
  analogWrite(HAPTIC_PIN, 0);
  
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
  static uint8_t startIndex = 0;
  startIndex = startIndex + 1; /* motion speed */
    
  FillLEDsFromPaletteColors( startIndex);
  FastLED.show();
  //FastLED.delay(20);

  // TODO check and display battery voltage
  int val; 
  val= analogRead(THROTTLE_PIN);
  
  val= map(val, 0, 1023,1000,2000); //mapping val to minimum and maximum 
  esc.writeMicroseconds(val); //using val as the signal to esc
}

void FillLEDsFromPaletteColors( uint8_t colorIndex)
{
    uint8_t brightness = 255;
    
    for( int i = 0; i < NUM_LEDS; i++) {
        leds[i] = ColorFromPalette( currentPalette, colorIndex, brightness, currentBlending);
        colorIndex += 3;
    }
}

void SetupBlackAndRedStripedPalette()
{
    // 'black out' all 16 palette entries...
    fill_solid( currentPalette, 16, CRGB::Black);
    // and set every fourth one to white.
    currentPalette[0] = CRGB::Red;
    currentPalette[2] = CRGB::Orange;
    currentPalette[4] = CRGB::Red;
    currentPalette[6] = CRGB::Orange;
    currentPalette[8] = CRGB::Red;
    currentPalette[10] = CRGB::Orange;
    currentPalette[12] = CRGB::Red;
    currentPalette[14] = CRGB::Orange;
}

