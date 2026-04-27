# Gen 3 Trade Timeline

This file turns the PRET code trace into a practical timeline for native Gen 3 cable trading.

## Scope

This timeline is for:

- cable trades
- two-player Pokemon Gen 3 trading
- the normal trade-room path

It is based mainly on:

- `pret/pokefirered` `src/trade.c`
- `pret/pokefirered` `src/trade_scene.c`
- `pret/pokeemerald` `src/trade.c`
- `pret/*` `include/link.h`

FireRed and Emerald match closely enough here that they can be treated as the same first-pass protocol target.

## High-level shape

The trade is not just:

- connect
- pick a mon
- swap data

Instead it looks like this:

1. Open the GBA link session as a trade session.
2. Finish generic link-player exchange.
3. Exchange party-related data blocks before the menu becomes live.
4. Negotiate mon selection.
5. Negotiate trade confirmation.
6. Start the trade animation.
7. Swap the mons locally.
8. Exchange finish/ack messages.
9. Move into post-trade evolution handling.

## Phase 1. Link session setup

Observed in PRET:

- The menu startup sets `gLinkType = LINKTYPE_TRADE_CONNECTING`.
- The game opens the link.
- It waits for remote link players and for link-player data exchange completion.
- Only after that does it proceed into trade-specific data buffering.

Important takeaway:

- A Gen 3 implementation has to participate in the standard GBA link stack first.
- Trade logic starts after the generic link layer is already healthy.

## Phase 2. Pre-menu trade data exchange

Before the player can freely use the trade menu, the games exchange trade payloads in blocks.

From `BufferTradeParties()` in PRET:

- first pair of party mons: `200` bytes
- second pair of party mons: `200` bytes
- third pair of party mons: `200` bytes
- party mail block: `220` bytes
- gift ribbons block: `40` bytes

The leader requests these with `LINKCMD_SEND_BLOCK_REQ` via request types:

- `BLOCK_REQ_SIZE_200`
- `BLOCK_REQ_SIZE_220`
- `BLOCK_REQ_SIZE_40`

Important practical detail:

- The menu is not operating on just one selected Pokemon.
- The implementation needs enough of the full partner state to render the room and validate trades.

## Phase 3. Selection negotiation

### Local readiness

When a player chooses a valid Pokemon:

- the follower sends `LINKCMD_READY_TO_TRADE`
- the payload includes the selected cursor/slot position

The leader does not send a matching readiness message for its own local selection.
Instead:

- the leader marks itself ready locally
- it waits for the partner readiness block to arrive

### Leader decision point

Once both players are marked ready:

- the leader sends `LINKCMD_SET_MONS_TO_TRADE`
- the payload includes the leader's selected slot

The follower receives that and moves both selected mons into the center display.

### Selection cancel paths

If cancel is chosen before both selections lock in:

- selecting Cancel locally sends `LINKCMD_REQUEST_CANCEL`
- the leader resolves the combined state and sends one of:
  - `LINKCMD_PARTNER_CANCEL_TRADE`
  - `LINKCMD_PLAYER_CANCEL_TRADE`
  - `LINKCMD_BOTH_CANCEL_TRADE`

Practical implication:

- A device that emulates the partner cannot just mirror button-like events.
- It has to maintain the same leader/follower state machine and emit the correct resolved cancel command.

## Phase 4. Confirmation negotiation

After both selected mons are displayed, both sides see the "Is this trade okay?" prompt.

If the player confirms and the selected mons are valid:

- the game sends `LINKCMD_INIT_BLOCK`

In this context `LINKCMD_INIT_BLOCK` is being reused as the "confirmed and valid" signal for the trade confirmation phase.

If the player backs out or validation fails:

- the game sends `LINKCMD_READY_CANCEL_TRADE`

The leader waits until both local and partner confirmation states are known.

### Confirm success

If both sides are ready:

- the leader sends `LINKCMD_START_TRADE`

### Confirm failure

If either side canceled or the selection was not tradable:

- the leader sends `LINKCMD_PLAYER_CANCEL_TRADE`

Important implementation note:

- Validation happens before the trade starts.
- That means a Gen 3 emulator device needs more than a transport shim; it needs enough semantic data to avoid sending impossible or illegal trade confirmations.

## Phase 5. Entering the actual trade scene

After `LINKCMD_START_TRADE`:

- the menu fades out
- both games preserve the chosen mon indexes
- the menu code exits
- the trade scene callback takes over

This is the handoff from:

- menu protocol

to:

- trade animation and post-swap protocol

## Phase 6. Finish handshake after the swap

In the trade scene:

- the animation runs
- local party data is swapped with `TradeMons(...)`

After the local swap is complete:

- each side sends `LINKCMD_READY_FINISH_TRADE`

The leader waits until both finish statuses are ready.
Then:

- the leader sends `LINKCMD_CONFIRM_FINISH_TRADE`

When the partner receives that:

- it advances into the post-trade evolution callback

This is the cleanest end-of-trade ack sequence in the codebase:

1. both sides say "my trade is complete"
2. leader says "confirmed, move on"

## What this means for this repo

To support native Gen 3 cable trading, the implementation needs all of the following:

### Transport layer

- GBA multiplayer-mode transport
- parent/child role support
- 16-bit framed transfers
- generic link player exchange
- block request and block send support

### Trade-room block layer

- ability to serve:
  - `200`-byte party pair blocks
  - `220`-byte mail block
  - `40`-byte ribbon block
- ability to receive and parse the same

### Trade-room command layer

- `LINKCMD_READY_TO_TRADE`
- `LINKCMD_SET_MONS_TO_TRADE`
- `LINKCMD_REQUEST_CANCEL`
- `LINKCMD_PARTNER_CANCEL_TRADE`
- `LINKCMD_PLAYER_CANCEL_TRADE`
- `LINKCMD_BOTH_CANCEL_TRADE`
- `LINKCMD_INIT_BLOCK`
- `LINKCMD_READY_CANCEL_TRADE`
- `LINKCMD_START_TRADE`
- `LINKCMD_READY_FINISH_TRADE`
- `LINKCMD_CONFIRM_FINISH_TRADE`

### Data/model layer

- full Gen 3 `struct Pokemon` compatible serialization
- party layout matching the requested block sizes
- mail data support
- gift ribbon data support
- enough legality/validation logic to avoid impossible selections

## Recommended v1 target

The narrowest realistic Gen 3 v1 is:

1. implement cable multiplayer transport
2. implement generic block send/request behavior
3. emulate the trade-room command sequence
4. start with a minimal but valid two-party dataset
5. only then add persistent storage of captured mons

## Most important design insight

The existing Gen 1/2 sketch is centered on:

- immediate byte streaming
- a single trade-centre state machine
- fixed generation-specific offsets inside one outgoing block

Gen 3 needs:

- framed multiplayer transport
- a generic link layer
- block transfers before menu interaction
- a second-layer trade protocol on top

That is why Gen 3 should begin as a separate implementation track rather than a few more `MODE_*` branches in the current sketch.
