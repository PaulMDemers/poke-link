## RP2040 Gen 3 Transport Bring-Up

This directory is the starting point for native Pokemon Gen 3 cable support on the RP2040 Zero.

Goals for this first implementation slice:

- keep Gen 3 separate from the working Gen 1/2 sketch
- reuse only the RP2040-specific platform pieces that are already proven useful
- focus first on 16-bit transport bring-up, session logging, and persistent configuration

User-visible parity target with `pk-ball`:

- act as a trade partner
- start from a seeded default partner state
- capture one traded Pokemon
- persist that traded Pokemon across reboot
- offer it back on later trades

This first sketch does not implement the full trade flow yet.
It provides:

- RP2040 pin setup for the four-signal link board path
- flash-backed EEPROM compatibility storage
- a Gen 3 settings region with CRC-backed metadata
- 16-bit word capture and logging
- PRET-derived Gen 3 command names in the serial trace

Current pin mapping:

- `GPIO 0` -> `PSC`
- `GPIO 1` -> `PSI`
- `GPIO 2` -> `PSO`
- `GPIO 3` -> `PSD`

Current serial helpers:

- `h` prints help
- `d` dumps recent captured words
- `r` resets the transport session state
- `p` cycles the preferred role and saves it

Implementation files:

- [`pk-ball-rp2040-gen3/pk-ball-rp2040-gen3.ino`](/C:/Users/Paul/Desktop/poke-link/pk-ball-rp2040-gen3/pk-ball-rp2040-gen3.ino)
- [`pk-ball-rp2040-gen3/platform_storage.h`](/C:/Users/Paul/Desktop/poke-link/pk-ball-rp2040-gen3/platform_storage.h)
- [`pk-ball-rp2040-gen3/gen3_link.h`](/C:/Users/Paul/Desktop/poke-link/pk-ball-rp2040-gen3/gen3_link.h)
- [`pk-ball-rp2040-gen3/gen3_storage.h`](/C:/Users/Paul/Desktop/poke-link/pk-ball-rp2040-gen3/gen3_storage.h)
