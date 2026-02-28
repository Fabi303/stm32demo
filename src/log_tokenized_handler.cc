/**
 * pw_log_tokenized handler for the STM32F429I-DISCO.
 *
 * Pigweed's pw_log_tokenized backend calls this function for every log
 * statement.  At compile-time the format string is replaced by a 32-bit
 * token (hash) stored only in the ELF; at runtime only the token plus
 * varint-encoded arguments are transmitted.
 *
 * Wire format (Pigweed standard, compatible with pw_tokenizer.detokenize):
 *
 *   '$' <base64(token ++ encoded_args)> '\n'
 *
 * To decode on the host:
 *
 *   # 1. Extract token database from the ELF (run once after each build):
 *   python -m pw_tokenizer.database create \
 *       --database tokens.csv <build>/stm32f429i_demo
 *
 *   # 2. Live decode from serial port:
 *   python -m pw_tokenizer.detokenize \
 *       --database tokens.csv serial --device /dev/ttyACM0 --baud 115200
 */

#include "pw_log_tokenized/handler.h"

#include <modm/board.hpp>

namespace {

// Emit a single byte to the ST-Link virtual COM port.
inline void Emit(uint8_t b) {
    Board::stlink::Uart::write(b);
}

// Emit the Base64 character for a 6-bit index (RFC 4648 alphabet).
inline void EmitBase64(uint8_t idx) {
    static constexpr char kTable[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    Emit(static_cast<uint8_t>(kTable[idx & 0x3F]));
}

}  // namespace

// Called by pw_log_tokenized for every log statement.
//
// |metadata|  – packed level / line / flags / module token
//               (decode with pw::log_tokenized::Metadata if needed)
// |data|      – binary payload: 4-byte little-endian token followed by
//               varint-encoded printf arguments
// |size_bytes| – byte length of |data|
extern "C" void pw_log_tokenized_HandleLog(uint32_t /*metadata*/,
                                           const uint8_t data[],
                                           size_t size_bytes) {
    // '$' marks the start of a Pigweed tokenized message.
    Emit('$');

    // Encode |data| as standard Base64, 3 input bytes → 4 output chars.
    size_t i = 0;
    for (; i + 2 < size_bytes; i += 3) {
        const uint32_t g = (static_cast<uint32_t>(data[i])     << 16) |
                           (static_cast<uint32_t>(data[i + 1]) <<  8) |
                            static_cast<uint32_t>(data[i + 2]);
        EmitBase64(g >> 18);
        EmitBase64(g >> 12);
        EmitBase64(g >>  6);
        EmitBase64(g);
    }

    // Remaining 1 or 2 bytes, padded with '=' to a 4-char group.
    if (i < size_bytes) {
        const uint32_t g = (static_cast<uint32_t>(data[i]) << 16) |
                           (i + 1 < size_bytes
                                ? static_cast<uint32_t>(data[i + 1]) << 8
                                : 0u);
        EmitBase64(g >> 18);
        EmitBase64(g >> 12);
        if (i + 1 < size_bytes) {
            EmitBase64(g >> 6);
        } else {
            Emit('=');
        }
        Emit('=');
    }

    // Newline terminates the message on the wire.
    Emit('\n');
}
