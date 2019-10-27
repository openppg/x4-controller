// Copyright 2019 <Zach Whitehead>
// OpenPPG

// ** Logic for EEPROM **

void refreshDeviceData() {
  int offset = 0;

  uint8_t tempBuf[sizeof(STR_DEVICE_DATA)];
  if (0 != eep.read(offset, tempBuf, sizeof(STR_DEVICE_DATA))) {
    //Serial.println(F("error reading EEPROM"));
  }
  memcpy((uint8_t*)&deviceData, tempBuf, sizeof(STR_DEVICE_DATA));
  uint16_t crc = crc16((uint8_t*)&deviceData, sizeof(STR_DEVICE_DATA) - 2);
  if (crc != deviceData.crc) {
    //Serial.print(F("Memory CRC mismatch. Resetting"));
    resetDeviceData();
    return;
  }
}

void resetDeviceData(){
    deviceData = STR_DEVICE_DATA();
    deviceData.version_major = VERSION_MAJOR;
    deviceData.version_minor = VERSION_MINOR;
    deviceData.screen_rotation = 2;
    writeDeviceData();
}

void writeDeviceData() {
  deviceData.crc = crc16((uint8_t*)&deviceData, sizeof(STR_DEVICE_DATA) - 2);
  int offset = 0;

  if (0 != eep.write(offset, (uint8_t*)&deviceData, sizeof(STR_DEVICE_DATA))) {
    Serial.println(F("error writing EEPROM"));
  }
}

// ** Logic for WebUSB **

void line_state_callback(bool connected) {
  digitalWrite(LED_2, connected);

  if ( connected ) send_usb_serial();
}

void parse_usb_serial() {
  const size_t capacity = JSON_OBJECT_SIZE(4);
  DynamicJsonDocument doc(capacity);
  deserializeJson(doc, usb_web);
  int major_v = doc["major_v"];  // 4
  int minor_v = doc["minor_v"];  // 1
  const char* screen_rotation = doc["screen_rot"];  // "l/r"

  deviceData.screen_rotation = (String)screen_rotation == "l" ? 2 : 0;
  initDisplay();
  writeDeviceData();
  send_usb_serial();
}

void send_usb_serial() {
  const size_t capacity = JSON_OBJECT_SIZE(4);
  DynamicJsonDocument doc(capacity);

  doc["major_v"] = VERSION_MAJOR;
  doc["minor_v"] = VERSION_MINOR;
  doc["screen_rot"] = deviceData.screen_rotation == 2 ? "l" : "r";
  doc["armed_time"] = deviceData.armed_time;

  char output[128];
  serializeJson(doc, output);
  usb_web.println(output);
}
