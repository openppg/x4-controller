// Copyright 2020 <Zach Whitehead>
// OpenPPG

// ** Logic for EEPROM **

void refreshDeviceData() {
  static int offset = 0;

  uint8_t tempBuf[sizeof(deviceData)];
  if (0 != eep.read(offset, tempBuf, sizeof(deviceData))) {
    //Serial.println(F("error reading EEPROM"));
  }
  memcpy((uint8_t*)&deviceData, tempBuf, sizeof(deviceData));
  uint16_t crc = crc16((uint8_t*)&deviceData, sizeof(deviceData) - 2);
    // TODO: add upgrade complete melody
  if (crc != deviceData.crc) {
    //Serial.print(F("Memory CRC mismatch. Resetting"));
    resetDeviceData();
    return;
  }
}

void resetDeviceData() {
    deviceData = STR_DEVICE_DATA_140_V1();
    deviceData.version_major = VERSION_MAJOR;
    deviceData.version_minor = VERSION_MINOR;
    deviceData.screen_rotation = 3;
    deviceData.sea_pressure = DEFAULT_SEA_PRESSURE;  // 1013.25 mbar
    deviceData.metric_temp = true;
    deviceData.metric_alt = true;
    deviceData.performance_mode = 0;
    deviceData.batt_size = 4000;
    writeDeviceData();
}

void writeDeviceData() {
  deviceData.crc = crc16((uint8_t*)&deviceData, sizeof(deviceData) - 2);
  int offset = 0;

  if (0 != eep.write(offset, (uint8_t*)&deviceData, sizeof(deviceData))) {
    Serial.println(F("error writing EEPROM"));
  }
}

// ** Logic for WebUSB **

void line_state_callback(bool connected) {
  digitalWrite(LED_2, connected);

  if ( connected ) send_usb_serial();
}

// customized for sp140
void parse_usb_serial() {
  const size_t capacity = JSON_OBJECT_SIZE(12) + 90;
  DynamicJsonDocument doc(capacity);
  deserializeJson(doc, usb_web);

  if (doc["command"] && doc["command"] == "rbl") {
    display.fillScreen(DEFAULT_BG_COLOR);
    displayMessage("BL - UF2");
    rebootBootloader();
    return;  // run only the command
  }

  if (doc["major_v"] < 5) return;

  deviceData.screen_rotation = doc["screen_rot"];  // "3/1"
  deviceData.sea_pressure = doc["sea_pressure"];  // 1013.25 mbar
  deviceData.metric_temp = doc["metric_temp"];  // true/false
  deviceData.metric_alt = doc["metric_alt"];  // true/false
  deviceData.performance_mode = doc["performance_mode"];  // 0,1
  deviceData.batt_size = doc["batt_size"];  // 4000
  writeDeviceData();
  resetDisplay();
  send_usb_serial();
}

void send_usb_serial() {
  const size_t capacity = JSON_OBJECT_SIZE(11) + 90;
  DynamicJsonDocument doc(capacity);

  doc["major_v"] = VERSION_MAJOR;
  doc["minor_v"] = VERSION_MINOR;
  doc["screen_rot"] = deviceData.screen_rotation;
  doc["armed_time"] = deviceData.armed_time;
  doc["metric_temp"] = deviceData.metric_temp;
  doc["metric_alt"] = deviceData.metric_alt;
  doc["performance_mode"] = deviceData.performance_mode;
  doc["sea_pressure"] = deviceData.sea_pressure;
  doc["device_id"] = chipId();

  char output[256];
  serializeJson(doc, output);
  usb_web.println(output);
}
