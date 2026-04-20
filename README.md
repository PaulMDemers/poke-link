# poke-link

Arduino sketches and libraries for Pokemon link-cable trading and data handling across Generations 1-3.

## Status

This repository currently contains a working Gen 1 / Gen 2 trade sketch in [`pk-ball/pk-ball.ino`](pk-ball/pk-ball.ino).

The current sketch:

- supports Pokemon Generation 1 and Generation 2 trading
- stores one traded Pokemon for each generation in EEPROM
- persists that stored Pokemon across reboot
- validates EEPROM contents with a CRC-backed footer before trusting saved data
- falls back to built-in default data if EEPROM is invalid

## What The Sketch Does

The sketch emulates a trade partner over the Game Boy link cable.

For each supported generation:

- it presents a party to the game
- it allows the player to trade with the Arduino
- it captures the traded Pokemon from the player's party
- it saves that Pokemon into EEPROM
- on the next boot, it reloads the saved Pokemon from EEPROM and offers it back in future trades

The current implementation stores:

- one Gen 1 Pokemon in the Gen 1 trade block
- one Gen 2 Pokemon in the Gen 2 trade block

The rest of each outgoing trade block is seeded from the built-in default data arrays in:

- [`pk-ball/output.h`](pk-ball/output.h)

## Current File Layout

- [`pk-ball/pk-ball.ino`](pk-ball/pk-ball.ino)
  Main Arduino sketch.
- [`pk-ball/pokemon.h`](pk-ball/pokemon.h)
  Shared protocol constants and enums.
- [`pk-ball/output.h`](pk-ball/output.h)
  Built-in default Gen 1 and Gen 2 trade payloads.

## Mode Selection

The sketch currently uses a compile-time mode selection in [`pk-ball/pk-ball.ino`](pk-ball/pk-ball.ino):

```cpp
#define DEFAULT_MODE MODE_GEN_1
```

Set it to one of:

```cpp
#define DEFAULT_MODE MODE_GEN_1
#define DEFAULT_MODE MODE_GEN_2
```

Flash the sketch after changing the mode.

## Hardware Notes

The sketch uses these Arduino pins for the Game Boy link interface:

- `MOSI_` -> pin `8`
- `MISO_` -> pin `9`
- `SCLK_` -> pin `10`

See the sketch constants near the top of [`pk-ball/pk-ball.ino`](pk-ball/pk-ball.ino).

## EEPROM Storage Layout

The sketch stores separate regions for Gen 1 and Gen 2.

Current payload sizes:

- Gen 1 payload: `418` bytes
- Gen 2 payload: `444` bytes

Each region also has an 8-byte metadata footer.

Layout:

- Gen 1 payload: bytes `0-417`
- Gen 1 metadata: bytes `418-425`
- Gen 2 payload: bytes `426-869`
- Gen 2 metadata: bytes `870-877`

## EEPROM Validation And CRC

The current sketch uses an 8-byte footer per generation:

- 2 bytes magic: `PB`
- 1 byte metadata version
- 1 byte generation id
- 2 bytes payload length
- 2 bytes CRC-16 of the payload

At boot:

1. The sketch validates the Gen 1 footer.
2. The sketch validates the Gen 2 footer.
3. If both regions pass, EEPROM data is used.
4. If either region fails, the sketch formats EEPROM and reseeds both regions from the built-in default payloads.

On every successful save:

1. The traded Pokemon data is written into the active generation's payload.
2. A new CRC-16 is calculated for that payload.
3. The footer is rewritten with the updated CRC and metadata.

This makes EEPROM loading much safer against stale or partially corrupted data.

## How Trading Works

### Gen 1

For Gen 1:

- the sketch captures the selected traded Pokemon from the incoming Gen 1 party data
- it stores that Pokemon's 44-byte party structure
- it also stores the matching OT name and nickname data
- future Gen 1 trades present the stored Pokemon from EEPROM

### Gen 2

For Gen 2:

- the sketch captures the selected traded Pokemon from the incoming Gen 2 party data
- it stores that Pokemon's 48-byte party structure
- it also stores the matching OT name and nickname data
- future Gen 2 trades present the stored Pokemon from EEPROM

The Gen 2 flow includes extra state handling because the Gen 2 protocol has more selection and confirmation chatter than Gen 1.

## Default Data

If EEPROM is invalid or freshly formatted, both generations are initialized from the static payload blocks in [`pk-ball/output.h`](pk-ball/output.h).

That means:

- the outgoing trade party is immediately usable on first boot
- later trades replace the single stored Pokemon in EEPROM

## Temporary Debug Output

The sketch currently supports several debug flags near the top of [`pk-ball/pk-ball.ino`](pk-ball/pk-ball.ino).

The most useful one during EEPROM bring-up is:

```cpp
#define DEBUG_EEPROM_STATUS 1
```

This prints:

- region validation results at boot
- whether EEPROM was loaded or reformatted
- save-time CRC updates

Other debug flags exist for detailed Gen 1 and Gen 2 trade tracing, but they are best left off unless debugging protocol behavior.

## Typical Boot Output

When EEPROM is valid, boot output looks roughly like:

```text
eeprom validate gen1 base=0 len=418 gen=1 stored=.... computed=.... result=ok
eeprom validate gen2 base=426 len=444 gen=2 stored=.... computed=.... result=ok
eeprom load source=stored data
mode gen1
```

If EEPROM is invalid, it will instead show a format and reseed sequence.

## How To Use

1. Open [`pk-ball/pk-ball.ino`](pk-ball/pk-ball.ino) in the Arduino IDE.
2. Set `DEFAULT_MODE` to `MODE_GEN_1` or `MODE_GEN_2`.
3. Build and flash the sketch to your Arduino.
4. Connect the Arduino to the Game Boy link interface hardware.
5. Enter the trade center in the target game.
6. Perform a trade.
7. Reboot and trade again to confirm the stored Pokemon persisted.

## Recommended Bring-Up Steps

When testing on fresh hardware:

1. Flash Gen 1 mode and verify a trade persists across reboot.
2. Flash Gen 2 mode and verify a trade persists across reboot.
3. Watch the serial log for EEPROM validation and save messages.

If you want to force a full EEPROM rebuild from defaults, temporarily set:

```cpp
#define FORCE_EEPROM_FORMAT_ON_BOOT 1
```

Boot once, then change it back to `0` and reflash.

## Limitations

- The current sketch is focused on Gen 1 and Gen 2.
- It stores one traded Pokemon per supported generation, not a full captured rotating party.
- Gen 1 and Gen 2 currently share one sketch and a shared state machine. That works, but the generation-specific trade logic is different enough that future cleanup may split more of that logic apart.

## Next Steps

Planned or likely future improvements:

- split generation-specific trade logic more cleanly
- add Gen 3 support and shared protocol helpers
- separate reusable libraries from board-specific sketch code


## References

The original Arduino version of this script started life as a heavily modified version of these scripts:
- [arduino-poke-gen2](https://github.com/stevenchaulk/arduino-poke-gen2)
- [arduino-boy](https://github.com/pepijndevos/arduino-boy)
