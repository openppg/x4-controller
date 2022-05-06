// Copyright 2021 <Zach Whitehead>
#ifndef INC_SP140_RP2040_CONFIG_H_
#define INC_SP140_RP2040_CONFIG_H_

#include "shared-config.h"

// Arduino Pins
#define BUTTON_TOP    15   // arm/disarm button_top
#define BUTTON_SIDE   7   // secondary button_top
#define BUZZER_PIN    10   // output for buzzer speaker
#define LED_SW        12// LED_BUILTIN   // output for LED
#define LED_2         -1  // FIXME   // output for LED 2
#define THROTTLE_PIN  A0  // throttle pot input

#define SerialESC  Serial1  // ESC UART connection

#define CRUISE_GRACE 1.5  // 1.5 sec period to get off throttle
#define POT_SAFE_LEVEL 0.10 * 4096  // 10% or less

#define DEFAULT_SEA_PRESSURE 1013.25

// Library config
#define NO_ADAFRUIT_SSD1306_COLOR_COMPATIBILITY

// SP140
#define BUZ_PIN 5
#define POT_PIN A0
#define TFT_RST 5
#define TFT_CS 13
#define TFT_DC 11
#define TFT_LITE 25
#define ESC_PIN 14

#endif  // INC_SP140_RP2040_CONFIG_H_
