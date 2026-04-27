# Gen 3 Research Notes

## 1. Why Gen 3 is a separate project slice

The current sketch is clearly Gen 1/2 shaped:

- It uses three link pins: `MOSI_`, `MISO_`, and `SCLK_` in [`pk-ball/pk-ball.ino`](../pk-ball/pk-ball.ino).
- It shifts one byte at a time in `transferBit()`.
- Its state machine is built around the Gen 1/2 trade-centre handshake and fixed-size payload blocks.

That architecture is a good fit for Gen 1 and Gen 2, but it is not a direct fit for native Gen 3 trading.

Inference from the hardware references:

- Native Gen 3 cable communication uses GBA multiplayer mode, which is a 16-bit transport with parent/child role handling.
- That mode uses `SC`, `SD`, `SI`, and `SO`, so there is at least one more signal in play than the current sketch exposes.
- The project should treat Gen 3 as a separate transport layer, not just a third mode value in the existing state machine.

## 2. Gen 3 hardware/link-layer facts

From the GBA serial documentation mirrored in `gba-link-connection/docs/gbatek.md`:

- GBA multiplayer mode is `Multiplay 16bit`, selected through `RCNT`/`SIOCNT`.
- Multiplayer mode supports up to 4 units.
- Supported baud rates are `9600`, `38400`, `57600`, and `115200`.
- `SIOMLT_SEND` sends one local 16-bit word per transfer.
- `SIOMULTI0-3` receive the 16-bit words for all connected players after each transfer.
- The bus has an explicit parent/master and child arrangement.
- Readiness and error reporting are exposed in `SIOCNT`.

This is the key protocol jump from Gen 1/2:

- Gen 1/2 in this repo is effectively a streamed byte exchange.
- Gen 3 is a framed 16-bit multiplayer exchange.

## 3. What the decomp projects show

The PRET Gen 3 repos are the best source for the game-facing protocol:

- `pret/pokeemerald`
- `pret/pokefirered`

Important details from `include/link.h` in `pret/pokeemerald`:

- `MAX_LINK_PLAYERS` is `4`.
- `CMD_LENGTH` is `8`.
- `BLOCK_BUFFER_SIZE` is `0x100`.
- The games define trade-specific link commands such as:
  - `LINKCMD_READY_TO_TRADE`
  - `LINKCMD_START_TRADE`
  - `LINKCMD_SET_MONS_TO_TRADE`
  - `LINKCMD_READY_FINISH_TRADE`
  - `LINKCMD_CONFIRM_FINISH_TRADE`
  - `LINKCMD_READY_CANCEL_TRADE`
  - `LINKCMD_PLAYER_CANCEL_TRADE`
  - `LINKCMD_BOTH_CANCEL_TRADE`
- The link type constants include:
  - `LINKTYPE_TRADE`
  - `LINKTYPE_TRADE_CONNECTING`
  - `LINKTYPE_TRADE_SETUP`
  - `LINKTYPE_TRADE_DISCONNECTED`

Important details from `src/link.c` in `pret/pokeemerald`:

- The transport is command-oriented, not just raw byte mirroring.
- The engine supports explicit block transfer commands such as `LINKCMD_INIT_BLOCK`, `LINKCMD_CONT_BLOCK`, and `LINKCMD_SEND_BLOCK_REQ`.
- Link-player metadata is exchanged in a `LinkPlayerBlock`, including version, trainer ID, name, progress flags, and language.

Practical implication:

- A usable Gen 3 implementation will likely need two layers:
  - a GBA multiplayer transport layer
  - a Pokemon trade protocol layer on top of it

## 4. Existing Gen 3-capable implementations and examples

### A. PRET decomp repos

Use these as the canonical software behavior reference:

- `pret/pokeemerald`
- `pret/pokefirered`

Why they matter:

- They expose the real command names, link types, and block-transfer model.
- They are the best source for reconstructing the order of trade-room messages and data blocks.

### B. `Lorenzooone/Pokemon-Gen3-to-Gen-X`

This is the strongest homebrew example I found for Gen 3 interaction.

What it shows:

- It is GBA homebrew.
- It supports a Gen 3 mode.
- Its README says Gen 3 is for trading with other GBA consoles running the homebrew.
- It also exposes an `Act as` option, which strongly suggests explicit handling of link-side role behavior.
- It is multibootable and designed around real hardware usage.

Important caveat:

