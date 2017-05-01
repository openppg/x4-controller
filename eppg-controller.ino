// Zach Whitehead - Apr 30th 2017

#include <Servo.h>//Using servo library to control ESC

Servo esc; //Creating a servo class with name as esc
int led_sw = 9; // LED on button switch
const long interval = 750;   // interval at which to blink (milliseconds)
unsigned long previousMillis = 0;        // will store last time LED was updated
void setup()
{
  pinMode(LED_BUILTIN, OUTPUT); //onboard LED
  pinMode(led_sw, OUTPUT); //setup the external LED pin
  
  esc.attach(8); //Specify the esc signal pin, Here as D8

  Serial.begin(9600);
  
  checkArmRange();
  // Arming range check exited so continue
  digitalWrite(led_sw, HIGH);
  digitalWrite(LED_BUILTIN, HIGH);
  esc.writeMicroseconds(1000); //initialize the signal to 1000
}

void checkArmRange(){
  bool throttle_high = true;
  unsigned long currentMillis = millis();
  int ledState = LOW;
  
  while(throttle_high){
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
      digitalWrite(led_sw, ledState);
    }
  }
}

void loop()
{
  int val; 
  val= analogRead(A0); //Read input from analog pin a0 and store in val
  
  val= map(val, 0, 1023,1000,2000); //mapping val to minimum and maximum(Change if needed) 
  esc.writeMicroseconds(val); //using val as the signal to esc
}
