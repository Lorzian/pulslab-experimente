#include <Wire.h>
#include <stdio.h>
#include <string.h>

// MAX30100, nu MAX30102: registrele celor doua circuite sunt diferite.
constexpr uint8_t ADDRESS = 0x57;
constexpr uint8_t STATUS = 0x00, FIFO_WR = 0x02, FIFO_DATA = 0x05;
constexpr uint8_t MODE = 0x06, SPO2 = 0x07, LED = 0x09, PART_ID = 0xFF;
enum class State : uint8_t { Retry, Resetting, Ready };
State state = State::Retry;
uint32_t stateSince = 0, lastPoll = 0, lastSample = 0, sequence = 0;
uint8_t ledIr = 7, ledRed = 7;
bool streaming = true;
char command[24];
uint8_t commandLength = 0;
bool commandTooLong = false;

bool writeRegister(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(ADDRESS);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}

bool readRegisters(uint8_t reg, uint8_t* values, uint8_t count) {
  Wire.beginTransmission(ADDRESS);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(ADDRESS, count) != count) {
    while (Wire.available()) Wire.read();
    return false;
  }
  for (uint8_t i = 0; i < count; ++i) values[i] = Wire.read();
  return true;
}

void fail(const __FlashStringHelper* reason) {
  Serial.print(F("#ERR "));
  Serial.println(reason);
  state = State::Retry;
  stateSince = millis();
}

bool clearFifo() {
  return writeRegister(0x02, 0) && writeRegister(0x03, 0) &&
         writeRegister(0x04, 0);
}

void configuration() {
  Serial.print(F("#CFG sensor=MAX30100 rate=100 bits=16 ir="));
  Serial.print(ledIr);
  Serial.print(F(" red="));
  Serial.print(ledRed);
  Serial.print(F(" streaming="));
  Serial.print(streaming ? 1 : 0);
  Serial.print(F(" ready="));
  Serial.println(state == State::Ready ? 1 : 0);
}

// Parser strict, fara String/alocari dinamice; limiteaza si numarul de octeti/loop.
bool parseIndex(const char*& p, uint8_t& value) {
  if (*p < '0' || *p > '9') return false;
  uint16_t result = 0;
  while (*p >= '0' && *p <= '9') {
    result = result * 10 + (*p++ - '0');
    if (result > 15) return false;
  }
  value = result;
  return true;
}

void handleCommand() {
  if (!strcmp(command, "?")) { configuration(); return; }
  if (!strcmp(command, "P") || !strcmp(command, "S")) {
    // Achizitia continua in pauza; FIFO este golit, transmisia D este oprita.
    streaming = command[0] == 'S';
    Serial.println(streaming ? F("#START") : F("#PAUSE"));
    return;
  }
  const char* p = command + 1;
  uint8_t ir, red;
  if (command[0] != 'L' || !parseIndex(p, ir) || *p++ != ',' ||
      !parseIndex(p, red) || *p != '\0') {
    Serial.println(F("#ERR COMMAND expected L0..15,0..15 P S ?"));
    return;
  }
  if (state == State::Ready && !writeRegister(LED, (red << 4) | ir)) {
    fail(F("I2C LED"));
    return;
  }
  ledIr = ir;
  ledRed = red;
  configuration();
}

void receiveCommands() {
  for (uint8_t budget = 0; budget < 32 && Serial.available(); ++budget) {
    char c = Serial.read();
    if (c == '\n') {
      if (commandTooLong) Serial.println(F("#ERR COMMAND too long"));
      else {
        if (commandLength && command[commandLength - 1] == '\r') --commandLength;
        command[commandLength] = '\0';
        if (commandLength) handleCommand();
      }
      commandLength = 0;
      commandTooLong = false;
    } else if (!commandTooLong) {
      if (commandLength < sizeof(command) - 1) command[commandLength++] = c;
      else commandTooLong = true;
    }
  }
}

