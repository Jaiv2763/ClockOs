#include <Wire.h>
#include <RTClib.h>
#include <TM1637Display.h>
#include <EEPROM.h>
#include <ctype.h>
#include <string.h>

#define CLK 2
#define DIO 3

TM1637Display display(CLK, DIO);
RTC_DS3231 rtc;

// DISPLAY LOGOS

const byte COS2[] = {0x39, 0x3F, 0x6D, 0x5B};
const byte RECO[] = {0x50, 0x79, 0x39, 0x5C};
const byte SAFE[] = {0x6D, 0x77, 0x71, 0x79};
const byte DASH[] = {0x40, 0x40, 0x40, 0x40};
const byte ALL[]  = {0xFF, 0xFF, 0xFF, 0xFF};

// SYSTEM VARIABLES

byte mode = 0;
byte fault = 0;
byte brightness = 5;
byte failures = 0;

bool valid = false;
bool present = false;
bool fallback = false;
bool rtcSeen = false;

uint32_t secondsOfDay = 0;
uint32_t tick = 0;
uint32_t poll = 0;
uint32_t lastRtc = 0;
uint32_t lastAdvance = 0;

char line[48];
byte used = 0;
bool overflow = false;

DateTime current(2000, 1, 1, 0, 0, 0);

// ERROR LOGGING

void saveError(byte e) {
  byte p = EEPROM.read(1);

  if (p >= 16) p = 0;

  EEPROM.update(16 + p, e);
  EEPROM.update(1, (p + 1) % 16);
}

void record(byte e) {
  if (e == fault) return;

  fault = e;

  if (e) {
    saveError(e);

    Serial.print(F("FAULT E00"));
    Serial.println(e);
  } else {
    Serial.println(F("RTC recovered"));
  }
}

// RTC COMMUNICATION

bool readRegs(byte reg, byte *b, byte n) {
  Wire.beginTransmission(0x68);
  Wire.write(reg);

  if (Wire.endTransmission(false))
    return false;

  if (Wire.requestFrom((uint8_t)0x68, n) != n)
    return false;

  for (byte i = 0; i < n; i++) {
    b[i] = Wire.read();
  }

  return true;
}

int bcd(byte x) {
  return (x >> 4) * 10 + (x & 15);
}

bool validBCD(byte x) {
  return (x & 15) <= 9 && (x >> 4) <= 9;
}

// RTC DIAGNOSTICS

byte checkRTC() {
  byte b[7], s;

  Wire.beginTransmission(0x68);
  present = Wire.endTransmission() == 0;

  if (!present) {
    rtcSeen = false;
    return 2;
  }

  if (!readRegs(0, b, 7) ||
      !readRegs(15, &s, 1)) {
    rtcSeen = false;
    return 3;
  }

  if (s & 128) {
    rtcSeen = false;
    return 1;
  }

  if (!validBCD(b[0] & 127) ||
      !validBCD(b[1] & 127) ||
      !validBCD(b[4] & 63) ||
      !validBCD(b[5] & 31) ||
      !validBCD(b[6])) {
    return 4;
  }

  int h;

  if (b[2] & 0x40) {
    byte rawHour = b[2] & 0x1F;

    if (!validBCD(rawHour)) return 4;

    int hour12 = bcd(rawHour);

    if (hour12 < 1 || hour12 > 12)
      return 4;

    h = hour12 % 12;

    if (b[2] & 0x20)
      h += 12;

  } else {
    if (!validBCD(b[2] & 0x3F))
      return 4;

    h = bcd(b[2] & 0x3F);
  }

  int m = bcd(b[1] & 127);
  int sec = bcd(b[0] & 127);
  int day = bcd(b[4] & 63);
  int month = bcd(b[5] & 31);
  int year = 2000 + bcd(b[6]);

  if (h > 23 || m > 59 || sec > 59 ||
      day < 1 || day > 31 ||
      month < 1 || month > 12 ||
      year > 2099)
    return 4;

  DateTime t(year, month, day, h, m, sec);

  if (!t.isValid())
    return 4;

  uint32_t stamp = t.unixtime();

  if (!rtcSeen || stamp != lastRtc) {
    rtcSeen = true;
    lastRtc = stamp;
    lastAdvance = millis();
  } else if (millis() - lastAdvance > 3500) {
    return 6;
  }

  current = t;

  secondsOfDay =
    h * 3600UL + m * 60UL + sec;

  tick = millis();

  valid = true;
  fallback = false;

  return 0;
}

void pollRTC() {
  byte e = checkRTC();

  record(e);

  if (e) {
    fallback = valid;

    if (failures < 3)
      failures++;

    if (failures >= 3 &&
        !valid && mode == 0) {

      mode = 1;

      Serial.println(
        F("Automatic recovery activated")
      );
    }

  } else {
    failures = 0;
  }
}

// DISPLAY SYSTEM

void showError(byte e) {
  byte segments[] = {
    0x79,
    display.encodeDigit(e / 100),
    display.encodeDigit((e / 10) % 10),
    display.encodeDigit(e % 10)
  };

  display.setSegments(segments);
}

