# ChipWhisperer RP2040 Target

An extension to the ChipWhisperer platform that adds the **Raspberry Pi RP2040** as a target: a bare-metal HAL, OpenOCD config for flashing, and example capture notebook. Includes a side-by-side comparison against the ATXmega128A4U.

## What's here

| Path | What it is |
|------|------------|
| `firmware/mcu/hal/rp2040/` | RP2040 HAL (UART, trigger, clocking) + pico-sdk CMake build |
| `openocd/` | OpenOCD config + `run_openocd.sh` to flash over SWD via the ChipWhisperer |
| `rp2040.ipynb` | Example capture script for the RP2040 |
| `xmega.ipynb` | Comparison capture script on the ATXmega128A4U |

## Quick start

One-time setup. Point the build at a pico-sdk checkout (needs `arm-none-eabi-gcc` and `cmake`):

```bash
export PICO_SDK_PATH=/path/to/pico-sdk
ln -s "$PICO_SDK_PATH" firmware/mcu/hal/rp2040/pico-sdk
```

From the notebook, put the ChipWhisperer into MPSSE mode so it can act as the SWD programmer:

```python
import chipwhisperer as cw
scope = cw.scope()
if scope.check_feature("MPSSE"):
    print("MPSSE supported!")
else:
    print("MPSSE not supported.")

scope.enable_MPSSE(1)
```

Build SimpleSerial-AES for the RP2040 and flash it over SWD (PID `0xace2` is the Lite, the rest are includded in the Debugging reference):

```bash
%%bash -s "$PLATFORM" "$CRYPTO_TARGET" "$SS_VER"
cd firmware/mcu/simpleserial-aes
make PLATFORM=$1 CRYPTO_TARGET=$2 SS_VER=$3 -j
openocd -f ../../../openocd/cw_openocd.cfg \
    -c "ftdi vid_pid 0x2b3e 0xace2" \
    -c "transport select swd" \
    -f ../../../openocd/rp2040.cfg \
    -c "program simpleserial-aes-CW308_RP2040.bin verify reset exit 0x10000000"
```

After flashing, take the ChipWhisperer back out of MPSSE mode before capturing:

```python
scope = cw.scope()
scope.finish_mpsse_setup()
scope.dis()
```

`rp2040.ipynb` includes the flashing steps and captures traces. The RP2040 speaks SimpleSerial just like any other CW target.

## Pin mapping

RP2040 on a CW308 UFO board:

| RP2040 pin | Function | CW308 header |
|------------|----------|--------------|
| GPIO 0 | UART0 TX → CW reads | TIO2 (pin 12) |
| GPIO 1 | UART0 RX ← CW writes | TIO1 (pin 10) |
| GPIO 27 | Trigger out | TIO4 (pin 16) |
| XIN | Clock in | HS2 |

ChipWhisperer's HS2 clocks the chip directly (12 MHz for RP2040), and the internal PLL is powered down to keep power traces clean. Baud is 38400 for SimpleSerial v1, 230400 for v2.

## Hardware

- **Raspberry Pi RP2040** on a **CW308 UFO** target board
- A **ChipWhisperer**

## References

- [ChipWhisperer documentation](https://chipwhisperer.readthedocs.io/)
- [ChipWhisperer debugging reference](https://chipwhisperer.readthedocs.io/en/latest/debugging.html) — device PIDs and MPSSE/OpenOCD troubleshooting
- [RP2040 Datasheet](https://datasheets.raspberrypi.com/rp2040/rp2040-datasheet.pdf)
- [Raspberry Pi Pico SDK](https://github.com/raspberrypi/pico-sdk)

## Note

I built this for in-house use as a patch on top of ChipWhisperer. Feel free to take and build upon it.
