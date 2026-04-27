#include <EEPROM.h>
#include "platform_storage.h"
#include "gen3_link.h"
#include "gen3_storage.h"

#define PSC_ 0
#define PSI_ 1
#define PSO_ 2
#define PSD_ 3

#define SERIAL_BAUD 115200
#define DEBUG_FRAME_TRACE 1
#define DEBUG_STORAGE_STATUS 1
#define DEBUG_SESSION_TRACE 1

#define GEN3_STORAGE_METADATA_LENGTH 8
#define GEN3_STORAGE_METADATA_MAGIC_0 'P'
#define GEN3_STORAGE_METADATA_MAGIC_1 'B'
#define GEN3_STORAGE_METADATA_VERSION 1
#define GEN3_STORAGE_GENERATION_ID 3
#define GEN3_STORAGE_REGION_SIZE (GEN3_SETTINGS_PAYLOAD_LENGTH + GEN3_STORAGE_METADATA_LENGTH)
#define GEN3_STORAGE_BASE 0

#define TRANSPORT_IDLE_TIMEOUT_US 1000000UL
#define FRAME_LOG_CAPACITY 32

typedef struct {
  uint16_t word;
  uint32_t atMicros;
  uint8_t sdLevel;
  uint8_t sessionState;
} gen3_frame_log_entry_t;

uint8_t shiftCount = 0;
uint16_t incomingWord = 0;
uint16_t outgoingShiftRegister = 0;
uint16_t idleOutgoingWord = 0x0000;

gen3_role_t preferredRole = GEN3_ROLE_UNKNOWN;
gen3_role_t observedRole = GEN3_ROLE_UNKNOWN;
gen3_transport_state_t transportState = GEN3_TRANSPORT_IDLE;
gen3_session_state_t sessionState = GEN3_SESSION_DISCONNECTED;

gen3_frame_log_entry_t frameLog[FRAME_LOG_CAPACITY];
uint8_t frameLogHead = 0;
uint8_t frameLogCount = 0;

uint32_t lastClockActivityMicros = 0;
uint32_t lastWordMicros = 0;
uint32_t wordsSeen = 0;
uint16_t lastIncomingWord = 0x0000;
uint8_t lastObservedSdLevel = 0;
bool sessionDirty = false;

void ensureDefaultStorage(void);
bool hasValidSettingsPayloadHeader(int baseOffset);
bool hasValidRegionMetadata(int baseOffset, int payloadLength, uint8_t generation);
void writeRegionMetadata(int baseOffset, int payloadLength, uint8_t generation);
uint16_t computeRegionCrc(int baseOffset, int payloadLength);
void formatStorage(void);
void loadSettingsFromStorage(void);
void saveSettingsToStorage(void);

void resetTransportSession(void);
void handleIdleTimeout(void);
void transferTransportBit(void);
void handleCompletedWord(uint16_t word, uint8_t sdLevel);
uint16_t getNextOutgoingWord(void);
void rememberWord(uint16_t word, uint8_t sdLevel);

void handleSerialCommands(void);
void printHelp(void);
void dumpRecentWords(void);
void printWordTrace(uint16_t word, uint8_t sdLevel);
void printCurrentConfig(void);
void updateSessionStateFromWord(uint16_t word);
void cyclePreferredRole(void);

void setup() {
  Serial.begin(SERIAL_BAUD);
  {
    unsigned long serialStart = millis();
    while (!Serial && (millis() - serialStart) < 3000UL) {
    }
  }

  storageBegin(GEN3_STORAGE_REGION_SIZE);

  pinMode(PSC_, INPUT);
  pinMode(PSI_, INPUT);
  pinMode(PSO_, OUTPUT);
  pinMode(PSD_, INPUT);

  digitalWrite(PSO_, LOW);
  outgoingShiftRegister = 0x0000;

  ensureDefaultStorage();
  loadSettingsFromStorage();
  resetTransportSession();

  Serial.print("pk-ball-rp2040-gen3 boot\n");
  Serial.print("serial baud=");
  Serial.print(SERIAL_BAUD);
  Serial.print("\n");
  printCurrentConfig();
  printHelp();
}

