#ifndef GEN3_LINK_H_
#define GEN3_LINK_H_

#include <stdint.h>

#define GEN3_MAX_LINK_PLAYERS 4
#define GEN3_CMD_LENGTH 8
#define GEN3_BLOCK_BUFFER_SIZE 0x100
#define GEN3_PLAYER_NAME_LENGTH 7
#define GEN3_TRAINER_ID_LENGTH 4
#define GEN3_PARTY_SIZE 6

typedef enum {
  GEN3_ROLE_UNKNOWN = 0,
  GEN3_ROLE_CHILD = 1,
  GEN3_ROLE_PARENT = 2
} gen3_role_t;

typedef enum {
  GEN3_TRANSPORT_IDLE = 0,
  GEN3_TRANSPORT_LISTENING,
  GEN3_TRANSPORT_ACTIVE,
  GEN3_TRANSPORT_ERROR
} gen3_transport_state_t;

typedef enum {
  GEN3_SESSION_DISCONNECTED = 0,
  GEN3_SESSION_CONNECTING,
  GEN3_SESSION_EXCHANGING_PLAYERS,
  GEN3_SESSION_TRADE_ROOM,
  GEN3_SESSION_TRADING,
  GEN3_SESSION_ERROR
} gen3_session_state_t;

typedef struct {
  uint16_t version;
  uint16_t lpField2;
  uint32_t trainerId;
  uint8_t name[GEN3_PLAYER_NAME_LENGTH + 1];
  uint8_t progressFlags;
  uint8_t neverRead;
  uint8_t progressFlagsCopy;
  uint8_t gender;
  uint32_t linkType;
  uint16_t id;
  uint16_t language;
} gen3_link_player_t;

typedef struct {
  char magic1[16];
  gen3_link_player_t linkPlayer;
  char magic2[16];
} gen3_link_player_block_t;

#define GEN3_LINKCMD_SEND_LINK_TYPE       0x2222
#define GEN3_LINKCMD_CONT_BLOCK           0x8888
#define GEN3_LINKCMD_READY_TO_TRADE       0xAABB
#define GEN3_LINKCMD_READY_FINISH_TRADE   0xABCD
#define GEN3_LINKCMD_INIT_BLOCK           0xBBBB
#define GEN3_LINKCMD_READY_CANCEL_TRADE   0xBBCC
#define GEN3_LINKCMD_SEND_BLOCK_REQ       0xCCCC
#define GEN3_LINKCMD_START_TRADE          0xCCDD
#define GEN3_LINKCMD_CONFIRM_FINISH_TRADE 0xDCBA
#define GEN3_LINKCMD_SET_MONS_TO_TRADE    0xDDDD
#define GEN3_LINKCMD_PLAYER_CANCEL_TRADE  0xDDEE
#define GEN3_LINKCMD_REQUEST_CANCEL       0xEEAA
#define GEN3_LINKCMD_PARTNER_CANCEL_TRADE 0xEECC
#define GEN3_LINKCMD_BOTH_CANCEL_TRADE    0xEEEE
#define GEN3_LINKCMD_NONE                 0xEFFF

#define GEN3_LINKTYPE_TRADE              0x1111
#define GEN3_LINKTYPE_TRADE_CONNECTING   0x1122
#define GEN3_LINKTYPE_TRADE_SETUP        0x1133
#define GEN3_LINKTYPE_TRADE_DISCONNECTED 0x1144

typedef enum {
  GEN3_BLOCK_REQ_SIZE_NONE = 0,
  GEN3_BLOCK_REQ_SIZE_200 = 1,
  GEN3_BLOCK_REQ_SIZE_100 = 2,
  GEN3_BLOCK_REQ_SIZE_220 = 3,
  GEN3_BLOCK_REQ_SIZE_40 = 4
} gen3_block_request_t;

static inline const char *getGen3RoleName(gen3_role_t role) {
  switch (role) {
    case GEN3_ROLE_CHILD:
      return "child";
    case GEN3_ROLE_PARENT:
      return "parent";
    default:
      return "unknown";
  }
}

