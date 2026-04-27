# Gen 3 Support Notes

This directory is the staging area for Pokemon Generation 3 support research and planning.

Current conclusion:

- Gen 3 is feasible on the RP2040 track, but it should not be bolted onto the existing Gen 1/2 sketch as a small extension.
- Native Gen 3 cartridge trading uses the GBA-era link stack, not the byte-at-a-time Game Boy protocol the current sketch is built around.
- The safest path is a separate RP2040 Gen 3 implementation with its own transport layer, trade state machine, and storage layout.

Primary target:

- `pk-ball-rp2040-gen12` is the hardware baseline and board reference.
- `pk-ball/pk-ball.ino` is the original behavior reference.
- The Gen 3 goal is to reproduce the same user-visible behavior on RP2040:
  - act as a trade partner
  - expose a usable seeded party on first boot
  - capture the player's traded Pokemon
  - persist that traded Pokemon across reboot
  - offer the stored Pokemon back on future trades

Contents:

- `research-notes.md`
  Protocol and architecture notes gathered from PRET, GBA transport references, and project-specific analysis.
- `trade-timeline.md`
  Concrete Gen 3 cable trade timeline traced from PRET FireRed/Emerald code.
- `hardware-feasibility.md`
  RP2040-first hardware note for native Gen 3 cable support.
- `implementation-plan.md`
  RP2040-first staged plan for implementing `pk-ball`-style Gen 3 behavior.
- `rp2040-task-breakdown.md`
  Concrete implementation tasks and sequencing for the first Gen 3 build.
- `sources.md`
  Source list for the research captured here.

Quick takeaways:

- The current Gen 1/2 RP2040 port still behaves like a direct port of the original three-wire, byte-stream sketch.
- Native GBA multiplayer uses framed 16-bit exchanges, parent/child roles, and block transfers before the trade menu becomes active.
- RP2040 is the better target than AVR for this project slice because it is already the active board path, is 3.3V-native, and gives us better timing headroom.
- FireRed, LeafGreen, Ruby, Sapphire, and Emerald cable trading should be the first target. Wireless support should stay out of v1.

Recommended first engineering milestone:

1. Lock the RP2040 Gen 3 hardware assumptions and transport strategy.
2. Exchange stable 16-bit multiplayer frames with a real Gen 3 game or emulator harness.
3. Implement only enough generic link behavior to survive entry into the trade room.
4. Then build the trade-room command flow and `pk-ball`-style persistence.