- Its README explicitly says it uses the DMG/GB/GBC link cable and is not compatible with the GBA link cable.

That makes it useful as:

- a data-translation and high-level trade-flow reference
- a proof that Gen 3-facing trade homebrew exists

But not automatically a drop-in transport match for native cartridge-to-cartridge Gen 3 cable trading.

### C. `GearsProgress/Poke_Transporter_GB`

This is another strong hardware-oriented example.

What it shows:

- It is a GBA homebrew project designed to bridge older games into Gen 3.
- Its README credits both `Pokemon Gen 3 to Gen X` and `gba-link-connection`.
- It supports multiboot deployment and real hardware workflows.

Why it matters here:

- It is a good reference for practical GBA homebrew project structure.
- It is likely useful for cartridge hot-swap, multiboot, and link helper patterns, even though it is not a native Gen 3 trade clone.

### D. `Goppier/Gen3DistributionRoms`

This is a useful side reference for the wireless path.

What it shows:

- It targets FireRed, LeafGreen, and Emerald distribution workflows.
- Its README lists `2x GBA Wireless Adapters` as required hardware.

Why it matters:

- It is evidence that Gen 3 communication work in the community often splits into separate cable and wireless implementations.
- It reinforces that wireless support is its own project track and should not be mixed into the first Gen 3 cable milestone.

### E. `afska/gba-link-connection`

This is a useful low-level homebrew library reference.

Why it matters:

- It is aimed at GBA serial port interaction and multiplayer support for homebrew.
- Its bundled `gbatek.md` mirror is already useful for the transport layer.
- Even if we do not copy its design, it is a strong reference for how modern GBA homebrew structures link access.

### F. `mGBA`

This is a strong testing reference, not a protocol reference.

Why it matters:

- The official README lists local link cable support.
- The same README lists wireless adapter support as planned rather than already shipped.

Practical use:

- mGBA is a good candidate for early cable-side iteration and scripted verification.
- It is not enough by itself for the wireless path.

## 5. What would be needed in this repo

### Hardware

Likely needed:

- a Gen 3-specific hardware note or adapter design
- support for the GBA multiplayer signal set rather than the current three-wire setup
- validation of voltage and cable assumptions before any firmware design is trusted

This is partly inference from the GBA transport docs plus the current codebase shape.

### Firmware architecture

Likely needed:

- a new Gen 3 transport module rather than extending the current Gen 1/2 byte loop directly
- explicit master/child role handling
- 16-bit frame exchange support
- block-transfer support
- a separate Gen 3 trade state machine
- Gen 3 party/mon serialization and validation rules
- a new persistence format for stored Gen 3 data

### Research still worth doing

Still needed before writing much firmware:

- identify the exact trade-room command sequence and block contents from the PRET trade code
- capture or script a minimal happy-path trade timeline
- decide whether the first target is:
  - real cartridge over real GBA cable
  - emulator-assisted harness
  - GBA homebrew interoperability only

## 6. Recommended project stance

Recommended scope for Gen 3 v1:

- Target native cable trade first.
- Ignore wireless support for v1.
- Build a transport proof-of-life before trying to store or swap Pokemon.
- Keep Gen 3 code in its own directory or sketch until it is stable enough to share helpers with Gen 1/2.

## 7. Concrete trade sequence found in PRET

The code trace in `pret/pokefirered` and `pret/pokeemerald` shows a stable cable-trade flow:

1. Open link with `LINKTYPE_TRADE_CONNECTING`.
2. Finish generic link-player exchange.
3. Exchange pre-menu trade data in blocks:
   - `200` bytes
   - `200` bytes
   - `200` bytes
   - `220` bytes
   - `40` bytes
4. Selection phase:
   - follower sends `LINKCMD_READY_TO_TRADE`
   - leader resolves both ready states
   - leader sends `LINKCMD_SET_MONS_TO_TRADE`
5. Confirmation phase:
   - each side sends `LINKCMD_INIT_BLOCK` when its selection is valid and confirmed
   - cancel/backout uses `LINKCMD_READY_CANCEL_TRADE`
   - once both are ready, leader sends `LINKCMD_START_TRADE`
6. After the trade animation and local mon swap:
   - both sides send `LINKCMD_READY_FINISH_TRADE`
   - leader sends `LINKCMD_CONFIRM_FINISH_TRADE`
   - receiver advances to post-trade evolution handling

The full write-up is in `trade-timeline.md`.