void loop() {
  handleSerialCommands();

  if (digitalRead(PSC_)) {
    handleIdleTimeout();
    return;
  }

  transferTransportBit();
}

void transferTransportBit(void) {
  uint8_t sampledData = digitalRead(PSI_) ? 1 : 0;
  uint8_t sampledSd = digitalRead(PSD_) ? 1 : 0;

  incomingWord |= (uint16_t)sampledData << (15 - shiftCount);
  lastObservedSdLevel = sampledSd;
  lastClockActivityMicros = micros();

  shiftCount++;
  if (shiftCount == 16) {
    handleCompletedWord(incomingWord, sampledSd);
    incomingWord = 0x0000;
    shiftCount = 0;
    outgoingShiftRegister = getNextOutgoingWord();
  }

  while (!digitalRead(PSC_)) {
  }

  digitalWrite(PSO_, (outgoingShiftRegister & 0x8000) ? HIGH : LOW);
  outgoingShiftRegister <<= 1;
}

void handleCompletedWord(uint16_t word, uint8_t sdLevel) {
  wordsSeen++;
  lastIncomingWord = word;
  lastWordMicros = micros();
  sessionDirty = true;

  if (transportState == GEN3_TRANSPORT_IDLE || transportState == GEN3_TRANSPORT_LISTENING) {
    transportState = GEN3_TRANSPORT_ACTIVE;
  }

  updateSessionStateFromWord(word);
  rememberWord(word, sdLevel);

#if DEBUG_FRAME_TRACE
  printWordTrace(word, sdLevel);
#endif
}

uint16_t getNextOutgoingWord(void) {
  return idleOutgoingWord;
}

void rememberWord(uint16_t word, uint8_t sdLevel) {
  frameLog[frameLogHead].word = word;
  frameLog[frameLogHead].atMicros = micros();
  frameLog[frameLogHead].sdLevel = sdLevel;
  frameLog[frameLogHead].sessionState = (uint8_t)sessionState;

  frameLogHead = (frameLogHead + 1) % FRAME_LOG_CAPACITY;
  if (frameLogCount < FRAME_LOG_CAPACITY) {
    frameLogCount++;
  }
}

void updateSessionStateFromWord(uint16_t word) {
  if (word == GEN3_LINKCMD_SEND_LINK_TYPE ||
      word == GEN3_LINKTYPE_TRADE_CONNECTING ||
      word == GEN3_LINKTYPE_TRADE_SETUP) {
    sessionState = GEN3_SESSION_CONNECTING;
  } else if (word == GEN3_LINKCMD_INIT_BLOCK ||
             word == GEN3_LINKCMD_CONT_BLOCK ||
             word == GEN3_LINKCMD_SEND_BLOCK_REQ) {
    sessionState = GEN3_SESSION_EXCHANGING_PLAYERS;
  } else if (isGen3TradeControlWord(word)) {
    if (word == GEN3_LINKCMD_START_TRADE ||
        word == GEN3_LINKCMD_READY_FINISH_TRADE ||
        word == GEN3_LINKCMD_CONFIRM_FINISH_TRADE) {
      sessionState = GEN3_SESSION_TRADING;
    } else {
      sessionState = GEN3_SESSION_TRADE_ROOM;
    }
  } else if (word == GEN3_LINKTYPE_TRADE_DISCONNECTED) {
    sessionState = GEN3_SESSION_DISCONNECTED;
  }
}

