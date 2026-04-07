# Trading Protocol Notes

This document describes the Generation 1 and Generation 2 trading flows as implemented by the current Arduino sketch in [`pk-ball/pk-ball.ino`](pk-ball/pk-ball.ino).

It is not intended to be a full reverse-engineering reference for the original games. It is a practical implementation note for this project: what the sketch listens for, what it sends, how it changes state, and where it decides which Pokemon to save.

## Shared Link Layer

The sketch operates on the Game Boy link cable one byte at a time.

Pins used by the sketch:

- `MOSI_` -> Arduino pin `8`
- `MISO_` -> Arduino pin `9`
- `SCLK_` -> Arduino pin `10`

Each received byte is processed by:

- [`handleIncomingByte()`](pk-ball/pk-ball.ino)

The sketch has two top-level connection phases:

- `NOT_CONNECTED`
- `CONNECTED`

and then moves into:

- `TRADE_CENTRE`
- `COLOSSEUM`

The trade logic in this project is built around the `TRADE_CENTRE` path.

## Shared Connection Handshake

Important constants from [`pk-ball/pokemon.h`](pk-ball/pokemon.h):

- `PKMN_MASTER = 0x01`
- `PKMN_SLAVE = 0x02`
- `PKMN_CONNECTED_I = 0x60`
- `PKMN_CONNECTED_II = 0x61`
- `PKMN_TRADE_CENTRE = 0xD4`
- `GEN_II_CABLE_TRADE_CENTER = 0xD1`
- `TRADE_CENTRE_WAIT = 0xFD`

Shared connection behavior:

1. The Arduino waits for the host.
2. If it sees `PKMN_MASTER`, it responds with `PKMN_SLAVE`.
3. If it sees the generation-specific connected token, it enters `CONNECTED`.
4. If it sees the generation-specific trade-center command, it enters `TRADE_CENTRE`.

## Shared Trade State Machine

The trade center state machine is defined in [`pk-ball/pokemon.h`](pk-ball/pokemon.h):

- `INIT`
- `READY_TO_GO`
- `SEEN_FIRST_WAIT`
- `SENDING_RANDOM_DATA`
- `WAITING_TO_SEND_DATA`
- `SENDING_DATA`
- `SENDING_PATCH_DATA`
- `MIMIC_SELECTION`
- `TRADE_PENDING`
- `TRADE_CONFIRMATION`
- `DONE`

In practice, both generations follow this broad structure:

1. Wait for the trade center to settle.
2. Exchange the main player/trade block.
3. Exchange a post-data phase.
4. Enter selection.
5. Confirm the trade.
6. Save the traded Pokemon.

The details differ between generations.

## Data Blocks

### Generation 1

Main trade block length:

- `GEN1_PLAYER_LENGTH = 418`

Patch length used by the sketch:

- `GEN1_PATCH_LENGTH = 197`

Incoming Gen 1 data is captured into:

- `gen1InputBlock[]`

Outgoing Gen 1 data is served from:

- the Gen 1 EEPROM region via [`readGen1Data()`](pk-ball/pk-ball.ino)

### Generation 2

Main trade block length:

- `GEN2_PLAYER_LENGTH = 444`

Patch length used by the sketch:

- `GEN2_PATCH_LENGTH = 197`

Incoming Gen 2 data is captured into:

- `gen2InputBlock[]`

Outgoing Gen 2 data is served from:

- the Gen 2 EEPROM region via [`readGen2Data()`](pk-ball/pk-ball.ino)

## Generation 1 Protocol Outline

### 1. Entering Trade Flow

Gen 1 uses:

- connected token: `0x60`
- trade center command: `0xD4`

Once inside `TRADE_CENTRE`, the sketch progresses through:

- `INIT`
- `READY_TO_GO`
- `SEEN_FIRST_WAIT`
- `SENDING_RANDOM_DATA`
- `WAITING_TO_SEND_DATA`
- `SENDING_DATA`

### 2. Main Data Exchange

During `SENDING_DATA`:

- the sketch sends bytes from the Gen 1 EEPROM payload
- the incoming bytes are stored into `gen1InputBlock`

This continues until all `418` bytes have been exchanged.

### 3. Patch Phase

After the main block, Gen 1 enters `SENDING_PATCH_DATA`.

In this implementation:

- the sketch mostly mirrors the incoming patch bytes back
- after `197` patch bytes, it transitions to `TRADE_PENDING`

### 4. Slot Selection

Gen 1 slot-selection bytes are:

- slot 1 -> `0x60`
- slot 2 -> `0x61`
- slot 3 -> `0x62`
- slot 4 -> `0x63`
- slot 5 -> `0x64`
- slot 6 -> `0x65`
- cancel -> `0x6F`

In the current sketch:

- valid Gen 1 trade slot bytes are detected by [`isValidTradeSlotByte()`](pk-ball/pk-ball.ino)
- Gen 1 always responds with `0x60` from [`getFirstTradeSlotByte()`](pk-ball/pk-ball.ino)

