# HUD Theme Memory Offsets

All offsets are relative to the MBAACC module base (`MBAA.exe`). Colors are stored as 32-bit ARGB values in little-endian order (bytes appear `BB GG RR AA` in memory). Default colors are listed for reference. Overlay speeds control the HUD shine animation direction.

| Element | Offset | Overlay Speed | Default Color |
| --- | --- | --- | --- |
| Meter lower third (<100%) | `0x2551F` | `1` | `#ffc80000` |
| Meter middle third (100–199.9% or 100–149.9% H) | `0x25536` | `2` | `#ffc8c800` |
| Meter upper third (200%+ or 150%+ H) | `0x25544` | `3` | `#ff00c800` |
| Unlimited | `0x25466` | `2` | `#ff3296ff` |
| Heat | `0x2547B` | `-2` | `#ff5a5ae6` |
| Max | `0x2549C` | `-2` | `#fffaa000` |
| Blood Heat | `0x254BA` | `-2` | `#ffb4b4b4` |
| Break | `0x25567` | `-2` (overlay locked) | `#ffbe64c8` |
| Guard bar (highest quality) | `0x252CC` | — | `#ff00bee6` |
| Guard bar (lowest quality) | `0x252C6` | — | `#ffe60a0a` |
| Guard break | `0x252B8` | — | `#ff767676` |

> Source: https://github.com/armonte/hantei-chan/issues/56