void handleIdleTimeout(void) {
  unsigned long now = micros();

  if ((transportState == GEN3_TRANSPORT_IDLE || transportState == GEN3_TRANSPORT_LISTENING) &&
      !sessionDirty) {
    return;
  }

  if ((unsigned long)(now - lastClockActivityMicros) < TRANSPORT_IDLE_TIMEOUT_US) {
    return;
  }

#if DEBUG_SESSION_TRACE
  Serial.print("transport idle words=");
  Serial.print(wordsSeen);
  Serial.print(" last=");
  if (lastIncomingWord < 0x1000) {
    Serial.print("0");
  }
  if (lastIncomingWord < 0x100) {
    Serial.print("0");
  }
  if (lastIncomingWord < 0x10) {
    Serial.print("0");
  }
  Serial.print(lastIncomingWord, HEX);
  Serial.print(" session=");
  Serial.print(getGen3SessionStateName(sessionState));
  Serial.print("\n");
#endif

  resetTransportSession();
}

void resetTransportSession(void) {
  shiftCount = 0;
  incomingWord = 0x0000;
  outgoingShiftRegister = idleOutgoingWord;
  transportState = GEN3_TRANSPORT_LISTENING;
  sessionState = GEN3_SESSION_DISCONNECTED;
  observedRole = GEN3_ROLE_UNKNOWN;
  lastClockActivityMicros = micros();
  lastWordMicros = 0;
  wordsSeen = 0;
  lastIncomingWord = 0x0000;
  lastObservedSdLevel = 0;
  sessionDirty = false;
}

void handleSerialCommands(void) {
  while (Serial.available() > 0) {
    char command = (char)Serial.read();

    if (command == '\r' || command == '\n') {
      continue;
    }

    switch (command) {
      case 'h':
      case '?':
        printHelp();
        break;
      case 'd':
        dumpRecentWords();
        break;
      case 'r':
        resetTransportSession();
        Serial.print("transport reset\n");
        break;
      case 'p':
        cyclePreferredRole();
        saveSettingsToStorage();
        printCurrentConfig();
        break;
      default:
        Serial.print("unknown command=");
        Serial.print(command);
        Serial.print("\n");
        printHelp();
        break;
    }
  }
}

void printHelp(void) {
  Serial.print("commands: h/help d/dump r/reset p/cycle-role\n");
}

void dumpRecentWords(void) {
  uint8_t count = frameLogCount;
  uint8_t start = (frameLogHead + FRAME_LOG_CAPACITY - count) % FRAME_LOG_CAPACITY;

  Serial.print("recent words=");
  Serial.print(count);
  Serial.print("\n");

  for (uint8_t i = 0; i < count; i++) {
    uint8_t index = (start + i) % FRAME_LOG_CAPACITY;
    const char *name = getGen3LinkCommandName(frameLog[index].word);

    Serial.print("  [");
    Serial.print(i);
    Serial.print("] t=");
    Serial.print(frameLog[index].atMicros);
    Serial.print(" sd=");
    Serial.print(frameLog[index].sdLevel);
    Serial.print(" word=");
    if (frameLog[index].word < 0x1000) {
      Serial.print("0");
    }
    if (frameLog[index].word < 0x100) {
      Serial.print("0");
    }
    if (frameLog[index].word < 0x10) {
      Serial.print("0");
    }
    Serial.print(frameLog[index].word, HEX);
    Serial.print(" session=");
    Serial.print(getGen3SessionStateName((gen3_session_state_t)frameLog[index].sessionState));
    if (name != 0) {
      Serial.print(" name=");
      Serial.print(name);
    }
    Serial.print("\n");
  }
}

void printWordTrace(uint16_t word, uint8_t sdLevel) {
  const char *name = getGen3LinkCommandName(word);

  Serial.print("g3 word=");
  if (word < 0x1000) {
    Serial.print("0");
  }
  if (word < 0x100) {
    Serial.print("0");
  }
  if (word < 0x10) {
    Serial.print("0");
  }
  Serial.print(word, HEX);
  Serial.print(" sd=");
  Serial.print(sdLevel);
  Serial.print(" transport=");
  Serial.print(getGen3TransportStateName(transportState));
  Serial.print(" session=");
  Serial.print(getGen3SessionStateName(sessionState));
  if (name != 0) {
    Serial.print(" name=");
    Serial.print(name);
  }
  Serial.print("\n");
}