That means the Arduino always offers the first slot on its own side, even while saving whichever slot the player selected from their party.

### 5. Confirmation

Gen 1 then moves through:

- `TRADE_PENDING`
- `TRADE_CONFIRMATION`

Important current behavior:

- the selected slot is first latched into `tradePokemon`
- during Gen 1 confirmation, the sketch keeps that already-latched slot instead of trusting the later confirmation byte blindly

This was an important bug fix, because the later confirmation byte in Gen 1 can differ from the original selected slot in a way that would otherwise shift saves to the wrong party member.

### 6. Save Decision

Gen 1 save logic lives in [`saveCompletedTrade()`](pk-ball/pk-ball.ino).

It copies:

- the selected 44-byte party record
- the species byte used in the visible party list
- the matching OT name
- the matching nickname

Offsets used by the sketch:

- party data starts at `19`
- each party record is `44` bytes
- OT names start at `283`
- nicknames start at `349`

Save slot formula:

- `start = 19 + (savedTradeSlot * 44)`

## Generation 2 Protocol Outline

### 1. Entering Trade Flow

Gen 2 uses:

- connected token: `0x61`
- trade center command: `0xD1`

The early shared state progression is the same:

- `INIT`
- `READY_TO_GO`
- `SEEN_FIRST_WAIT`
- `SENDING_RANDOM_DATA`
- `WAITING_TO_SEND_DATA`
- `SENDING_DATA`

### 2. Main Data Exchange

During `SENDING_DATA`:

- the sketch sends bytes from the Gen 2 EEPROM payload
- the incoming bytes are stored into `gen2InputBlock`

This continues until all `444` bytes have been exchanged.

### 3. Post-Data / Mimic Phase

After the main block, Gen 2 enters:

- `SENDING_PATCH_DATA`
- then `MIMIC_SELECTION`

`MIMIC_SELECTION` exists because the Gen 2 trade flow has extra chatter after the main data transfer before the real slot-selection bytes appear.

During this phase, the sketch:

- mirrors or normalizes some menu/selection-related traffic
- waits for real trade slot bytes or cancel
- then transitions into `TRADE_PENDING`

### 4. Slot Selection

In the current sketch implementation, valid Gen 2 slot bytes are treated as:

- `0x70` through `0x75`
- cancel -> `0x6F`

The Arduino responds with:

- `0x70`

so the Arduino side stays pinned to its own first slot.

Internally, Gen 2 uses:

- `tradePokemon`
- `confirmedTradePokemon`
- `lastGen2TradeByte`

to track what the player selected and what was later confirmed.

### 5. Confirmation

Gen 2 then moves through:

- `TRADE_PENDING`
- `TRADE_CONFIRMATION`

Unlike Gen 1, Gen 2 now has its own confirmation branch in the sketch so its behavior is isolated from Gen 1.

Current Gen 2 confirmation behavior:

- cancel resets the pending trade state
- a valid confirmed slot byte updates `lastGen2TradeByte`
- `confirmedTradePokemon` is set from that confirmed Gen 2 byte
- `tradePokemon` is synced to `confirmedTradePokemon`
- `pendingTradeSave` is set so the save happens reliably

### 6. Save Decision

Gen 2 save logic also lives in [`saveCompletedTrade()`](pk-ball/pk-ball.ino).

It copies:

- the selected 48-byte party record
- the visible species byte in the outgoing block
- the two party-header bytes at offsets `19` and `20`
- the matching OT name
- the matching nickname

Offsets used by the sketch:

- party data starts at `21`
- each party record is `48` bytes
- OT names start at `309`
- nicknames start at `375`

Save slot formula:

- `start = 21 + (savedTradeSlot * 48)`

## EEPROM And Outgoing Party Data

For both generations, outgoing trade data is always served from EEPROM, not directly from the last live incoming party buffer.

That means:

- after a successful trade, the selected Pokemon is written into EEPROM
- on the next boot, the sketch reads that stored Pokemon back out and offers it in future trades

EEPROM validity is checked per generation using an 8-byte metadata footer:

- magic `PB`
- metadata version
- generation id
- payload length
- CRC-16 of the payload

If a generation fails validation, the sketch reformats EEPROM and reseeds both trade blocks from the built-in default payloads.

## Important Implementation Notes

### Generation 1

The key Gen 1 lesson in this codebase is:

- the selection-phase slot byte is the slot that should be saved
- the later confirmation byte should not be allowed to shift the save slot if a slot is already latched

### Generation 2

The key Gen 2 lesson in this codebase is:

- Gen 2 needs its own confirmation handling
- Gen 2 includes extra post-data traffic before the final trade-selection flow settles
- sharing too much of the confirmation logic with Gen 1 can cause regressions in either direction

## Useful Files

- [`pk-ball/pk-ball.ino`](pk-ball/pk-ball.ino)
- [`pk-ball/pokemon.h`](pk-ball/pokemon.h)
- [`pk-ball/output.h`](pk-ball/output.h)

