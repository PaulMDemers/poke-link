#include <EEPROM.h>
#include "pokemon.h"
#include "output.h"

#define MOSI_ 8
#define MISO_ 9
#define SCLK_ 10

#define FORCE_EEPROM_FORMAT_ON_BOOT 0
#define DEBUG_EEPROM_DUMPS 0
#define DEBUG_EEPROM_STATUS 1
#define DEBUG_LINK_TRACE 0
#define DEBUG_GEN1_SELECTION_TRACE 0
#define DEBUG_GEN1_SAVE_BLOCK 0
#define DEBUG_GEN2_SELECTION_TRACE 1
#define DEBUG_GEN2_SAVE_BLOCK 1
#define DEFAULT_MODE MODE_GEN_1
#define REGION_METADATA_LENGTH 8
#define REGION_METADATA_MAGIC_0 'P'
#define REGION_METADATA_MAGIC_1 'B'
#define REGION_METADATA_VERSION 1
#define GEN1_REGION_SIZE (GEN1_PLAYER_LENGTH + REGION_METADATA_LENGTH)
#define GEN2_REGION_SIZE (GEN2_PLAYER_LENGTH + REGION_METADATA_LENGTH)
#define GEN1_EEPROM_BASE 0
#define GEN2_EEPROM_BASE GEN1_REGION_SIZE

#define GEN1_PATCH_LENGTH 197
#define GEN2_PATCH_LENGTH 197

operating_mode_t currentMode = DEFAULT_MODE;
uint8_t shift = 0;
uint8_t in_data = 0;
uint8_t out_data = 0;

connection_state_t connectionState = NOT_CONNECTED;
trade_centre_state_t tradeCentreState = INIT;
int counter = 0;
int tradePokemon = -1;
int confirmedTradePokemon = -1;
bool pendingTradeSave = false;
byte lastGen2TradeByte = 0x00;
unsigned long lastBit = 0;

byte handleIncomingByte(byte in);
void transferBit(void);
void ensureDefaultStorage(void);
void initializeGen1Storage(void);
void initializeGen2Storage(void);
void saveCompletedTrade(void);
bool shouldSaveCompletedTrade(void);
byte readOutgoingData(int index);
byte readGen1Data(int index);
byte readGen2Data(int index);
void updateModeIfIdle(void);
bool hasValidRegionMetadata(int baseOffset, int payloadLength, byte generation);
void writeRegionMetadata(int baseOffset, int payloadLength, byte generation);
void formatEeprom(void);
void dumpStoredData(void);
void dumpRange(const char *label, int start, int length);
bool isValidTradeSlotByte(byte in);
bool isTradeCancelByte(byte in);
void logGen1SelectionByte(byte in, byte out);
void logGen1SaveCandidates(void);
void logGen1SavedBlock(int slot, int start);
void logGen2SelectionByte(byte in, byte out);
void logGen2SaveCandidates(void);
void logGen2SavedBlock(int slot, int start);
bool shouldLogGen1SelectionEvent(byte in);
bool shouldLogGen2SelectionEvent(byte in);
byte getFirstTradeSlotByte(void);
int decodeTradeSlot(byte in);
int getPlayerLength(void);
int getPatchLength(void);
uint16_t computeRegionCrc(int baseOffset, int payloadLength);
void logRegionValidation(const char *label, int baseOffset, int payloadLength, byte generation, bool valid, uint16_t storedCrc, uint16_t computedCrc);
void logRegionWrite(const char *label, int payloadLength, uint16_t crc);

void setup() {
  Serial.begin(115200);

  pinMode(SCLK_, INPUT);
  pinMode(MISO_, INPUT);
  pinMode(MOSI_, OUTPUT);

  currentMode = DEFAULT_MODE;

  ensureDefaultStorage();

#if DEBUG_EEPROM_DUMPS
  dumpStoredData();
#endif

  Serial.print("mode ");
  Serial.print(currentMode == MODE_GEN_1 ? "gen1" : "gen2");
  Serial.print("\n");

  digitalWrite(MOSI_, LOW);
  out_data <<= 1;
}

void loop() {
  lastBit = micros();
  while (digitalRead(SCLK_)) {
  if (micros() - lastBit > 1000000UL) {
#if DEBUG_LINK_TRACE
      Serial.print("idle\n");
#endif
      lastBit = micros();
      shift = 0;
      in_data = 0;
      saveCompletedTrade();
      updateModeIfIdle();
    }
  }
  transferBit();
}