static inline const char *getGen3TransportStateName(gen3_transport_state_t state) {
  switch (state) {
    case GEN3_TRANSPORT_LISTENING:
      return "listening";
    case GEN3_TRANSPORT_ACTIVE:
      return "active";
    case GEN3_TRANSPORT_ERROR:
      return "error";
    default:
      return "idle";
  }
}

static inline const char *getGen3SessionStateName(gen3_session_state_t state) {
  switch (state) {
    case GEN3_SESSION_CONNECTING:
      return "connecting";
    case GEN3_SESSION_EXCHANGING_PLAYERS:
      return "exchange";
    case GEN3_SESSION_TRADE_ROOM:
      return "trade-room";
    case GEN3_SESSION_TRADING:
      return "trading";
    case GEN3_SESSION_ERROR:
      return "error";
    default:
      return "disconnected";
  }
}

static inline const char *getGen3LinkCommandName(uint16_t command) {
  switch (command) {
    case GEN3_LINKCMD_SEND_LINK_TYPE:
      return "SEND_LINK_TYPE";
    case GEN3_LINKCMD_CONT_BLOCK:
      return "CONT_BLOCK";
    case GEN3_LINKCMD_READY_TO_TRADE:
      return "READY_TO_TRADE";
    case GEN3_LINKCMD_READY_FINISH_TRADE:
      return "READY_FINISH_TRADE";
    case GEN3_LINKCMD_INIT_BLOCK:
      return "INIT_BLOCK";
    case GEN3_LINKCMD_READY_CANCEL_TRADE:
      return "READY_CANCEL_TRADE";
    case GEN3_LINKCMD_SEND_BLOCK_REQ:
      return "SEND_BLOCK_REQ";
    case GEN3_LINKCMD_START_TRADE:
      return "START_TRADE";
    case GEN3_LINKCMD_CONFIRM_FINISH_TRADE:
      return "CONFIRM_FINISH_TRADE";
    case GEN3_LINKCMD_SET_MONS_TO_TRADE:
      return "SET_MONS_TO_TRADE";
    case GEN3_LINKCMD_PLAYER_CANCEL_TRADE:
      return "PLAYER_CANCEL_TRADE";
    case GEN3_LINKCMD_REQUEST_CANCEL:
      return "REQUEST_CANCEL";
    case GEN3_LINKCMD_PARTNER_CANCEL_TRADE:
      return "PARTNER_CANCEL_TRADE";
    case GEN3_LINKCMD_BOTH_CANCEL_TRADE:
      return "BOTH_CANCEL_TRADE";
    case GEN3_LINKCMD_NONE:
      return "NONE";
    case GEN3_LINKTYPE_TRADE:
      return "LINKTYPE_TRADE";
    case GEN3_LINKTYPE_TRADE_CONNECTING:
      return "LINKTYPE_TRADE_CONNECTING";
    case GEN3_LINKTYPE_TRADE_SETUP:
      return "LINKTYPE_TRADE_SETUP";
    case GEN3_LINKTYPE_TRADE_DISCONNECTED:
      return "LINKTYPE_TRADE_DISCONNECTED";
    default:
      return 0;
  }
}

static inline bool isGen3NamedWord(uint16_t value) {
  return getGen3LinkCommandName(value) != 0;
}

static inline bool isGen3TradeControlWord(uint16_t value) {
  switch (value) {
    case GEN3_LINKCMD_READY_TO_TRADE:
    case GEN3_LINKCMD_READY_FINISH_TRADE:
    case GEN3_LINKCMD_INIT_BLOCK:
    case GEN3_LINKCMD_READY_CANCEL_TRADE:
    case GEN3_LINKCMD_START_TRADE:
    case GEN3_LINKCMD_CONFIRM_FINISH_TRADE:
    case GEN3_LINKCMD_SET_MONS_TO_TRADE:
    case GEN3_LINKCMD_PLAYER_CANCEL_TRADE:
    case GEN3_LINKCMD_REQUEST_CANCEL:
    case GEN3_LINKCMD_PARTNER_CANCEL_TRADE:
    case GEN3_LINKCMD_BOTH_CANCEL_TRADE:
      return true;
    default:
      return false;
  }
}

#endif
