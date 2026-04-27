# Gen 3 Research Sources

Primary protocol and implementation references used for this planning pass:

## Hardware and transport

- GBATEK mirror in `afska/gba-link-connection`:
  - https://github.com/afska/gba-link-connection/blob/master/docs/gbatek.md
  - Useful sections:
    - SIO Multi-Player Mode
    - SIO Control Registers Summary
    - GBA Wireless Adapter

## Game implementation references

- PRET Pokemon Emerald:
  - https://github.com/pret/pokeemerald
  - https://github.com/pret/pokeemerald/blob/master/include/link.h
  - https://github.com/pret/pokeemerald/blob/master/src/link.c

- PRET Pokemon FireRed:
  - https://github.com/pret/pokefirered
  - https://github.com/pret/pokefirered/blob/master/src/trade.c
  - https://github.com/pret/pokefirered/blob/master/src/trade_scene.c

## Existing homebrew or community implementations

- Pokemon Gen 3 to Gen X:
  - https://github.com/Lorenzooone/Pokemon-Gen3-to-Gen-X

- Poke Transporter GB:
  - https://github.com/GearsProgress/Poke_Transporter_GB

- Gen 3 Distribution ROMs / wireless-focused community work:
  - https://github.com/Goppier/Gen3DistributionRoms

- GBA link homebrew library:
  - https://github.com/afska/gba-link-connection

## Testing reference

- mGBA:
  - https://github.com/mgba-emu/mgba

Notes:

- This source list is intentionally biased toward primary repos and project-maintainer documentation.
- The PRET repos are the best software-behavior reference.
- The GBATEK mirror is the best transport-layer reference used in this pass.
