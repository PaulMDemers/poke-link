# Gen 3 Hardware Feasibility For RP2040

This note is specifically about the current project target:

- RP2040 Zero
- the RP2040 Gen 1/2 port in [`pk-ball-rp2040-gen12/pk-ball-rp2040-gen12.ino`](../pk-ball-rp2040-gen12/pk-ball-rp2040-gen12.ino)
- the existing four-pin board wiring called out in [`pk-ball-rp2040-gen12/README.md`](../pk-ball-rp2040-gen12/README.md)

## Short answer

RP2040 is a much better Gen 3 target than the original AVR sketch baseline.

Reasons:

- it is already the active hardware track for this repo
- it is 3.3V-native, which is a better fit for GBA-era link hardware than a 5V Uno/Nano-class board
- it already exposes four GPIOs in the current link-board mapping
- it has enough timing headroom to move Gen 3 transport out of the fragile per-bit polling model
- PIO gives us a realistic path to a tight framed transport if plain GPIO polling is not reliable enough

That does not mean Gen 3 is a small extension of the current RP2040 Gen 1/2 sketch.
It means RP2040 is the right place to build a separate Gen 3 implementation.

## What carries over from the current RP2040 port

Useful carry-over:

- board choice
- serial logging workflow
- flash-backed EEPROM compatibility layer in [`platform_storage.h`](../pk-ball-rp2040-gen12/platform_storage.h)
- the project pattern of static default data plus persisted trade data
- the use of the fourth signal line that did not exist in the original AVR sketch

What does not carry over cleanly:

- the byte-at-a-time `transferBit()` timing model
- the Gen 1/2 state machine shape
- the assumption that one contiguous outgoing block is the whole session
- the idea that mode selection can stay inside one shared protocol loop

## Why RP2040 is a good fit

### 1. Electrical fit is better than AVR

The biggest hardware problem in the older Gen 3 notes was direct AVR-to-GBA electrical compatibility.

RP2040 improves that immediately:

- GPIO is 3.3V-native
- the board is already in use for the link project
- it reduces the need for extra level-shifting decisions that only existed because of the 5V AVR baseline

We still need to confirm the exact link-board direction/protection behavior before real-cartridge testing, but RP2040 removes the biggest obvious mismatch.

### 2. Four-wire wiring is already normal on this board path

The original `pk-ball` sketch uses:

- `MOSI_`
- `MISO_`
- `SCLK_`

The RP2040 port already adds:

- `PSD_`

Gen 3 cable support needs a four-signal design around:

- `SO`
- `SI`
- `SC`
- `SD`

That does not mean the current pin usage is already correct for Gen 3, but it does mean the RP2040 hardware path is already shaped more like the problem we need to solve.

### 3. Timing headroom is much healthier

The current RP2040 Gen 1/2 sketch still polls the link lines in software.
That is acceptable for the existing byte-stream protocol because it is a direct port of the original design.

Gen 3 is different:

- framed 16-bit exchanges
- parent/child role handling
- block send/request traffic
- stricter dependence on stable turnaround timing

RP2040 gives us three realistic implementation options:

1. a transport proof-of-life with tight polling and interrupts
2. a hardware-assisted serial approach if a peripheral maps cleanly
3. a PIO-based transport if we need deterministic line control

That makes RP2040 the right place to prototype transport before we commit to the final shape.

## Real hardware questions we still need to answer

### 1. Exact cable and line behavior

We still need a project-specific answer for:

- which physical cable/adapter path we are targeting for native cartridge tests
- whether the current link board maps cleanly onto Gen 3 cable expectations
- whether any line needs open-drain or explicit direction switching
- whether `SD` needs special handling beyond plain input sampling

### 2. Transport architecture choice

We should decide early whether the first Gen 3 build uses:

- interrupt-driven GPIO handling
- a PIO program
- or a mixed model

Recommendation:

- start with a tiny transport-only experiment
- if plain firmware timing is not clean, move immediately to PIO rather than overfitting the Gen 1/2 loop

### 3. Test harness strategy

Real hardware alone is too slow for early bring-up.

We need at least one of:

- an emulator-assisted link harness
- logic-analyzer traces from a known-good session
- a second controlled endpoint for transport experiments

## Recommendation

For Gen 3 in this repo, the hardware-first order should be:

1. Treat RP2040 as the primary and default Gen 3 platform.
2. Keep Gen 3 in its own RP2040 source directory rather than adding a quick `MODE_GEN_3` branch to the Gen 1/2 sketch.
3. Prove a stable 16-bit multiplayer exchange path first.
4. Only after transport is stable, layer in link-player exchange, block serving, and trade logic.
5. Reuse the RP2040 storage wrapper and CRC-backed persistence pattern once trade data capture works.

## Bottom line

If the question is:

- "Which hardware path should we use for Gen 3 in this repo?"

My answer is:

- RP2040 first
- separate Gen 3 implementation
- transport proof before protocol
- storage and `pk-ball`-style behavior only after the trade room can stay connected
