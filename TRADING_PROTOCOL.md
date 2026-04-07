# Trading Protocol Reference

This document describes the Generation 1 and Generation 2 trade protocol as implemented by the current Arduino sketch in [`pk-ball/pk-ball.ino`](pk-ball/pk-ball.ino).

It is intentionally implementation-focused:

- which bytes the sketch reacts to
- which state transitions matter
- where party data lives in the incoming buffers
- how the selected traded Pokemon is chosen and saved

It is not a complete reverse-engineering spec for the original games. It is a practical reference for maintaining this codebase.

## Scope

Current supported generations:

- Generation 1
- Generation 2

Current sketch behavior:

- emulates a trade partner over the Game Boy link cable
- exposes one stored Pokemon per generation
- captures the player's selected traded Pokemon
- persists that Pokemon to EEPROM
- restores the stored Pokemon on future boots

## Shared Transport

### Pins

The sketch uses:

| Signal | Arduino pin |
| --- | --- |
| `MOSI_` | `8` |
| `MISO_` | `9` |
| `SCLK_` | `10` |

### Byte Processing Model

The link is processed a bit at a time, but protocol decisions happen one byte at a time inside:

- [`handleIncomingByte()`](pk-ball/pk-ball.ino)

For each byte:

1. The sketch receives one byte from the peer.
2. It updates connection / trade state.
3. It chooses one reply byte.
4. It shifts that reply back out over the link.

### Connection States

Defined in [`pk-ball/pokemon.h`](pk-ball/pokemon.h):

| State | Meaning |
| --- | --- |
| `NOT_CONNECTED` | Waiting for initial handshake |
| `CONNECTED` | Link established, waiting for mode selection |
| `TRADE_CENTRE` | In trade-center flow |
| `COLOSSEUM` | In colosseum flow |

### Common Handshake Constants

| Name | Value | Meaning |
| --- | --- | --- |
| `PKMN_MASTER` | `0x01` | Host/master token |
| `PKMN_SLAVE` | `0x02` | Arduino/slave response |
| `PKMN_BLANK` | `0x00` | Idle / blank |
| `TRADE_CENTRE_WAIT` | `0xFD` | Trade wait marker |
| `PKMN_BREAK_LINK` | `0xD6` | Disconnect / break |

### Generation-Specific Entry Bytes

| Generation | Connected token | Trade center command | Colosseum command |
| --- | --- | --- | --- |
| Gen 1 | `0x60` | `0xD4` | `0xD5` |
| Gen 2 | `0x61` | `0xD1` | `0xD2` |

### Shared Trade State Machine

Defined in [`pk-ball/pokemon.h`](pk-ball/pokemon.h):

| State | Purpose |
| --- | --- |
| `INIT` | Waiting for trade-center start |
| `READY_TO_GO` | Link settled, waiting for first wait byte |
| `SEEN_FIRST_WAIT` | First wait seen |
| `SENDING_RANDOM_DATA` | Transitional pre-data chatter |
| `WAITING_TO_SEND_DATA` | Waiting for the main block exchange to begin |
| `SENDING_DATA` | Main player/trade block exchange |
| `SENDING_PATCH_DATA` | Post-data patch / trailing exchange |
| `MIMIC_SELECTION` | Gen 2-only post-data menu chatter |
| `TRADE_PENDING` | Selection phase |
| `TRADE_CONFIRMATION` | Confirmation phase |
| `DONE` | Trade finished, waiting to settle back to idle |

## Generation 1 Reference

### Core Constants

| Item | Value |
| --- | --- |
| Connected token | `0x60` |
| Trade center command | `0xD4` |
| Main block length | `418` bytes |
| Patch length | `197` bytes |
| Input buffer | `gen1InputBlock[]` |

### High-Level Flow

Gen 1 trade flow in this sketch:

1. Enter `TRADE_CENTRE`
2. Exchange the 418-byte main trade block
3. Exchange 197 bytes of patch / trailing data
4. Enter slot selection
5. Confirm the trade
6. Save the selected Pokemon to EEPROM

### State Progression

Typical Gen 1 path:

```text
INIT
READY_TO_GO
SEEN_FIRST_WAIT
SENDING_RANDOM_DATA
WAITING_TO_SEND_DATA
SENDING_DATA
SENDING_PATCH_DATA
TRADE_PENDING
TRADE_CONFIRMATION
DONE
INIT
```

