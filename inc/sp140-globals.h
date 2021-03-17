
byte escData[ESC_DATA_SIZE];
byte prevData[ESC_DATA_SIZE];
unsigned long readMillis = 0;
unsigned long transmitted = 0;
unsigned long failed = 0;
bool receiving = false;
bool cruising = false;
bool beginner = false;
unsigned long displayMillis = 0;
int potLvl = 0;
int prevPotLvl = 0;
int cruiseLvl = 0;
float throttlePWM = 0;
float prevThrotPWM = 0;
float throttlePercent = 0;
float prevThrotPercent = 0;
bool buttonState = 1;
bool prevButtonState = 1;
byte numberPowerCycles = 0;
float batteryPercent = 0;
float prevBatteryPercent = 0;
bool batteryFlag = true;
float armingIn = 0;
float prevArmingIn = 0;
float cruisingIn = 0;
float prevCruisingIn = 0;
bool throttledFlag = true;
bool throttled = false;
unsigned long throttledAtMillis = 0;
unsigned int throttleSecs = 0;
float minutes = 0;
float prevMinutes = 0;
float seconds = 0;
float prevSeconds = 0;
float hours = 0;  // logged flight hours

uint16_t _volts = 0;
uint16_t _temperatureC = 0;
int16_t _amps = 0;
uint32_t _eRPM = 0;
uint16_t _inPWM = 0;
uint16_t _outPWM = 0;

//ESC Telemetry
float volts = 0;
float prevVolts = 0;
float temperatureC = 0;
float amps = 0;
float prevAmps = 0;
float kilowatts = 0;
float prevKilowatts = 0;
float eRPM = 0;
float inPWM = 0;
float outPWM = 0;

//ALTIMETER
float ambientTempC = 0;
float altitudeM = 0;
float aglM = 0;
float lastAltM = 0;

Adafruit_BMP3XX bmp;
Servo esc; // Creating a servo class with name of esc
