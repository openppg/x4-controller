// Arduino Pins
#define BUTTON_TOP    6   // arm/disarm button_top
#define BUTTON_SIDE   7   // secondary button_top
#define BUZZER_PIN    5   // output for buzzer speaker
#define LED_SW        9   // output for LED on button_top switch
#define LED_2         0   // output for LED 2
#define LED_3         38  // output for LED 3
#define THROTTLE_PIN  A0  // throttle pot input
#define RX_TX_TOGGLE  11  // RS485

#define CTRL_VER 0x00
#define CTRL2HUB_ID 0x10
#define HUB2CTRL_ID 0x20

#define ARM_VERIFY false
#define CURRENT_DIVIDE 100.0
#define VOLTAGE_DIVIDE 1000.0

// Batt setting now configurable by user. Read from device data
#define BATT_MIN_V 49.0  // 7S min (use 42v for 6S)
#define BATT_MAX_V 58.8  // 7S max (use 50v for 6S)

// Calibration
#define MAMP_OFFSET 200

#define VERSION_MAJOR 4
#define VERSION_MINOR 2

#define CRUISE_GRACE 2  // 2 sec period to get off throttle
#define CRUISE_MAX 300  // 5 min max cruising

#define DEFAULT_SEA_PRESSURE 1013.25

// Library config
#define NO_ADAFRUIT_SSD1306_COLOR_COMPATIBILITY

//SP140
#define BUZ_PIN 5
#define BTN_PIN 6
#define POT_PIN A0
#define TFT_RST 9
#define TFT_CS 10
#define TFT_DC 11
#define TFT_LITE A1
#define ESC_PIN 12

#define BLACK                 ST77XX_BLACK
#define WHITE                 ST77XX_WHITE
#define GREEN                 ST77XX_GREEN
#define YELLOW                ST77XX_YELLOW
#define RED                   ST77XX_RED
#define BLUE                  ST77XX_BLUE
#define DIGIT_ARRAY_SIZE      7
#define ESC_BAUD_RATE         115200
#define ESC_DATA_SIZE         20
#define READ_INTERVAL         0
#define ESC_TIMEOUT           10
#define DISPLAY_INTERVAL      100
#define SEALEVELPRESSURE_HPA  1013.25
#define ENABLE_BUZ            true    // enable buzzer
#define ENABLE_VIB            true    // enable vibration
#define ENABLE_VIB_LOW_BAT    true    // vibrate if armed and battery voltage sags below min volts. Gets pilot's attention.
#define MINIMUM_VOLTAGE       60      // 24 * 2.5V per cell
#define MAXIMUM_VOLTAGE       100.8   // 24 * 4.2V per cell
#define ALTITUDE_AGL          1       // [1] Zero altitude when armed, [0] Measure MSL altitude
#define LEFT_HAND_THROTTLE    true    // true for left handed throttle
