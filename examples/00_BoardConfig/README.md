# Custom board config

Full guide: **[docs/Custom-Boards.md](../../docs/Custom-Boards.md)**

This sketch prints camera / SD / mic / Wi-Fi / Ethernet GPIOs from **`board_config.h`
in this folder**, then tries camera (and optionally SD + mic).

It does not assume Guition, Waveshare, or any other brand. If Serial `CFG:` pins
are wrong, change this folder’s header — other examples have their own copy.
