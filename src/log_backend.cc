/**
 * pw_sys_io backend for the STM32F429I-DISCO – routes all Pigweed I/O through
 * modm's buffered UART on the ST-Link virtual COM port.
 *
 * Pigweed's pw_log_basic calls pw::sys_io::WriteLine() to emit each formatted
 * log line.  This file provides the three primitive functions that the
 * pw_sys_io facade requires:
 *
 *   pw::Status pw::sys_io::WriteByte(std::byte)
 *   pw::Status pw::sys_io::ReadByte(std::byte*)
 *   pw::Status pw::sys_io::TryReadByte(std::byte*)
 *
 * The facade itself builds ReadBytes/WriteBytes on top of those, and
 * log_basic.cc calls sys_io::WriteLine which is implemented here because the
 * facade only provides ReadBytes/WriteBytes but not WriteLine.
 */

#include "pw_sys_io/sys_io.h"

#include <modm/board.hpp>

namespace pw::sys_io {

// ── Output ────────────────────────────────────────────────────────────────────

Status WriteByte(std::byte b) {
    Board::stlink::Uart::write(static_cast<uint8_t>(b));
    return OkStatus();
}

StatusWithSize WriteLine(std::string_view s) {
    for (char c : s) {
        Board::stlink::Uart::write(static_cast<uint8_t>(c));
    }
    Board::stlink::Uart::write('\n');
    return StatusWithSize(s.size() + 1);
}

// ── Input (not used; stubs satisfy the linker) ────────────────────────────────

Status ReadByte(std::byte*) {
    return Status::Unimplemented();
}

Status TryReadByte(std::byte*) {
    return Status::Unavailable();
}

}  // namespace pw::sys_io
