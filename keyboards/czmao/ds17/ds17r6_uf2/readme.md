# DS17R6

A 5x4 numpad keyboard with direct pin wiring and WS2812 RGB underglow.

* Keyboard Maintainer: [micahyy](https://github.com/micahyy)
* Hardware Supported: CZMAO DS17R6 PCB
* Hardware Availability: [Taobao](https://item.taobao.com/item.htm?ft=t&id=848078753675)

Make example for this keyboard (after setting up your build environment):

    make czmao/ds17/ds17r6:default

Flashing example for this keyboard:

    make czmao/ds17/ds17r6:default:flash

See the [build environment setup](https://docs.qmk.fm/#/getting_started_build_tools) and the [make instructions](https://docs.qmk.fm/#/getting_started_make_guide) for more information. Brand new to QMK? Start with our [Complete Newbs Guide](https://docs.qmk.fm/#/newbs).

## Bootloader

Enter the bootloader in 3 ways:

* **Bootmagic reset**: Hold down the key at (0,0) in the matrix (usually the top left key or Escape) and plug in the keyboard
* **Physical reset button**: Briefly short the reset pads on the back of the PCB
* **Keycode in layout**: Press the key mapped to `QK_BOOT` if it is available
