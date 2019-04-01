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

void blinkLED() {
  byte ledState = !digitalRead(LED_2);
  setLEDs(ledState);
}

void runVibe(unsigned int sequence[], int siz) {
  vibe.begin();
  for (int thisNote = 0; thisNote < siz; thisNote++) {
    vibe.setWaveform(thisNote, sequence[thisNote]);
  }
  vibe.go();
}

void playMelody(unsigned int melody[], int siz) {
  for (int thisNote = 0; thisNote < siz; thisNote++) {
    // quarter note = 1000 / 4, eigth note = 1000/8, etc.
    int noteDuration = 125;
    tone(BUZZER_PIN, melody[thisNote], noteDuration);
    delay(noteDuration);  // to distinguish the notes, delay between them
  }
  noTone(BUZZER_PIN);
}