void draw() {
  if (mode == 1) {
    display.setSegments(RECO);
    return;
  }

  if (mode == 2) {
    display.setSegments(SAFE);
    return;
  }

  if (mode == 3) {
    display.setSegments(ALL);
    return;
  }

  if (!valid) {
    if (fault)
      showError(fault);
    else
      display.setSegments(DASH);

    return;
  }

  // Briefly show active faults every 10 seconds.

  if (fault && millis() % 10000 < 2000) {
    showError(fault);
    return;
  }

  int h = secondsOfDay / 3600 % 12;

  if (!h) h = 12;

  int m = (secondsOfDay / 60) % 60;

  display.showNumberDecEx(
    h * 100 + m,
    (secondsOfDay % 2) ? 0x40 : 0,
    false
  );
}

// SYSTEM STATUS

void status() {
  Serial.println(F("--------------------"));

  Serial.print(F("RTC: "));

  Serial.println(
    present ? F("Detected") : F("Missing")
  );

  Serial.print(F("Fault: E00"));
  Serial.println(fault);

  Serial.print(F("Time source: "));

  if (!valid)
    Serial.println(F("Unset"));
  else if (fallback)
    Serial.println(F("Software clock"));
  else
    Serial.println(F("DS3231"));

  Serial.print(F("Mode: "));

  switch (mode) {
    case 0:
      Serial.println(F("ClockOS"));
      break;

    case 1:
      Serial.println(F("Recovery"));
      break;

    case 2:
      Serial.println(F("Safe mode"));
      break;

    case 3:
      Serial.println(F("Display test"));
      break;
  }

  Serial.print(F("Brightness: "));
  Serial.println(brightness);

  Serial.print(F("Uptime: "));
  Serial.print(millis() / 1000);
  Serial.println(F(" seconds"));

  Serial.println(F("--------------------"));
}

// HELP MENU

void help() {
  Serial.println(F("===================="));
  Serial.println(F("CLOCKOS 2.0 COMMANDS"));
  Serial.println(F("===================="));

  Serial.println(F("HELP"));
  Serial.println(F("STATUS"));
  Serial.println(F("RECOVERY"));
  Serial.println(F("BOOT"));
  Serial.println(F("SAFE"));
  Serial.println(F("TEST RTC"));
  Serial.println(F("TEST DISPLAY"));
  Serial.println(F("TEST RAM"));
  Serial.println(F("TEST ALL"));
  Serial.println(F("ERRORS"));
  Serial.println(F("LOG"));
  Serial.println(F("RESET LOG"));
  Serial.println(F("SET HH:MM:SS"));
  Serial.println(F("BRIGHT 0-7"));
  Serial.println(F("UPTIME"));
  Serial.println(F("VERSION"));
  Serial.println(F("FACTORY"));
  Serial.println(F("REBOOT"));
}

// RAM DIAGNOSTICS

extern int __heap_start;
extern void *__brkval;

void testRAM() {
  int local;

  int freeRAM =
    (int)&local -
    (int)(__brkval ? __brkval : &__heap_start);

  Serial.print(F("Approx free RAM: "));
  Serial.print(freeRAM);
  Serial.println(F(" bytes"));
}

// MANUAL TIME SETTING

void setTime() {
  bool ok =
    strlen(line) == 12 &&
    line[6] == ':' &&
    line[9] == ':';

  for (byte i = 4; i < 12 && ok; i++) {
    if (i != 6 && i != 9 &&
        !isdigit((unsigned char)line[i])) {
      ok = false;
    }
  }

  if (ok) {
    int h = (line[4] - '0') * 10
            + line[5] - '0';

    int m = (line[7] - '0') * 10
            + line[8] - '0';

    int s = (line[10] - '0') * 10
            + line[11] - '0';

    ok = h < 24 && m < 60 && s < 60;

    if (ok) {
      secondsOfDay =
        h * 3600UL + m * 60UL + s;

      tick = millis();
      valid = true;
      fallback = true;

      Wire.beginTransmission(0x68);
      present = Wire.endTransmission() == 0;

      if (present && rtc.begin()) {
        rtc.adjust(DateTime(
          current.year(),
          current.month(),
          current.day(),
          h, m, s
        ));

        rtcSeen = false;
        lastAdvance = millis();

        pollRTC();
      }

      mode = 0;

      Serial.println(F("Time set successfully"));

      if (fallback)
        Serial.println(F("Software clock active"));

      return;
    }
  }

  saveError(5);

  Serial.println(
    F("E005: Use SET HH:MM:SS")
  );
}

// ERROR HISTORY

void showLog() {
  byte p = EEPROM.read(1);

  if (p >= 16) p = 0;

  bool found = false;

  Serial.println(F("SAVED ERROR HISTORY"));

  for (byte i = 0; i < 16; i++) {
    byte e = EEPROM.read(16 + (p + i) % 16);

    if (e >= 1 && e <= 6) {
      Serial.print(F("E00"));
      Serial.println(e);
      found = true;
    }
  }

  if (!found)
    Serial.println(F("No saved errors"));
}

