// Copyright 2021 <Zach Whitehead>
// OpenPPG

// simple set of data points from load testing
float getBatteryPercent(float voltage) {
  float battPercent = 0;
  if (voltage > 92.4) {
    battPercent = mapf(voltage, 92.4, 100.8, 90, 100);
  } else if (voltage > 90.48) {
    battPercent = mapf(voltage, 90.48, 92.4, 80, 90);
  } else if (voltage > 87.84) {
    battPercent = mapf(voltage, 87.84, 90.48, 70, 80);
  } else if (voltage > 85.44) {
    battPercent = mapf(voltage, 85.44, 87.84, 60, 70);
  } else if (voltage > 82.32) {
    battPercent = mapf(voltage, 82.32, 85.44, 50, 60);
  } else if (voltage > 79.92) {
    battPercent = mapf(voltage, 79.92, 82.32, 40, 50);
  } else if (voltage > 76.32) {
    battPercent = mapf(voltage, 76.32, 79.92, 30, 40);
  } else if (voltage > 67.44) {
    battPercent = mapf(voltage, 67.44, 76.32, 20, 30);
  } else if (voltage > 63.6) {
    battPercent = mapf(voltage, 63.6, 67.44, 10, 20);
  } else if (voltage > 60) {
    battPercent = mapf(voltage, 60, 63.6, 0, 10);
  }
  return constrain(battPercent, 0, 100);
}


// inspired by https://github.com/rlogiacco/BatterySense/
// https://www.desmos.com/calculator/7m9lu26vpy
uint8_t battery_sigmoidal(float voltage, uint16_t minVoltage, uint16_t maxVoltage) {
  uint8_t result = 105 - (105 / (1 + pow(1.724 * (voltage - minVoltage)/(maxVoltage - minVoltage), 5.5)));
  return result >= 100 ? 100 : result;
}
