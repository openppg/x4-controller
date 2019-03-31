// Map float values
double mapf(double x, double in_min, double in_max, double out_min, double out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

// For digital time display - prints leading 0
void printDigits(byte digits) {
  if (digits < 10) {
    display.print("0");
  }
  // Serial.print(digits, DEC);
  display.print(digits);
}

int nextPage() {
  if (page == 2) {
    return page = 0;
  }
  return ++page;
}

void setLEDs(byte state) {
  digitalWrite(LED_2, state);
  digitalWrite(LED_3, !state);
  digitalWrite(LED_SW, state);
}