### Main Data Exchange

During `SENDING_DATA`:

- outgoing bytes come from [`readGen1Data()`](pk-ball/pk-ball.ino)
- incoming bytes are copied into `gen1InputBlock`

The full Gen 1 trade block is `418` bytes long.

### Gen 1 Slot Bytes

The current sketch treats these as valid Gen 1 trade-slot bytes:

| Byte | Slot index | Human slot |
| --- | --- | --- |
| `0x60` | `0` | Slot 1 |
| `0x61` | `1` | Slot 2 |
| `0x62` | `2` | Slot 3 |
| `0x63` | `3` | Slot 4 |
| `0x64` | `4` | Slot 5 |
| `0x65` | `5` | Slot 6 |
| `0x6F` | cancel | Back out |

The Arduino always answers with:

| Meaning | Byte |
| --- | --- |
| Arduino-selected slot | `0x60` |

So the Arduino side stays fixed on its first slot.

### Gen 1 Save Rule

Important behavior:

- the selected slot is first latched during the selection phase
- the later confirmation byte is not trusted to override it if a slot is already latched

This prevents the save from drifting to the wrong slot during confirmation.

### Gen 1 Data Layout

Within `gen1InputBlock`:

| Section | Start | Size |
| --- | --- | --- |
| Party data | `19` | `44 * 6` |
| OT names | `283` | `11 * 6` |
| Nicknames | `349` | `11 * 6` |

Per-Pokemon record size:

| Field | Size |
| --- | --- |
| Party Pokemon record | `44` bytes |
| OT name | `11` bytes |
| Nickname | `11` bytes |

Save offset formulas:

| Item | Formula |
| --- | --- |
| Party record | `19 + (slot * 44)` |
| OT name | `283 + (slot * 11)` |
| Nickname | `349 + (slot * 11)` |

### Gen 1 EEPROM Write Behavior

When a Gen 1 trade is saved, the sketch writes:

- the selected 44-byte party record into the outgoing first-slot location
- the selected species byte into the visible party list
- the selected OT name
- the selected nickname

This is what makes the traded Pokemon appear as the Arduino's offered Pokemon on the next boot.

## Generation 2 Reference

### Core Constants

| Item | Value |
| --- | --- |
| Connected token | `0x61` |
| Trade center command | `0xD1` |
| Main block length | `444` bytes |
| Patch length | `197` bytes |
| Input buffer | `gen2InputBlock[]` |

### High-Level Flow

Gen 2 trade flow in this sketch:

1. Enter `TRADE_CENTRE`
2. Exchange the 444-byte main trade block
3. Process post-data traffic
4. Enter a Gen 2-specific selection handling phase
5. Confirm the trade
6. Save the selected Pokemon to EEPROM

### State Progression

Typical Gen 2 path:

```text
INIT
READY_TO_GO
SEEN_FIRST_WAIT
SENDING_RANDOM_DATA
WAITING_TO_SEND_DATA
SENDING_DATA
SENDING_PATCH_DATA
MIMIC_SELECTION
TRADE_PENDING
TRADE_CONFIRMATION
DONE
INIT
```

### Main Data Exchange

During `SENDING_DATA`:

- outgoing bytes come from [`readGen2Data()`](pk-ball/pk-ball.ino)
- incoming bytes are copied into `gen2InputBlock`

The full Gen 2 trade block is `444` bytes long.

### Why `MIMIC_SELECTION` Exists

Gen 2 has more post-data menu / selection chatter than Gen 1.

The sketch uses `MIMIC_SELECTION` as a buffer state to:

- absorb that extra traffic
- normalize some menu highlight / menu select bytes
- wait until real trade-selection bytes appear

This exists because treating the first post-data bytes as final slot decisions caused repeated slot-selection bugs.

### Gen 2 Selection-Related Bytes

Menu-related bytes seen by the sketch:

| Name | Value |
| --- | --- |
| `ITEM_1_HIGHLIGHTED` | `0xD0` |
| `ITEM_2_HIGHLIGHTED` | `0xD1` |
| `ITEM_3_HIGHLIGHTED` | `0xD2` |
| `ITEM_1_SELECTED` | `0xD4` |
| `ITEM_2_SELECTED` | `0xD5` |
| `ITEM_3_SELECTED` | `0xD6` |