void printCurrentConfig(void) {
  Serial.print("config role=");
  Serial.print(getGen3RoleName(preferredRole));
  Serial.print(" idleWord=");
  if (idleOutgoingWord < 0x1000) {
    Serial.print("0");
  }
  if (idleOutgoingWord < 0x100) {
    Serial.print("0");
  }
  if (idleOutgoingWord < 0x10) {
    Serial.print("0");
  }
  Serial.print(idleOutgoingWord, HEX);
  Serial.print("\n");
}

void cyclePreferredRole(void) {
  if (preferredRole == GEN3_ROLE_UNKNOWN) {
    preferredRole = GEN3_ROLE_CHILD;
  } else if (preferredRole == GEN3_ROLE_CHILD) {
    preferredRole = GEN3_ROLE_PARENT;
  } else {
    preferredRole = GEN3_ROLE_UNKNOWN;
  }
}

void ensureDefaultStorage(void) {
  bool regionValid = hasValidRegionMetadata(GEN3_STORAGE_BASE, GEN3_SETTINGS_PAYLOAD_LENGTH, GEN3_STORAGE_GENERATION_ID);

  if (!regionValid) {
    formatStorage();
#if DEBUG_STORAGE_STATUS
    Serial.print("gen3 storage source=defaults\n");
#endif
  } else {
#if DEBUG_STORAGE_STATUS
    Serial.print("gen3 storage source=saved\n");
#endif
  }
}

void loadSettingsFromStorage(void) {
  preferredRole = (gen3_role_t)storageRead(GEN3_STORAGE_BASE + GEN3_SETTINGS_OFFSET_PREFERRED_ROLE);
  if (preferredRole != GEN3_ROLE_UNKNOWN &&
      preferredRole != GEN3_ROLE_CHILD &&
      preferredRole != GEN3_ROLE_PARENT) {
    preferredRole = GEN3_ROLE_UNKNOWN;
  }

  idleOutgoingWord = (uint16_t)storageRead(GEN3_STORAGE_BASE + GEN3_SETTINGS_OFFSET_IDLE_WORD_LO) |
                     ((uint16_t)storageRead(GEN3_STORAGE_BASE + GEN3_SETTINGS_OFFSET_IDLE_WORD_HI) << 8);
}

void saveSettingsToStorage(void) {
  storageWrite(GEN3_STORAGE_BASE + GEN3_SETTINGS_OFFSET_PREFERRED_ROLE, (uint8_t)preferredRole);
  storageWrite(GEN3_STORAGE_BASE + GEN3_SETTINGS_OFFSET_IDLE_WORD_LO, idleOutgoingWord & 0xFF);
  storageWrite(GEN3_STORAGE_BASE + GEN3_SETTINGS_OFFSET_IDLE_WORD_HI, (idleOutgoingWord >> 8) & 0xFF);
  writeRegionMetadata(GEN3_STORAGE_BASE, GEN3_SETTINGS_PAYLOAD_LENGTH, GEN3_STORAGE_GENERATION_ID);
}

void formatStorage(void) {
  for (int i = 0; i < storageLength(); i++) {
    storageWrite(i, 0x00);
  }

  for (int i = 0; i < GEN3_SETTINGS_PAYLOAD_LENGTH; i++) {
    storageWrite(GEN3_STORAGE_BASE + i, readDefaultGen3SettingsByte(i));
  }

  writeRegionMetadata(GEN3_STORAGE_BASE, GEN3_SETTINGS_PAYLOAD_LENGTH, GEN3_STORAGE_GENERATION_ID);
  storageCommit();
}