void transferBit(void) {
  in_data |= digitalRead(MISO_) << (7 - shift);

  if (++shift > 7) {
    shift = 0;
    out_data = handleIncomingByte(in_data);
#if DEBUG_LINK_TRACE
    Serial.print(currentMode == MODE_GEN_1 ? "g1 " : "g2 ");
    Serial.print(tradeCentreState);
    Serial.print(" ");
    Serial.print(connectionState);
    Serial.print(" ");
    Serial.print(in_data, HEX);
    Serial.print(" ");
    Serial.print(out_data, HEX);
    Serial.print("\n");
#endif
#if DEBUG_GEN2_SELECTION_TRACE
    if (currentMode == MODE_GEN_2 &&
        (tradeCentreState == SENDING_PATCH_DATA ||
         tradeCentreState == MIMIC_SELECTION ||
         tradeCentreState == TRADE_PENDING ||
         tradeCentreState == TRADE_CONFIRMATION ||
         tradeCentreState == DONE) &&
        shouldLogGen2SelectionEvent(in_data)) {
      logGen2SelectionByte(in_data, out_data);
    }
#endif
#if DEBUG_GEN1_SELECTION_TRACE
    if (currentMode == MODE_GEN_1 &&
        (tradeCentreState == SENDING_PATCH_DATA ||
         tradeCentreState == TRADE_PENDING ||
         tradeCentreState == TRADE_CONFIRMATION ||
         tradeCentreState == DONE) &&
        shouldLogGen1SelectionEvent(in_data)) {
      logGen1SelectionByte(in_data, out_data);
    }
#endif
    in_data = 0;
  }

  while (!digitalRead(SCLK_)) {
  }

  digitalWrite(MOSI_, out_data & 0x80 ? HIGH : LOW);
  out_data <<= 1;
}

