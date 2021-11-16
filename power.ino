// Copyright 2021 <Zach Whitehead>
// OpenPPG

// simple set of data points from load testing
float getBatteryPercent(float voltage) {
  float battPercent = 0;
  
  if (voltage > 92.88) {
    battPercent = mapf(voltage, 92.88, 94.8, 80, 90);
  } else if (voltage > 90.24) {
    battPercent = mapf(voltage, 90.24, 92.88, 70, 80);
  } else if (voltage > 85.44) {
    battPercent = mapf(voltage, 85.44, 90.24, 60, 70);
  } else if (voltage > 84.72) {
    battPercent = mapf(voltage, 84.72, 85.44, 50, 60);
  } else if (voltage > 82.32) {
    battPercent = mapf(voltage, 82.32, 84.72, 40, 50);
  } else if (voltage > 78.72) {
    battPercent = mapf(voltage, 78.72, 82.32, 30, 40);
  } else if (voltage > 69.84) {
    battPercent = mapf(voltage, 69.84, 78.72, 20, 30);
  } else if (voltage > 68.4) {
    battPercent = mapf(voltage, 68.4, 69.84, 10, 20);
  } else if (voltage > 60) {
    battPercent = mapf(voltage, 60, 68.4, 0, 10);
  }
  return constrain(battPercent, 0, 100);
}


// inspired by https://github.com/rlogiacco/BatterySense/
// https://www.desmos.com/calculator/7m9lu26vpy
uint8_t battery_sigmoidal(float voltage, uint16_t minVoltage, uint16_t maxVoltage) {
  uint8_t result = 105 - (105 / (1 + pow(1.724 * (voltage - minVoltage)/(maxVoltage - minVoltage), 5.5)));
  return result >= 100 ? 100 : result;
}