bool hasValidRegionMetadata(int baseOffset, int payloadLength, uint8_t generation) {
  int metadataOffset = baseOffset + payloadLength;
  uint16_t storedLength = (uint16_t)storageRead(metadataOffset + 4) |
                          ((uint16_t)storageRead(metadataOffset + 5) << 8);
  uint16_t storedCrc = (uint16_t)storageRead(metadataOffset + 6) |
                       ((uint16_t)storageRead(metadataOffset + 7) << 8);
  uint16_t computedCrc = computeRegionCrc(baseOffset, payloadLength);
  bool payloadHeaderValid = hasValidSettingsPayloadHeader(baseOffset);
  bool valid = storageRead(metadataOffset + 0) == GEN3_STORAGE_METADATA_MAGIC_0 &&
               storageRead(metadataOffset + 1) == GEN3_STORAGE_METADATA_MAGIC_1 &&
               storageRead(metadataOffset + 2) == GEN3_STORAGE_METADATA_VERSION &&
               storageRead(metadataOffset + 3) == generation &&
               payloadHeaderValid &&
               storedLength == payloadLength &&
               storedCrc == computedCrc;

#if DEBUG_STORAGE_STATUS
  Serial.print("gen3 storage validate len=");
  Serial.print(payloadLength);
  Serial.print(" stored=");
  if (storedCrc < 0x1000) {
    Serial.print("0");
  }
  if (storedCrc < 0x100) {
    Serial.print("0");
  }
  if (storedCrc < 0x10) {
    Serial.print("0");
  }
  Serial.print(storedCrc, HEX);
  Serial.print(" computed=");
  if (computedCrc < 0x1000) {
    Serial.print("0");
  }
  if (computedCrc < 0x100) {
    Serial.print("0");
  }
  if (computedCrc < 0x10) {
    Serial.print("0");
  }
  Serial.print(computedCrc, HEX);
  Serial.print(" result=");
  Serial.print(valid ? "ok" : "bad");
  Serial.print("\n");
#endif

  return valid;
}

bool hasValidSettingsPayloadHeader(int baseOffset) {
  return storageRead(baseOffset + GEN3_SETTINGS_OFFSET_MAGIC_0) == 'G' &&
         storageRead(baseOffset + GEN3_SETTINGS_OFFSET_MAGIC_1) == '3' &&
         storageRead(baseOffset + GEN3_SETTINGS_OFFSET_MAGIC_2) == 'C' &&
         storageRead(baseOffset + GEN3_SETTINGS_OFFSET_MAGIC_3) == 'F' &&
         storageRead(baseOffset + GEN3_SETTINGS_OFFSET_VERSION) == 1;
}

void writeRegionMetadata(int baseOffset, int payloadLength, uint8_t generation) {
  int metadataOffset = baseOffset + payloadLength;
  uint16_t crc = computeRegionCrc(baseOffset, payloadLength);

  storageWrite(metadataOffset + 0, GEN3_STORAGE_METADATA_MAGIC_0);
  storageWrite(metadataOffset + 1, GEN3_STORAGE_METADATA_MAGIC_1);
  storageWrite(metadataOffset + 2, GEN3_STORAGE_METADATA_VERSION);
  storageWrite(metadataOffset + 3, generation);
  storageWrite(metadataOffset + 4, payloadLength & 0xFF);
  storageWrite(metadataOffset + 5, (payloadLength >> 8) & 0xFF);
  storageWrite(metadataOffset + 6, crc & 0xFF);
  storageWrite(metadataOffset + 7, (crc >> 8) & 0xFF);
  storageCommit();
}

uint16_t computeRegionCrc(int baseOffset, int payloadLength) {
  uint16_t crc = 0xFFFF;

  for (int i = 0; i < payloadLength; i++) {
    crc ^= (uint16_t)storageRead(baseOffset + i) << 8;
    for (uint8_t bit = 0; bit < 8; bit++) {
      if (crc & 0x8000) {
        crc = (crc << 1) ^ 0x1021;
      } else {
        crc <<= 1;
      }
    }
  }

  return crc;
}