byte handleIncomingByte(byte in) {
  byte send = 0x00;
  const byte connectedToken = currentMode == MODE_GEN_1 ? PKMN_CONNECTED_I : PKMN_CONNECTED_II;

  switch (connectionState) {
    case NOT_CONNECTED:
      if (in == PKMN_MASTER) {
        send = PKMN_SLAVE;
      } else if (in == PKMN_BLANK) {
        send = PKMN_BLANK;
      } else if (in == connectedToken) {
        send = connectedToken;
        connectionState = CONNECTED;
      }
      break;

    case CONNECTED:
      if (in == connectedToken) {
        send = connectedToken;
      } else if ((currentMode == MODE_GEN_1 && in == PKMN_TRADE_CENTRE) ||
                 (currentMode == MODE_GEN_2 && in == GEN_II_CABLE_TRADE_CENTER)) {
        connectionState = TRADE_CENTRE;
        send = in;
      } else if ((currentMode == MODE_GEN_1 && in == PKMN_COLOSSEUM) ||
                 (currentMode == MODE_GEN_2 && in == GEN_II_CABLE_CLUB_COLOSSEUM)) {
        connectionState = COLOSSEUM;
        send = in;
      } else if (in == PKMN_BREAK_LINK || in == PKMN_MASTER) {
        connectionState = NOT_CONNECTED;
        tradeCentreState = INIT;
        send = PKMN_BREAK_LINK;
      } else {
        send = in;
      }
      break;

    case TRADE_CENTRE:
      if (currentMode == MODE_GEN_2 &&
          tradeCentreState >= MIMIC_SELECTION &&
          tradeCentreState <= TRADE_PENDING &&
          isValidTradeSlotByte(in)) {
        lastGen2TradeByte = in;
        tradePokemon = decodeTradeSlot(in);
        if (tradeCentreState == MIMIC_SELECTION) {
          tradeCentreState = TRADE_PENDING;
        }
        send = getFirstTradeSlotByte();
        break;
      }

      if (tradeCentreState == INIT && in == 0x00) {
        tradeCentreState = READY_TO_GO;
        send = 0x00;
      } else if (tradeCentreState == READY_TO_GO && in == TRADE_CENTRE_WAIT) {
        tradeCentreState = SEEN_FIRST_WAIT;
        send = TRADE_CENTRE_WAIT;
      } else if (tradeCentreState == SEEN_FIRST_WAIT && in != TRADE_CENTRE_WAIT) {
        send = in;
        tradeCentreState = SENDING_RANDOM_DATA;
      } else if (tradeCentreState == SENDING_RANDOM_DATA && in == TRADE_CENTRE_WAIT) {
        tradeCentreState = WAITING_TO_SEND_DATA;
        send = TRADE_CENTRE_WAIT;
      } else if (tradeCentreState == WAITING_TO_SEND_DATA && in != TRADE_CENTRE_WAIT) {
        counter = 0;
        send = readOutgoingData(counter);
        if (currentMode == MODE_GEN_1) {
          gen1InputBlock[counter] = in;
        } else {
          gen2InputBlock[counter] = in;
        }
        counter++;
        tradeCentreState = SENDING_DATA;
      } else if (tradeCentreState == SENDING_DATA) {
        send = readOutgoingData(counter);
        if (currentMode == MODE_GEN_1) {
          gen1InputBlock[counter] = in;
        } else {
          gen2InputBlock[counter] = in;
        }
        counter++;
        if (counter == getPlayerLength()) {
          tradeCentreState = SENDING_PATCH_DATA;
        }
      } else if (tradeCentreState == SENDING_PATCH_DATA && in == TRADE_CENTRE_WAIT) {
        counter = 0;
        send = TRADE_CENTRE_WAIT;
      } else if (tradeCentreState == SENDING_PATCH_DATA && in != TRADE_CENTRE_WAIT) {
        send = in;
        if (currentMode == MODE_GEN_2) {
          tradeCentreState = MIMIC_SELECTION;
        } else {
          counter++;
          if (counter == getPatchLength()) {
            tradeCentreState = TRADE_PENDING;
          }
        }
      } else if (tradeCentreState == MIMIC_SELECTION) {
        if (currentMode == MODE_GEN_2 &&
            (in == ITEM_1_HIGHLIGHTED || in == ITEM_2_HIGHLIGHTED || in == ITEM_3_HIGHLIGHTED)) {
          send = ITEM_1_HIGHLIGHTED;
        } else if (currentMode == MODE_GEN_2 &&
                   (in == ITEM_1_SELECTED || in == ITEM_2_SELECTED || in == ITEM_3_SELECTED)) {
          send = ITEM_1_SELECTED;
        } else if (isTradeCancelByte(in) || isValidTradeSlotByte(in)) {
          tradeCentreState = TRADE_PENDING;
          if (isTradeCancelByte(in)) {
            tradeCentreState = READY_TO_GO;
            send = 0x6F;
            tradePokemon = -1;
            confirmedTradePokemon = -1;
            pendingTradeSave = false;
            lastGen2TradeByte = 0x00;
          } else {
            lastGen2TradeByte = in;
            send = getFirstTradeSlotByte();
            tradePokemon = decodeTradeSlot(in);
          }
        } else if (in == 0x00) {
          send = 0x00;
        } else {
          send = in;
        }
      } else if (tradeCentreState == TRADE_PENDING && (isTradeCancelByte(in) || isValidTradeSlotByte(in))) {
        if (isTradeCancelByte(in)) {
          tradeCentreState = READY_TO_GO;
          send = 0x6F;
          tradePokemon = -1;
          confirmedTradePokemon = -1;
          pendingTradeSave = false;
          lastGen2TradeByte = 0x00;
        } else {
          lastGen2TradeByte = in;
          send = getFirstTradeSlotByte();
          tradePokemon = decodeTradeSlot(in);
        }
      } else if (tradeCentreState == TRADE_PENDING && in == 0x00) {
        send = 0x00;
        tradeCentreState = TRADE_CONFIRMATION;
      } else if (currentMode == MODE_GEN_1 &&
                 tradeCentreState == TRADE_CONFIRMATION &&
                 (isTradeCancelByte(in) || isValidTradeSlotByte(in))) {
        if (in == 0x61) {
          send = in;
          tradePokemon = -1;
          confirmedTradePokemon = -1;
          pendingTradeSave = false;
          lastGen2TradeByte = 0x00;
          tradeCentreState = TRADE_PENDING;
        } else {
          if (isValidTradeSlotByte(in)) {
            confirmedTradePokemon = tradePokemon >= 0 ? tradePokemon : decodeTradeSlot(in);
            tradePokemon = confirmedTradePokemon;
            pendingTradeSave = true;
          } else {
            tradePokemon = decodeTradeSlot(in);
            confirmedTradePokemon = tradePokemon;
            pendingTradeSave = true;
          }
          send = getFirstTradeSlotByte();
          tradeCentreState = DONE;
        }
      } else if (currentMode == MODE_GEN_2 &&
                 tradeCentreState == TRADE_CONFIRMATION &&
                 (isTradeCancelByte(in) || isValidTradeSlotByte(in))) {
        if (isTradeCancelByte(in)) {
          send = 0x6F;
          tradePokemon = -1;
          confirmedTradePokemon = -1;
          pendingTradeSave = false;
          lastGen2TradeByte = 0x00;
          tradeCentreState = TRADE_PENDING;
        } else {
          lastGen2TradeByte = in;
          confirmedTradePokemon = decodeTradeSlot(in);
          tradePokemon = confirmedTradePokemon;
          pendingTradeSave = true;
          send = getFirstTradeSlotByte();
          tradeCentreState = DONE;
        }
      } else if (tradeCentreState == DONE && isValidTradeSlotByte(in)) {
        send = getFirstTradeSlotByte();
      } else if (tradeCentreState == DONE && in == 0x00) {
        send = 0x00;
        tradeCentreState = INIT;
      } else {
        send = in;
      }
      break;

    default:
      send = in;
      break;
  }

  return send;
}

