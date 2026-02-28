# STM32F429I-DISCO Demo – modm + Pigweed + ETL

A minimal bare-metal C++20 demo for the **STM32F429I-DISCO** development board
that integrates three embedded libraries via **CMake**:
The demo is compilable using clang. While pigweed is clang ready, unfortunately modm is not, so it required a bunch of
ugly patches in the CMakeLists.txt. It would be a better idea to use gcc instead, since pigweed is gcc friendly too,
but i just wanted to do some testing on clang on embedded for educational purpose. 

| Library | Role in this demo |
|---------|-------------------|
| **[modm](https://modm.io)** | Hardware abstraction – board init, GPIO (LEDs), UART1, SysTick delay |
| **[Pigweed](https://pigweed.dev)** | `pw_log_tokenized` – compact binary logging, `pw_assert` safe-halt, `pw_status` / `pw_span` |
| **[ETL](https://www.etlcpp.com)** | `etl::vector` and `etl::string` – static containers, zero heap |

## Hardware

| Feature | Detail |
|---------|--------|
| MCU     | STM32F429ZIT6 (Cortex-M4F, 180 MHz, 2 MB Flash, 256 KB RAM) |
| Green LED | PG13 – heartbeat, toggles every 500 ms |
| Red LED   | PG14 – flashes briefly after each batch is processed |
| UART1 TX  | PA9  – 115 200 Bd, 8N1 – carries all `pw_log` output |
| UART1 RX  | PA10 |

Connect any USB-serial adapter to PA9/PA10 or use the ST-LINK VCP (if wired
on your board revision) to read the log output.

## Prerequisites

### Tools

| Tool | Version | Install |
|------|---------|---------|
| `arm-none-eabi-gcc` | ≥ 12 | [Arm GNU Toolchain](https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads) |
| CMake | ≥ 3.25 | <https://cmake.org/download/> |
| Ninja | any | `pip install ninja` or your package manager |
| Python | ≥ 3.10 | <https://python.org> |
| lbuild | ≥ 1.0 | `pip install lbuild` |
| Git | ≥ 2.20 | (for submodules) |

### Flashing

- **OpenOCD** `openocd -f board/stm32f429disc1.cfg …`
- or **STM32CubeProgrammer** / **ST-LINK Utility** using the generated `.hex`

## Getting Started

### 1 – Clone with submodules

```bash
git clone --recurse-submodules https://github.com/YOUR_USER/stm32demo.git
cd stm32demo
```

If you already cloned without `--recurse-submodules`:

```bash
git submodule update --init --recursive
```

### 2 – (Optional) pre-generate modm

CMake will run `lbuild build` automatically on first configure if the
`modm/` directory is absent.  You can also run it manually:

```bash
lbuild build          # reads lbuild.xml, writes modm/
```

### 3 – Configure

```bash
cmake --preset debug
# or: cmake --preset release
```

### 4 – Build

```bash
cmake --build --preset debug
```

Artifacts land in `build/debug/`:
- `stm32f429i_demo`       – ELF (for GDB)
- `stm32f429i_demo.bin`   – raw binary
- `stm32f429i_demo.hex`   – Intel HEX

### 5 – Flash

```bash
openocd -f board/stm32f429disc1.cfg \
        -c "program build/debug/stm32f429i_demo.hex verify reset exit"
```

### 6 – Read the log output

See [Tokenized Logging](#tokenized-logging) below.

## Tokenized Logging

This demo uses **pw_log_tokenized** instead of plain-text logging.  Every
`PW_LOG_*` call stores only a 32-bit hash token in the firmware binary — the
format string itself is stripped out at compile time.  At runtime, only the
token plus varint-encoded arguments are transmitted, wrapped in a
`$`-prefixed Base64 line.  The host-side tool looks the token up in a
database extracted from the ELF and reconstructs the original message.

### Why bother?

| Approach | Flash cost per log site | UART bytes per message |
|----------|------------------------|------------------------|
| `printf` / `pw_log_basic` | ~40–200 B (string in flash) | ~40–80 B (ASCII) |
| `pw_log_tokenized` | **4 B** (token only) | **8–16 B** (Base64) |

On a 2 MB device this makes little practical difference, but it
demonstrates the technique and the tooling end-to-end.

### Wire format

Each log message is one newline-terminated line on the UART:

```
$Qlqz/hM=
```

The payload inside the Base64 is `[ 4-byte LE token ][ varint args... ]`.
The `$` prefix lets the detokenizer recognise tokenized lines even when
mixed with other UART traffic.

### Token database

The CMake post-build step automatically extracts the token→string database
from the ELF after every build and writes it next to the ELF:

```
build/debug/stm32f429i_demo.tokens.csv
```

To regenerate it manually:

```bash
# Linux / macOS
PYTHONPATH=ext/pigweed/pw_tokenizer/py \
python3 -m pw_tokenizer.database create \
    --database build/debug/stm32f429i_demo.tokens.csv \
    build/debug/stm32f429i_demo
```

```cmd
:: Windows (cmd) – run from the repo root
set PYTHONPATH=ext\pigweed\pw_tokenizer\py
python -m pw_tokenizer.database create ^
    --database build\debug\stm32f429i_demo.tokens.csv ^
    build\debug\stm32f429i_demo
```

### Live decoding

Install the only host-side dependency (once):

```bash
pip install pyserial
```

Find your COM port: on Windows open **Device Manager → Ports (COM & LPT)**
and look for *STMicroelectronics STLink Virtual COM Port*.

```bash
# Linux / macOS
PYTHONPATH=ext/pigweed/pw_tokenizer/py \
python3 -m pw_tokenizer.serial_detokenizer \
    --device /dev/ttyACM0 --baudrate 115200 \
    build/debug/stm32f429i_demo.tokens.csv
```

```cmd
:: Windows (cmd) – run from the repo root
set PYTHONPATH=ext\pigweed\pw_tokenizer\py
python -m pw_tokenizer.serial_detokenizer ^
    --device COM4 --baudrate 115200 ^
    build\debug\stm32f429i_demo.tokens.csv
```

You can also point the tool directly at the ELF to skip the CSV step:

```bash
PYTHONPATH=ext/pigweed/pw_tokenizer/py \
python3 -m pw_tokenizer.serial_detokenizer \
    --device /dev/ttyACM0 --baudrate 115200 \
    build/debug/stm32f429i_demo
```

### Expected output

```
[DEMO] =========================================
[DEMO]  STM32F429I-DISCO  modm + Pigweed + ETL
[DEMO] =========================================
[DEMO] System clock: 180000000 Hz
[DEMO] ETL reading buffer capacity: 16
[DEMO] --- Batch #1 (t=8000 ms) ---
[DEMO]   t=500     raw=-49
[DEMO]   t=1000    raw=-48
...
[DEMO] batch mean=-8  n=16
```

## Project Structure

```
stm32demo/
├── CMakeLists.txt          # root build description
├── CMakePresets.json       # debug / release / minsizerel presets
├── lbuild.xml              # modm module selection for DISCO-F429ZI
├── toolchain/
│   └── arm-none-eabi.cmake # cross-compilation toolchain file
├── src/
│   ├── main.cpp                  # application entry point
│   ├── log_backend.cc            # pw_sys_io backend (WriteByte → UART1)
│   ├── log_tokenized_handler.cc  # pw_log_tokenized handler ($-Base64 over UART)
│   └── pw_assert_backend/
│       └── assert_backend.cc     # pw_assert_basic backend (safe-halt)
└── ext/
    ├── modm/               # git submodule – modm source + lbuild recipes
    └── pigweed/            # git submodule – Pigweed source
```

> **ETL** is fetched automatically by CMake `FetchContent` at configure time –
> no manual step required.

> **modm** lives in `ext/modm/` (submodule) and the *generated* library is
> written to `modm/` (git-ignored) by lbuild.

## How the Libraries Fit Together

```
main.cpp
  │
  ├── modm::Board::initialize()     ← sets up clocks, FPU, LEDs
  ├── Usart1::initialize()          ← UART1 for log output
  │
  ├── pw_log (PW_LOG_INFO / PW_LOG_WARN / …)
  │     └── PW_LOG_TOKENIZED_TO_GLOBAL_HANDLER_WITH_PAYLOAD macro
  │           └── _pw_log_tokenized_EncodeTokenizedLog()   ← log_tokenized.cc
  │                 └── pw_log_tokenized_HandleLog()        ← log_tokenized_handler.cc
  │                       └── "$" + Base64(token+args) + "\n" → UART1
  │
  ├── pw_assert (PW_CHECK_OK)
  │     └── pw_assert_basic_HandleFailure() in assert_backend.cc
  │           └── safe-halt + LED indicator
  │
  └── etl::vector<SensorReading, 16>  ← stack-allocated, no heap
        └── pw::span view passed to ProcessBatch()
```

## Customising

- **UART baud rate** – change `115200_Bd` in `main.cpp`; `log_tokenized_handler.cc` inherits the rate from the same UART instance.
- **Log verbosity** – define `PW_LOG_LEVEL` in the target's compile options
  (e.g. `target_compile_definitions(… PRIVATE PW_LOG_LEVEL=PW_LOG_LEVEL_DEBUG)`).
- **Additional modm modules** – add `<module>` entries to `lbuild.xml` and
  re-run `lbuild build`.
- **Additional Pigweed modules** – add the module's `public/` directory to
  `PIGWEED_INCLUDE_DIRS` in `CMakeLists.txt` and link any required `.cc` files.
