# Gen 3 Implementation Plan

## Goal

Add native Pokemon Generation 3 cable trade support to this repo on the RP2040 track without destabilizing the working Gen 1/2 firmware.

## User-visible parity target

Gen 3 should match the current `pk-ball` behavior model, not the internal implementation shape.

For Gen 3 v1 that means:

- the device acts as a trade partner
- it offers a seeded partner party on first boot
- it supports one real cartridge trade at a time
- it captures the Pokemon the player traded away
- it persists that Pokemon across reboot
- it offers that stored Pokemon back on later trades

This is the behavior reference taken from:

- [`pk-ball/pk-ball.ino`](../pk-ball/pk-ball.ino)
- [`pk-ball-rp2040-gen12/pk-ball-rp2040-gen12.ino`](../pk-ball-rp2040-gen12/pk-ball-rp2040-gen12.ino)

## Proposed structure

Create Gen 3 as a separate RP2040 implementation track first:

- a new source directory such as `pk-ball-rp2040-gen3`
- a dedicated transport layer for GBA multiplayer
- a dedicated Gen 3 trade/session state machine
- a dedicated Gen 3 storage region and format version
- shared helpers only after Gen 3 transport is proven stable

## Architecture decisions

### 1. Keep Gen 3 separate from the Gen 1/2 loop

Do not add Gen 3 as a quick extra `MODE_*` branch inside the current sketch.

Reason:

- Gen 1/2 is built around streamed bytes and one trade-centre state machine
- Gen 3 needs framed multiplayer transport, generic link setup, block exchange, and then trade-room logic

### 2. Treat RP2040 as the primary hardware target

Do not spend the first milestone re-validating AVR viability.
That is no longer the project's main path.

Reason:

- RP2040 is already the active board port
- it is 3.3V-native
- it gives better timing options for Gen 3 transport

### 3. Separate transport, protocol, and storage milestones

Implementation should be layered:

1. physical transport
2. generic GBA link/session behavior
3. trade-room commands and block handling
4. Pokemon data integration
5. persistence

This order keeps bring-up failures local and debuggable.

## Milestones

### 0. RP2040 baseline sanity pass

Purpose:

- make sure the current RP2040 Gen 1/2 branch is still a trustworthy behavior reference before cloning patterns from it

Deliverables:

- document the current RP2040 divergences from `pk-ball`
- decide whether any Gen 1/2 behavior bugs need to be fixed before the Gen 3 branch is created

Exit criteria:

- we know exactly which parts of `pk-ball-rp2040-gen12` are board-port changes versus protocol changes

### 1. Gen 3 hardware and transport design lock

Deliverables:

- exact RP2040 pin-to-link-signal mapping for Gen 3
- chosen transport strategy: IRQ, PIO, or mixed
- agreed test harness path: real cartridge, emulator harness, or both
- logging format for 16-bit frame traces

Exit criteria:

- there is one clear transport implementation approach
- there is one clear hardware test setup

### 2. Transport proof-of-life

Deliverables:

- a minimal RP2040 test program that can enter GBA multiplayer behavior
- stable 16-bit frame exchange with a known-good partner
- parent/child role detection or explicit forced-role configuration
- disconnect and timeout recovery logs

Exit criteria:

- reproducible frame traces at the chosen baud rate
- stable reconnect after cable idle or link loss

### 3. Generic link layer

Deliverables:

- link-type negotiation for trade sessions
- remote link-player data exchange support
- command frame packing/unpacking compatible with PRET command layout
- block send/request scaffolding

Must-cover PRET concepts:

- `CMD_LENGTH == 8`
- `LINKTYPE_TRADE_CONNECTING`
- `LINKTYPE_TRADE`
- `LINKCMD_INIT_BLOCK`
- `LINKCMD_CONT_BLOCK`
- `LINKCMD_SEND_BLOCK_REQ`

Exit criteria:

- the device can survive generic link setup and block requests without desync

### 4. Pre-menu trade data serving

Deliverables:

- support for the observed pre-menu block requests:
  - `200`
  - `200`
  - `200`
  - `220`
  - `40`
- a minimal but valid outgoing partner dataset that satisfies the trade room
- receive-side parsing for the same blocks so we can inspect partner state

Exit criteria:

- the game reaches and holds the trade menu with the RP2040 partner

### 5. Trade-room state machine

Deliverables:

- selection readiness handling
- leader/follower resolution behavior
- cancel path handling
- confirmation path handling
- trade start handling
- finish/ack handling after the swap

Must-cover commands:

- `LINKCMD_READY_TO_TRADE`
- `LINKCMD_SET_MONS_TO_TRADE`
- `LINKCMD_REQUEST_CANCEL`
- `LINKCMD_PLAYER_CANCEL_TRADE`
- `LINKCMD_BOTH_CANCEL_TRADE`
- `LINKCMD_READY_CANCEL_TRADE`
- `LINKCMD_START_TRADE`
- `LINKCMD_READY_FINISH_TRADE`
- `LINKCMD_CONFIRM_FINISH_TRADE`

Exit criteria:

- one complete happy-path trade works end to end without persistence
- cancel and disconnect paths return to a clean idle state

### 6. Pokemon data integration

Deliverables:

- identify the minimum Gen 3 structures needed to emulate a legal trade partner
- define what part of the full trade data must be kept live in RAM during a session
- define which subset must be stored long-term to reproduce `pk-ball` behavior
- capture the player's selected traded Pokemon and all required companion metadata

Likely stored data for v1:

- the traded `struct Pokemon` or equivalent serialized party-mon form
- nickname and OT/trainer identity fields needed to rebuild menu-facing data
- any mail/ribbon data needed for the traded slot to remain acceptable
- a seed/default outgoing party template for the other slots

Exit criteria:

- the traded Pokemon can be captured from a live session and inserted into the outgoing partner dataset

### 7. Persistence and format versioning

Deliverables:

- a Gen 3 storage region layout on top of the RP2040 EEPROM wrapper
- CRC-backed metadata footer similar in spirit to Gen 1/2
- format versioning that can evolve without breaking older saves
- boot-time validation and reseed behavior

Recommended storage behavior:

- store one persisted Gen 3 partner state region
- reseed from built-in defaults on invalid CRC/version
- commit writes only when a trade fully completes

Exit criteria:

- Gen 3 traded data survives reboot
- corrupted data is rejected and rebuilt safely

### 8. Verification

Minimum matrix:

- Ruby or Sapphire
- FireRed or LeafGreen
- Emerald
- happy-path trade
- selection cancel path
- confirmation cancel path
- disconnect mid-session
- reboot and persisted trade path

Preferred matrix:

- real hardware plus emulator-assisted logging
- repeated trade/save/reboot cycles with the same board

Exit criteria:

- Gen 3 v1 is boringly repeatable, not just a one-off successful demo

## Near-term implementation order

Recommended next six tasks:

1. Freeze the Gen 3 target as RP2040-only for v1.
2. Create a fresh `pk-ball-rp2040-gen3` skeleton rather than branching inside `pk-ball-rp2040-gen12`.
3. Build a transport-only experiment that logs 16-bit multiplayer frames.
4. Add PRET-shaped command packing and block-request handling.
5. Serve the pre-menu `200/220/40` data blocks from a static seed dataset.
6. Only after the trade room is stable, implement capture-and-persist of one traded Pokemon.

## Explicit non-goals for v1

- wireless adapter support
- cross-generation conversion
- merging Gen 3 directly into the existing Gen 1/2 timing loop
- full cleanup of the Gen 1/2 firmware before Gen 3 transport exists
- support for every multiplayer feature beyond cable trading
