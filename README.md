<h2><img src="resources/logo-256x74.png"/></h2>

A Nintendo DS emulator developed for fun, with performance and multicore CPUs in mind.
A nearly from scratch rewrite of my previous emulator aiming to try new techniques and achieve higher code quality.

**This is highly experimental software.**
I am developing this emulator for fun and learning only.
I do **not** intend to replace any of the established DS emulators.  
**If you are looking for a mature emulator, [melonDS](https://github.com/melonDS-emu/melonDS) is the way to go.**

## Current Status

- Full emulation of most core hardware. Most notably missing though are:
  - Any resemblance of accurate timing emulation
  - WiFi, sound capture and microphone emulation
- A somewhat accurate software 3D renderer (not as accurate as melonDS though) with:
  - Somewhat accurate edge and vertex attribute interpolation
  - Support for fog, edge-marking and anti-aliasing (based on half-assed coverage calculations...)
- In terms of emulator optimizations there is:
  - An ARM to x86_64 dynamic recompiler for fast CPU emulation (using my own [lunatic](https://github.com/fleroviux/lunatic) library).
  - Rendering of the 2D graphics engines on separate threads

## Media

![hgss](resources/hgss.png)

## Credit
- Martin Korth: for [GBATEK](http://problemkaputt.de/gbatek.htm)
- [Arisotura](https://github.com/Arisotura/): for hardware documentation, especially regarding the 3D engine
- [Hydr8gon](https://github.com/Hydr8gon/): for some hints and sharing 3D engine insights
- [StrikerX3](https://github.com/StrikerX3): for documenting the precise formula for polygon edge interpolation
- [Jaklyy](https://github.com/Jaklyy/): for 3D engine research and sharing her insights

## Copyright

irisdual is Copyright © 2024 fleroviux. All rights reserved.<br>
irisdual is released under a free for non-commercial use license. Refer to the [LICENSE](LICENSE) file for details.

Nintendo DS is a registered trademark of Nintendo Co., Ltd.
