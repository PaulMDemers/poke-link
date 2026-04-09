## RP2040 Gen 1/2 Fresh Port

This directory is the clean restart for the RP2040 Zero port of the original `pk-ball` Gen 1/2 firmware.

It is based directly on:

- [`pk-ball/pk-ball.ino`](/C:/Users/Paul/Desktop/poke-link/pk-ball/pk-ball.ino)
- [`pk-ball/pokemon.h`](/C:/Users/Paul/Desktop/poke-link/pk-ball/pokemon.h)
- [`pk-ball/output.h`](/C:/Users/Paul/Desktop/poke-link/pk-ball/output.h)

The goal here is to keep the original protocol logic intact and only change the platform layer required to run on RP2040.

Current intentional differences from `pk-ball`:

- pin mapping for the RP2040 Zero + Game-Boy-Zero-Link-Board
- storage wrapper in place of direct AVR EEPROM assumptions
- small boot banner at `115200` baud

Pin mapping:

- `GPIO 0` -> `PSC`
- `GPIO 1` -> `PSI`
- `GPIO 2` -> `PSO`
- `GPIO 3` -> `PSD`

Implementation files:

- [`pk-ball-rp2040-fresh/pk-ball-rp2040-fresh.ino`](/C:/Users/Paul/Desktop/poke-link/rp2040-gen12-fresh/pk-ball-rp2040-fresh/pk-ball-rp2040-fresh.ino)
- [`pk-ball-rp2040-fresh/platform_storage.h`](/C:/Users/Paul/Desktop/poke-link/rp2040-gen12-fresh/pk-ball-rp2040-fresh/platform_storage.h)
- [`pk-ball-rp2040-fresh/pokemon.h`](/C:/Users/Paul/Desktop/poke-link/rp2040-gen12-fresh/pk-ball-rp2040-fresh/pokemon.h)
- [`pk-ball-rp2040-fresh/output.h`](/C:/Users/Paul/Desktop/poke-link/rp2040-gen12-fresh/pk-ball-rp2040-fresh/output.h)

This folder is intended to replace the earlier experimental RP2040 attempt as the new baseline for hardware testing.