void updateModeIfIdle(void) {
  return;
}

void ensureDefaultStorage(void) {
  bool gen1Valid = hasValidRegionMetadata(GEN1_EEPROM_BASE, GEN1_PLAYER_LENGTH, 1);
  bool gen2Valid = hasValidRegionMetadata(GEN2_EEPROM_BASE, GEN2_PLAYER_LENGTH, 2);

#if DEBUG_EEPROM_STATUS
  if (FORCE_EEPROM_FORMAT_ON_BOOT) {
    Serial.print("eeprom force format requested\n");
  }
#endif

  if (FORCE_EEPROM_FORMAT_ON_BOOT || !gen1Valid || !gen2Valid) {
    formatEeprom();
#if DEBUG_EEPROM_STATUS
    Serial.print("eeprom load source=static defaults\n");
#endif
  } else {
#if DEBUG_EEPROM_STATUS
    Serial.print("eeprom load source=stored data\n");
#endif
  }
}

void initializeGen1Storage(void) {
  int i;

  for (i = 0; i < GEN1_PLAYER_LENGTH; i++) {
    EEPROM.write(GEN1_EEPROM_BASE + i, pgm_read_byte(&(GEN1_DATA_BLOCK[i])));
  }

  writeRegionMetadata(GEN1_EEPROM_BASE, GEN1_PLAYER_LENGTH, 1);
}

void initializeGen2Storage(void) {
  int i;

  for (i = 0; i < GEN2_PLAYER_LENGTH; i++) {
    EEPROM.write(GEN2_EEPROM_BASE + i, pgm_read_byte(&(GEN2_DATA_BLOCK[i])));
  }

  writeRegionMetadata(GEN2_EEPROM_BASE, GEN2_PLAYER_LENGTH, 2);
}