void initializeSensor() {
  uint32_t now = millis();
  uint8_t value;
  if (state == State::Retry) {
    if (uint32_t(now - stateSince) < 1000) return;
    if (!readRegisters(PART_ID, &value, 1)) { fail(F("I2C PART_ID")); return; }
    if (value != 0x11) { fail(F("PART_ID expected 0x11; check sensor model")); return; }
    if (!writeRegister(MODE, 0x40)) { fail(F("I2C RESET")); return; }
    state = State::Resetting;
    stateSince = now;
    return;
  }
  if (!readRegisters(MODE, &value, 1)) { fail(F("I2C reset status")); return; }
  if (value & 0x40) {
    if (uint32_t(now - stateSince) > 100) fail(F("RESET timeout"));
    return;
  }
  // 0x47 = rezolutie inalta, 100 Hz, impuls LED 1600 us / 16 biti.
  // Modul 0x03 activeaza ambele canale; intreruperile raman dezactivate.
  if (!writeRegister(0x01, 0) || !writeRegister(SPO2, 0x47) ||
      !writeRegister(LED, (ledRed << 4) | ledIr) || !clearFifo() ||
      !readRegisters(STATUS, &value, 1) || !writeRegister(MODE, 0x03)) {
    fail(F("I2C configuration")); return;
  }
  uint8_t modeValue, spo2Value, ledValue;
  if (!readRegisters(MODE, &modeValue, 1) || !readRegisters(SPO2, &spo2Value, 1) ||
      !readRegisters(LED, &ledValue, 1) || modeValue != 0x03 ||
      spo2Value != 0x47 || ledValue != uint8_t((ledRed << 4) | ledIr)) {
    fail(F("configuration readback")); return;
  }
  state = State::Ready;
  lastSample = lastPoll = millis();
  Serial.println(F("#READY MAX30100"));
  configuration();
}

void acquire() {
  uint32_t now = millis();
  if (uint32_t(now - lastPoll) < 5) return;
  lastPoll = now;
  uint8_t pointers[3];
  if (!readRegisters(FIFO_WR, pointers, 3)) { fail(F("I2C FIFO pointers")); return; }
  uint8_t overflow = pointers[1] & 0x0F;
  uint8_t count = (pointers[0] - pointers[2]) & 0x0F;
  if (overflow) {
    // FIFO plin: pointerii egali sunt ambigui. Eliminam lotul corupt temporal.
    // Contorul se satureaza la 15: saltul este o limita inferioara a pierderilor.
    sequence += 16UL + overflow;
    Serial.println(F("#ERR FIFO overflow; dropped >=16 samples plus overflow counter"));
    if (!clearFifo()) fail(F("I2C FIFO clear"));
    return;
  }
  for (uint8_t i = 0; i < count; ++i) {
    uint8_t raw[4];
    if (!readRegisters(FIFO_DATA, raw, 4)) { fail(F("I2C FIFO data")); return; }
    uint16_t ir = (uint16_t(raw[0]) << 8) | raw[1];
    uint16_t red = (uint16_t(raw[2]) << 8) | raw[3];
    lastSample = millis();
    uint32_t number = sequence++;
    if (!streaming) continue;
    char line[32];
    int length = snprintf(line, sizeof(line), "D,%lu,%u,%u\n",
                          (unsigned long)number, (unsigned int)ir, (unsigned int)red);
    // Un esantion consumat dar netransmis lasa un salt vizibil in secventa.
    if (Serial.availableForWrite() >= length) Serial.write((const uint8_t*)line, length);
  }
  if (uint32_t(millis() - lastSample) > 1000) fail(F("no samples for 1s"));
}

void setup() {
  Serial.begin(115200);
  Wire.begin();
  Wire.setClock(100000);
  // Arduino AVR Boards recent: timeout pentru magistrala blocata fizic.
  Wire.setWireTimeout(25000, true);
  Serial.println(F("#BOOT PulsLab firmware 1.0"));
  stateSince = millis() - 1000UL;
}

void loop() {
  receiveCommands();
  if (state != State::Ready) initializeSensor();
  else acquire();
}
