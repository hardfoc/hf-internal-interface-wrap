# API reference

Every class, member and type of the interfaces and their implementations, generated from the headers.

[Open the API reference](api/index.html){ .md-button .md-button--primary }

## Start here

| Page | What it documents |
| --- | --- |
| [`BaseGpio`](api/class_base_gpio.html) | Pins: the public calls and the `*Impl()` methods a port implements |
| [`BaseCan`](api/class_base_can.html) | CAN: send, receive, filters, bus status |
| [`hf_can_message_t`](api/structhf__can__message__t.html) | The FD-sized CAN frame and its helpers |
| [`BaseSpi`](api/class_base_spi.html) and [`BaseI2c`](api/class_base_i2c.html) | Bus devices |
| [`EspCan`](api/class_esp_can.html) | The ESP32 TWAI implementation and its configuration |
| [`HfSpscRing`](api/class_hf_spsc_ring.html) | The lock-free ring behind `EspCan`'s receive path |
| [File list](api/files.html) | Every header |

The [API guide](api-guide.md) shows the same API by task, with code that compiles.
