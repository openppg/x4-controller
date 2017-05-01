// Zach Whitehead - Apr 30th 2017

#include <Servo.h>//Using servo library to control ESC

Servo esc; //Creating a servo class with name as esc
int led_sw = 9; // LED on button switch

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
  while(throttle_high){
    digitalWrite(led_sw, HIGH);
    digitalWrite(LED_BUILTIN, HIGH);
    if (analogRead(A0) < 10){
      throttle_high = false;
    }
    delay(750);
    digitalWrite(led_sw, LOW);
    digitalWrite(LED_BUILTIN, LOW);
    delay(750);
  }
}

void loop()
{
  int val; 
  val= analogRead(A0); //Read input from analog pin a0 and store in val
  
  val= map(val, 0, 1023,1000,2000); //mapping val to minimum and maximum(Change if needed) 
  esc.writeMicroseconds(val); //using val as the signal to esc
}
