# RP2040 Gen 3 Task Breakdown

This file turns the implementation plan into concrete work items for the first Gen 3 build.

## Phase 0. Baseline review

- [ ] Record the current RP2040 Gen 1/2 divergences from the original `pk-ball` sketch.
- [ ] Decide whether the RP2040 Gen 1/2 idle reset and colosseum/reset behavior should be corrected before using it as a branch base.
- [ ] Decide whether Gen 3 starts from a copy of the RP2040 port or from a cleaner new RP2040 sketch that only reuses helpers.

## Phase 1. New project skeleton

- [x] Create a new directory for Gen 3 firmware, for example `pk-ball-rp2040-gen3`.
- [x] Add a README that states the user-visible parity goal with `pk-ball`.
- [ ] Copy or extract only the reusable RP2040 pieces:
  - [x] storage wrapper
  - [ ] serial logging helpers
  - [x] board pin definitions
- [ ] Keep Gen 1/2 protocol code out of the first Gen 3 skeleton.

## Phase 2. Transport proof-of-life

- [ ] Define the Gen 3 signal mapping on the RP2040 board and link board.
- [ ] Decide whether the first transport attempt uses:
  - [ ] tight polling plus IRQ
  - [ ] PIO
  - [ ] another hardware-assisted path
- [ ] Build a frame logger that records:
  - [ ] local role
  - [ ] raw 16-bit words
  - [ ] timing gaps
  - [ ] disconnect or timeout events
- [ ] Verify stable reconnect after idle.
- [ ] Capture at least one known-good frame log from a real or emulator partner.

## Phase 3. Generic link layer

- [ ] Implement PRET-compatible command word framing around `CMD_LENGTH == 8`.
- [ ] Implement send/receive buffers for command words.
- [ ] Implement link-type setup for trade sessions.
- [ ] Implement link-player block exchange.
- [ ] Add block send scaffolding for:
  - [ ] `LINKCMD_INIT_BLOCK`
  - [ ] `LINKCMD_CONT_BLOCK`
- [ ] Add block request handling for:
  - [ ] `LINKCMD_SEND_BLOCK_REQ`
- [ ] Add debug logs for decoded command names instead of raw words only.

## Phase 4. Static seed trade-room bring-up

- [ ] Define a static seed partner dataset for Gen 3.
- [ ] Implement block responders for:
  - [ ] first `200`-byte party pair block
  - [ ] second `200`-byte party pair block
  - [ ] third `200`-byte party pair block
  - [ ] `220`-byte mail block
  - [ ] `40`-byte ribbon block
- [ ] Verify the trade room opens without immediate cancellation or link drop.
- [ ] Log every requested block type and size during menu bring-up.

## Phase 5. Trade-room command flow

- [ ] Implement receive handling for:
  - [ ] `LINKCMD_READY_TO_TRADE`
  - [ ] `LINKCMD_REQUEST_CANCEL`
  - [ ] `LINKCMD_READY_CANCEL_TRADE`
  - [ ] `LINKCMD_INIT_BLOCK`
- [ ] Implement leader-side resolution for:
  - [ ] `LINKCMD_SET_MONS_TO_TRADE`
  - [ ] `LINKCMD_PLAYER_CANCEL_TRADE`
  - [ ] `LINKCMD_BOTH_CANCEL_TRADE`
  - [ ] `LINKCMD_START_TRADE`
- [ ] Implement finish handshake handling for:
  - [ ] `LINKCMD_READY_FINISH_TRADE`
  - [ ] `LINKCMD_CONFIRM_FINISH_TRADE`
- [ ] Verify that cancel-before-selection and cancel-at-confirmation both return to a clean session state.

## Phase 6. Data capture and model integration

- [ ] Identify the remote slot/party data required to know which Pokemon the player traded.
- [ ] Extract the selected incoming Pokemon from the Gen 3 trade data.
- [ ] Identify all extra metadata required to offer that Pokemon back later:
  - [ ] trainer identity
  - [ ] nickname
  - [ ] mail
  - [ ] ribbons or other slot-linked trade data
- [ ] Replace slot 1 of the outgoing seed dataset with the captured Pokemon.
- [ ] Verify the traded Pokemon appears as the offered partner Pokemon on the next trade.

## Phase 7. Persistence

- [ ] Define a Gen 3 storage payload structure.
- [ ] Define a Gen 3 metadata footer:
  - [ ] magic
  - [ ] format version
  - [ ] generation id
  - [ ] payload length
  - [ ] CRC
- [ ] Add boot-time validation and reseed behavior.
- [ ] Commit stored data only after a fully completed trade.
- [ ] Verify persistence across power cycle.

## Phase 8. Verification and tooling

- [ ] Build a repeatable test checklist for Ruby/Sapphire.
- [ ] Build a repeatable test checklist for FireRed/LeafGreen.
- [ ] Build a repeatable test checklist for Emerald.
- [ ] Capture known-good logs for:
  - [ ] connect
  - [ ] pre-menu block exchange
  - [ ] selection
  - [ ] confirmation
  - [ ] trade finish
  - [ ] cancel
  - [ ] disconnect
- [ ] Decide whether a lightweight emulator harness is worth adding before broader hardware testing.

## Definition of done for Gen 3 v1

- [ ] The RP2040 device can enter the Gen 3 trade room with a retail game.
- [ ] One trade completes successfully.
- [ ] The traded Pokemon is captured.
- [ ] The traded Pokemon is stored with CRC-backed metadata.
- [ ] After reboot, the stored Pokemon is offered back in a later trade.
- [ ] Cancel and disconnect paths do not require reflashing or manual recovery.