void saveCompletedTrade(void) {
  int i;
  int start;
  int savedTradeSlot;

  if (!shouldSaveCompletedTrade()) {
    return;
  }

  savedTradeSlot = confirmedTradePokemon >= 0 ? confirmedTradePokemon : tradePokemon;
  if (savedTradeSlot < 0) {
    return;
  }

  if (currentMode == MODE_GEN_2) {
    logGen2SaveCandidates();
  } else {
    logGen1SaveCandidates();
  }

  if (currentMode == MODE_GEN_1) {
    start = 19 + (savedTradeSlot * 44);
#if DEBUG_GEN1_SAVE_BLOCK
    logGen1SavedBlock(savedTradeSlot, start);
#endif
    for (i = 0; i < 44; i++) {
      EEPROM.write(GEN1_EEPROM_BASE + 19 + i, gen1InputBlock[start + i]);
    }
    EEPROM.write(GEN1_EEPROM_BASE + 12, gen1InputBlock[start]);

    start = 283 + (savedTradeSlot * 11);
    for (i = 0; i < 11; i++) {
      EEPROM.write(GEN1_EEPROM_BASE + 283 + i, gen1InputBlock[start + i]);
    }

    start = 349 + (savedTradeSlot * 11);
    for (i = 0; i < 11; i++) {
      EEPROM.write(GEN1_EEPROM_BASE + 349 + i, gen1InputBlock[start + i]);
    }

    writeRegionMetadata(GEN1_EEPROM_BASE, GEN1_PLAYER_LENGTH, 1);
#if DEBUG_EEPROM_STATUS
    logRegionWrite("gen1", GEN1_PLAYER_LENGTH, computeRegionCrc(GEN1_EEPROM_BASE, GEN1_PLAYER_LENGTH));
#endif
  } else {
    start = 21 + (savedTradeSlot * 48);
#if DEBUG_GEN2_SAVE_BLOCK
    logGen2SavedBlock(savedTradeSlot, start);
#endif
    for (i = 0; i < 48; i++) {
      EEPROM.write(GEN2_EEPROM_BASE + 21 + i, gen2InputBlock[start + i]);
    }

    EEPROM.write(GEN2_EEPROM_BASE + 12, gen2InputBlock[start]);
    EEPROM.write(GEN2_EEPROM_BASE + 19, gen2InputBlock[19]);
    EEPROM.write(GEN2_EEPROM_BASE + 20, gen2InputBlock[20]);

    start = 309 + (savedTradeSlot * 11);
    for (i = 0; i < 11; i++) {
      EEPROM.write(GEN2_EEPROM_BASE + 309 + i, gen2InputBlock[start + i]);
    }

    start = 375 + (savedTradeSlot * 11);
    for (i = 0; i < 11; i++) {
      EEPROM.write(GEN2_EEPROM_BASE + 375 + i, gen2InputBlock[start + i]);
    }

    writeRegionMetadata(GEN2_EEPROM_BASE, GEN2_PLAYER_LENGTH, 2);
#if DEBUG_EEPROM_STATUS
    logRegionWrite("gen2", GEN2_PLAYER_LENGTH, computeRegionCrc(GEN2_EEPROM_BASE, GEN2_PLAYER_LENGTH));
#endif
  }

  tradePokemon = -1;
  confirmedTradePokemon = -1;
  pendingTradeSave = false;
  lastGen2TradeByte = 0x00;
  Serial.print("trade saved\n");

#if DEBUG_EEPROM_DUMPS
  dumpStoredData();
#endif
}

bool shouldSaveCompletedTrade(void) {
  if (pendingTradeSave) {
    return true;
  }

  if (tradePokemon < 0) {
    return false;
  }

  if (currentMode == MODE_GEN_2) {
    return tradeCentreState == DONE || tradeCentreState < TRADE_PENDING;
  }

  return tradeCentreState < TRADE_PENDING;
}

byte readOutgoingData(int index) {
  if (currentMode == MODE_GEN_1) {
    return readGen1Data(index);
  }

  return readGen2Data(index);
}

byte readGen1Data(int index) {
  return EEPROM.read(GEN1_EEPROM_BASE + index);
}

byte readGen2Data(int index) {
  return EEPROM.read(GEN2_EEPROM_BASE + index);
}

int getPlayerLength(void) {
  return currentMode == MODE_GEN_1 ? GEN1_PLAYER_LENGTH : GEN2_PLAYER_LENGTH;
}

int getPatchLength(void) {
  return currentMode == MODE_GEN_1 ? GEN1_PATCH_LENGTH : GEN2_PATCH_LENGTH;
}

uint16_t computeRegionCrc(int baseOffset, int payloadLength) {
  int i;
  uint16_t crc = 0xFFFF;

  for (i = 0; i < payloadLength; i++) {
    crc ^= (uint16_t)EEPROM.read(baseOffset + i) << 8;
    for (byte bit = 0; bit < 8; bit++) {
      if (crc & 0x8000) {
        crc = (crc << 1) ^ 0x1021;
      } else {
        crc <<= 1;
      }
    }
  }

  return crc;
}

void logRegionValidation(const char *label, int baseOffset, int payloadLength, byte generation, bool valid, uint16_t storedCrc, uint16_t computedCrc) {
  Serial.print("eeprom validate ");
  Serial.print(label);
  Serial.print(" base=");
  Serial.print(baseOffset);
  Serial.print(" len=");
  Serial.print(payloadLength);
  Serial.print(" gen=");
  Serial.print(generation);
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
}

void logRegionWrite(const char *label, int payloadLength, uint16_t crc) {
  Serial.print("eeprom save ");
  Serial.print(label);
  Serial.print(" len=");
  Serial.print(payloadLength);
  Serial.print(" crc=");
  if (crc < 0x1000) {
    Serial.print("0");
  }
  if (crc < 0x100) {
    Serial.print("0");
  }
  if (crc < 0x10) {
    Serial.print("0");
  }
  Serial.print(crc, HEX);
  Serial.print("\n");
}