Trade-slot bytes used by the current sketch:

| Byte | Slot index | Human slot |
| --- | --- | --- |
| `0x70` | `0` | Slot 1 |
| `0x71` | `1` | Slot 2 |
| `0x72` | `2` | Slot 3 |
| `0x73` | `3` | Slot 4 |
| `0x74` | `4` | Slot 5 |
| `0x75` | `5` | Slot 6 |
| `0x6F` | cancel | Back out |

The Arduino always answers with:

| Meaning | Byte |
| --- | --- |
| Arduino-selected slot | `0x70` |

So the Arduino side stays pinned to its own first slot.

### Gen 2 Confirmation Rule

Gen 2 now has its own confirmation branch in the sketch.

That is important because Gen 1 and Gen 2 do not behave the same way during confirmation.

Current Gen 2 confirmation behavior:

- valid confirmation bytes update `lastGen2TradeByte`
- the confirmed slot is decoded from the Gen 2 confirmation byte
- `confirmedTradePokemon` is set from that decoded slot
- `pendingTradeSave` is set so the save occurs reliably

This logic is intentionally separated from the Gen 1 confirmation path to prevent cross-regressions.

### Gen 2 Data Layout

Within `gen2InputBlock`:

| Section | Start | Size |
| --- | --- | --- |
| Party data | `21` | `48 * 6` |
| OT names | `309` | `11 * 6` |
| Nicknames | `375` | `11 * 6` |

Additional bytes copied for the visible party header:

| Offset | Meaning |
| --- | --- |
| `12` | visible first-slot species |
| `19` | Gen 2 header byte |
| `20` | Gen 2 header byte |

Per-Pokemon record size:

| Field | Size |
| --- | --- |
| Party Pokemon record | `48` bytes |
| OT name | `11` bytes |
| Nickname | `11` bytes |

Save offset formulas:

| Item | Formula |
| --- | --- |
| Party record | `21 + (slot * 48)` |
| OT name | `309 + (slot * 11)` |
| Nickname | `375 + (slot * 11)` |

### Gen 2 EEPROM Write Behavior

When a Gen 2 trade is saved, the sketch writes:

- the selected 48-byte party record into the outgoing first-slot location
- the selected species byte into the visible party list
- the two Gen 2 header bytes at offsets `19` and `20`
- the selected OT name
- the selected nickname

## EEPROM Backing

For both generations, outgoing trade data is always read from EEPROM, not from the last live incoming buffer.

That means:

- the sketch captures a traded Pokemon from live traffic
- it writes that Pokemon into the generation's EEPROM payload
- future trades are served from EEPROM

### EEPROM Region Layout

| Region | Payload range | Payload length | Footer range |
| --- | --- | --- | --- |
| Gen 1 | `0-417` | `418` bytes | `418-425` |
| Gen 2 | `426-869` | `444` bytes | `870-877` |

### EEPROM Footer Format

Each generation ends with an 8-byte footer:

| Offset in footer | Size | Meaning |
| --- | --- | --- |
| `0` | 1 | magic `P` |
| `1` | 1 | magic `B` |
| `2` | 1 | footer version |
| `3` | 1 | generation id |
| `4` | 1 | payload length low byte |
| `5` | 1 | payload length high byte |
| `6` | 1 | CRC-16 low byte |
| `7` | 1 | CRC-16 high byte |

### EEPROM Validation Rule

At boot:

1. Gen 1 footer is checked.
2. Gen 2 footer is checked.
3. Magic, version, generation, payload length, and CRC must all match.
4. If either generation fails validation, EEPROM is reformatted and both regions are reseeded from the built-in defaults in [`pk-ball/output.h`](pk-ball/output.h).

## Maintenance Notes

### Known Architectural Boundary

The sketch still uses one shared state machine for both generations, but:

- Gen 1 and Gen 2 share only the broad transport flow
- selection and confirmation handling are generation-specific
- save slot interpretation is generation-specific

When changing trade behavior:

- avoid re-sharing confirmation logic unless both generations are verified
- test slot 1 through slot 6 for both generations
- power-cycle between tests when debugging EEPROM behavior

### Key Files

- [`pk-ball/pk-ball.ino`](pk-ball/pk-ball.ino)
- [`pk-ball/pokemon.h`](pk-ball/pokemon.h)
- [`pk-ball/output.h`](pk-ball/output.h)

