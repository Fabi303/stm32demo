/**
 * pw_assert_basic backend – handles assertion failures on bare metal.
 *
 * Pigweed's pw_assert_basic backend contract (pw_assert_basic/assert_basic.h):
 *
 *   void pw_assert_basic_HandleFailure(const char* file_name,
 *                                      int         line_number,
 *                                      const char* function_name,
 *                                      const char* message,
 *                                      ...);
 *
 * On assertion failure this implementation:
 *   1. Emits the failure details over UART1.
 *   2. Turns both LEDs on as a visual indicator.
 *   3. Disables interrupts and spins forever (safe-halt).
 */

#include "pw_assert_basic/assert_basic.h"

#include <cstdarg>

#include <modm/board.hpp>

// ─────────────────────────────────────────────────────────────────────────────
// Helpers (duplicated from log_backend.cc to keep each file self-contained)
// ─────────────────────────────────────────────────────────────────────────────

namespace {

inline void UartWrite(const char* str) {
    while (str && *str) {
        Board::stlink::Uart::write(static_cast<uint8_t>(*str++));
    }
}

inline void UartWriteN(const char* buf, size_t len) {
    for (size_t i = 0; i < len; ++i) {
        Board::stlink::Uart::write(static_cast<uint8_t>(buf[i]));
    }
}

inline void UartWriteInt(int value) {
    if (value < 0) {
        Board::stlink::Uart::write('-');
        value = -value;
    }
    char buf[12];
    int  pos = 11;
    buf[pos] = '\0';
    if (value == 0) {
        Board::stlink::Uart::write('0');
        return;
    }
    while (value > 0) {
        buf[--pos] = static_cast<char>('0' + value % 10);
        value /= 10;
    }
    UartWrite(buf + pos);
}

}  // namespace

// ─────────────────────────────────────────────────────────────────────────────
// Backend implementation
// ─────────────────────────────────────────────────────────────────────────────

extern "C"
void pw_assert_basic_HandleFailure(const char* file_name,
                                   int         line_number,
                                   const char* function_name,
                                   const char* message,
                                   ...) {
    UartWriteN("\r\n", 2);
    UartWrite("!!! ASSERTION FAILED !!!\r\n");

    if (file_name) {
        UartWrite("  file:     ");
        UartWrite(file_name);
        UartWrite(":");
        UartWriteInt(line_number);
        UartWriteN("\r\n", 2);
    }

    if (function_name) {
        UartWrite("  function: ");
        UartWrite(function_name);
        UartWriteN("\r\n", 2);
    }

    if (message && *message) {
        UartWrite("  message:  ");
        // Simple varargs formatting: just print the format string as-is for
        // the demo.  A production project would use pw_string::Format here.
        UartWrite(message);
        UartWriteN("\r\n", 2);
    }

    UartWrite("  Halting MCU.\r\n");

    // Turn both LEDs on as a visual indicator of the fault
    Board::LedGreen::set();
    Board::LedRed::set();

    // Disable all interrupts and spin – safe halt for bare metal
    __disable_irq();
    while (true) {
        __NOP();
    }
}