bool hasValidRegionMetadata(int baseOffset, int payloadLength, byte generation) {
  int metadataOffset = baseOffset + payloadLength;
  uint16_t storedLength = (uint16_t)EEPROM.read(metadataOffset + 4) |
                          ((uint16_t)EEPROM.read(metadataOffset + 5) << 8);
  uint16_t storedCrc = (uint16_t)EEPROM.read(metadataOffset + 6) |
                       ((uint16_t)EEPROM.read(metadataOffset + 7) << 8);
  uint16_t computedCrc = computeRegionCrc(baseOffset, payloadLength);
  bool valid = EEPROM.read(metadataOffset + 0) == REGION_METADATA_MAGIC_0 &&
               EEPROM.read(metadataOffset + 1) == REGION_METADATA_MAGIC_1 &&
               EEPROM.read(metadataOffset + 2) == REGION_METADATA_VERSION &&
               EEPROM.read(metadataOffset + 3) == generation &&
               storedLength == payloadLength &&
               storedCrc == computedCrc;

#if DEBUG_EEPROM_STATUS
  logRegionValidation(generation == 1 ? "gen1" : "gen2", baseOffset, payloadLength, generation, valid, storedCrc, computedCrc);
#endif

  return valid;
}

void writeRegionMetadata(int baseOffset, int payloadLength, byte generation) {
  int metadataOffset = baseOffset + payloadLength;
  uint16_t crc = computeRegionCrc(baseOffset, payloadLength);

  EEPROM.write(metadataOffset + 0, REGION_METADATA_MAGIC_0);
  EEPROM.write(metadataOffset + 1, REGION_METADATA_MAGIC_1);
  EEPROM.write(metadataOffset + 2, REGION_METADATA_VERSION);
  EEPROM.write(metadataOffset + 3, generation);
  EEPROM.write(metadataOffset + 4, payloadLength & 0xFF);
  EEPROM.write(metadataOffset + 5, (payloadLength >> 8) & 0xFF);
  EEPROM.write(metadataOffset + 6, crc & 0xFF);
  EEPROM.write(metadataOffset + 7, (crc >> 8) & 0xFF);
}

void formatEeprom(void) {
  int i;

#if DEBUG_EEPROM_STATUS
  Serial.print("eeprom format begin\n");
#endif
  for (i = 0; i < EEPROM.length(); i++) {
    EEPROM.write(i, 0x00);
  }

  initializeGen1Storage();
  initializeGen2Storage();
#if DEBUG_EEPROM_STATUS
  Serial.print("eeprom format complete\n");
#endif
}

void dumpStoredData(void) {
  Serial.print("EEPROM DUMP BEGIN\n");
  dumpRange("G1 block", GEN1_EEPROM_BASE, GEN1_PLAYER_LENGTH);
  dumpRange("G1 meta", GEN1_EEPROM_BASE + GEN1_PLAYER_LENGTH, REGION_METADATA_LENGTH);
  dumpRange("G2 block", GEN2_EEPROM_BASE, GEN2_PLAYER_LENGTH);
  dumpRange("G2 meta", GEN2_EEPROM_BASE + GEN2_PLAYER_LENGTH, REGION_METADATA_LENGTH);
  Serial.print("EEPROM DUMP END\n");
}

void dumpRange(const char *label, int start, int length) {
  int i;

  Serial.print(label);
  Serial.print(" @");
  Serial.print(start);
  Serial.print(":");

  for (i = 0; i < length; i++) {
    if ((i % 16) == 0) {
      Serial.print("\n");
    }
    Serial.print(" ");
    if (EEPROM.read(start + i) < 0x10) {
      Serial.print("0");
    }
    Serial.print(EEPROM.read(start + i), HEX);
  }

  Serial.print("\n");
}

bool isValidTradeSlotByte(byte in) {
  if (currentMode == MODE_GEN_2) {
    return in >= 0x70 && in <= 0x75;
  }

  return in >= 0x60 && in <= 0x65;
}

bool isTradeCancelByte(byte in) {
  return in == 0x6F;
}