void clearLog() {
  for (byte i = 16; i < 32; i++)
    EEPROM.update(i, 255);

  EEPROM.update(1, 0);

  Serial.println(F("Error history cleared"));
}

// COMMAND PROCESSOR

void command() {
  for (char *p = line; *p; p++) {
    *p = toupper((unsigned char)*p);
  }

  if (!strcmp(line, "HELP")) {
    help();
  }

  else if (!strcmp(line, "RECOVERY")) {
    mode = 1;

    Serial.println(F("Recovery environment loaded"));
  }

  else if (!strcmp(line, "BOOT")) {
    mode = 0;

    display.setSegments(COS2);
    delay(1000);

    Serial.println(F("ClockOS loaded"));
  }

  else if (!strcmp(line, "SAFE")) {
    mode = 2;

    Serial.println(F("Safe mode loaded"));
  }

  else if (!strcmp(line, "STATUS") ||
           !strcmp(line, "ERRORS")) {
    status();
  }

  else if (!strcmp(line, "TEST RTC")) {
    pollRTC();
    status();
  }

  else if (!strcmp(line, "TEST DISPLAY")) {
    mode = 3;

    Serial.println(F("Display test started"));
    Serial.println(F("Inspect all segments"));
    Serial.println(F("Type BOOT to exit"));
  }

  else if (!strcmp(line, "TEST RAM")) {
    testRAM();
  }

  else if (!strcmp(line, "TEST ALL")) {
    testRAM();
    pollRTC();
    status();

    mode = 3;

    Serial.println(F("Inspect display manually"));
    Serial.println(F("Type BOOT to exit"));
  }

  else if (!strncmp(line, "SET ", 4)) {
    setTime();
  }

  else if (!strncmp(line, "BRIGHT ", 7) &&
           strlen(line) == 8 &&
           line[7] >= '0' &&
           line[7] <= '7') {

    brightness = line[7] - '0';

    display.setBrightness(brightness);
    EEPROM.update(2, brightness);

    Serial.println(F("Brightness updated"));
  }

  else if (!strcmp(line, "LOG")) {
    showLog();
  }

  else if (!strcmp(line, "RESET LOG")) {
    clearLog();
  }

  else if (!strcmp(line, "FACTORY")) {
    Serial.println(F("Type FACTORY CONFIRM"));
  }

  else if (!strcmp(line, "FACTORY CONFIRM")) {
    clearLog();

    brightness = 5;
    EEPROM.update(2, brightness);
    display.setBrightness(brightness);

    Serial.println(F("Factory settings restored"));
  }

  else if (!strcmp(line, "UPTIME")) {
    Serial.print(millis() / 1000);
    Serial.println(F(" seconds"));
  }

  else if (!strcmp(line, "VERSION")) {
    Serial.println(F("ClockOS 2.0 Serial Edition"));
    Serial.println(F("Boot logo: COS2"));
    Serial.println(F("Hardware: Arduino Nano"));
  }

  else if (!strcmp(line, "REBOOT")) {
    Serial.println(F("Press Nano RESET to restart"));
  }

  else {
    Serial.println(F("Unknown command. Type HELP"));
  }

  draw();

  Serial.println(F("ClockOS >"));
}

// STARTUP

void setup() {
  Serial.begin(9600);

  Wire.begin();

#if defined(WIRE_HAS_TIMEOUT)
  Wire.setWireTimeout(25000, true);
#endif

  byte b = EEPROM.read(2);

  if (b <= 7)
    brightness = b;

  display.setBrightness(brightness);

  // COS2 STARTUP LOGO

  display.setSegments(COS2);

  Serial.println(F("===================="));
  Serial.println(F("CLOCKOS 2.0"));
  Serial.println(F("SERIAL EDITION"));
  Serial.println(F("SYSTEM BOOT"));
  Serial.println(F("===================="));

  delay(1500);

  tick = millis();
  lastAdvance = tick;

  pollRTC();

  draw();

  Serial.println(F("Type HELP for commands"));
  Serial.println(F("ClockOS >"));
}

// MAIN LOOP

void loop() {
  uint32_t now = millis();

  if (now - tick >= 1000) {
    uint32_t n = (now - tick) / 1000;

    tick += n * 1000;

    secondsOfDay =
      (secondsOfDay + n) % 86400;
  }

  if (now - poll >= 1000) {
    poll = now;

    pollRTC();
    draw();
  }

  while (Serial.available()) {
    char c = Serial.read();

    if (c == '\r')
      continue;

    if (c == '\n') {
      line[used] = 0;

      if (overflow) {
        Serial.println(F("Command too long"));
      }

      else if (used) {
        command();
      }

      used = 0;
      overflow = false;
    }

    else if (used < sizeof(line) - 1) {
      line[used++] = c;
    }

    else {
      overflow = true;
    }
  }
}