void logGen1SelectionByte(byte in, byte out) {
  Serial.print("g1sel st=");
  Serial.print(tradeCentreState);
  Serial.print(" in=");
  if (in < 0x10) {
    Serial.print("0");
  }
  Serial.print(in, HEX);
  Serial.print(" out=");
  if (out < 0x10) {
    Serial.print("0");
  }
  Serial.print(out, HEX);
  Serial.print(" tp=");
  Serial.print(tradePokemon);
  Serial.print(" ctp=");
  Serial.print(confirmedTradePokemon);
  Serial.print("\n");
}

void logGen1SaveCandidates(void) {
  int slot;

  Serial.print("g1save confirmed=");
  Serial.print(confirmedTradePokemon);
  Serial.print(" current=");
  Serial.print(tradePokemon);
  Serial.print(" species:");

  for (slot = 0; slot < 6; slot++) {
    byte species = gen1InputBlock[19 + (slot * 44)];
    Serial.print(" ");
    Serial.print(slot);
    Serial.print("=");
    if (species < 0x10) {
      Serial.print("0");
    }
    Serial.print(species, HEX);
  }

  Serial.print("\n");
}

void logGen1SavedBlock(int slot, int start) {
  int i;

  Serial.print("g1block slot=");
  Serial.print(slot);
  Serial.print(" start=");
  Serial.print(start);
  Serial.print(" data:");

  for (i = 0; i < 44; i++) {
    if ((i % 16) == 0) {
      Serial.print("\n");
    }
    Serial.print(" ");
    if (gen1InputBlock[start + i] < 0x10) {
      Serial.print("0");
    }
    Serial.print(gen1InputBlock[start + i], HEX);
  }

  Serial.print("\n");
}

byte getFirstTradeSlotByte(void) {
  return currentMode == MODE_GEN_2 ? 0x70 : 0x60;
}

int decodeTradeSlot(byte in) {
  if (currentMode == MODE_GEN_2) {
    if (in <= 0x70) {
      return 0;
    }
    return in - 0x70;
  }

  return in - 0x60;
}

void logGen2SelectionByte(byte in, byte out) {
  Serial.print("g2sel st=");
  Serial.print(tradeCentreState);
  Serial.print(" in=");
  if (in < 0x10) {
    Serial.print("0");
  }
  Serial.print(in, HEX);
  Serial.print(" out=");
  if (out < 0x10) {
    Serial.print("0");
  }
  Serial.print(out, HEX);
  Serial.print(" tp=");
  Serial.print(tradePokemon);
  Serial.print(" ctp=");
  Serial.print(confirmedTradePokemon);
  Serial.print("\n");
}

void logGen2SaveCandidates(void) {
  int slot;

  Serial.print("g2save confirmed=");
  Serial.print(confirmedTradePokemon);
  Serial.print(" current=");
  Serial.print(tradePokemon);
  Serial.print(" raw=");
  if (lastGen2TradeByte < 0x10) {
    Serial.print("0");
  }
  Serial.print(lastGen2TradeByte, HEX);
  Serial.print(" species:");

  for (slot = 0; slot < 6; slot++) {
    byte species = gen2InputBlock[21 + (slot * 48)];
    Serial.print(" ");
    Serial.print(slot);
    Serial.print("=");
    if (species < 0x10) {
      Serial.print("0");
    }
    Serial.print(species, HEX);
  }

  Serial.print("\n");
}

void logGen2SavedBlock(int slot, int start) {
  int i;

  Serial.print("g2block slot=");
  Serial.print(slot);
  Serial.print(" start=");
  Serial.print(start);
  Serial.print(" data:");

  for (i = 0; i < 48; i++) {
    if ((i % 16) == 0) {
      Serial.print("\n");
    }
    Serial.print(" ");
    if (gen2InputBlock[start + i] < 0x10) {
      Serial.print("0");
    }
    Serial.print(gen2InputBlock[start + i], HEX);
  }

  Serial.print("\n");
}

bool shouldLogGen1SelectionEvent(byte in) {
  if (isValidTradeSlotByte(in) || isTradeCancelByte(in)) {
    return true;
  }

  if (tradeCentreState == SENDING_PATCH_DATA && in == TRADE_CENTRE_WAIT) {
    return true;
  }

  return false;
}

bool shouldLogGen2SelectionEvent(byte in) {
  if (isValidTradeSlotByte(in) || isTradeCancelByte(in)) {
    return true;
  }

  if (tradeCentreState == SENDING_PATCH_DATA && in == TRADE_CENTRE_WAIT) {
    return true;
  }

  return false;
}
