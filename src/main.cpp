/**
 * STM32F429I-DISCO Demo (Clang/Pigweed/modm/ETL)
 *
 * Logging uses pw_log_tokenized: format strings are replaced by 32-bit tokens
 * at compile time and transmitted as $-prefixed Base64 over UART at runtime.
 *
 * To decode the live output:
 *   python -m pw_tokenizer.database create \
 *       --database tokens.csv <build>/stm32f429i_demo
 *   python -m pw_tokenizer.detokenize \
 *       --database tokens.csv serial --device /dev/ttyACM0 --baud 115200
 */

#include <modm/board.hpp>

// ── Pigweed ──────────────────────────────────────────────────────────────────
// Override the default tokenized log format (■msg♦…■module♦…■file♦…) with a
// compact "[MODULE] message" string.  Must be defined before pw_log/log.h pulls
// in pw_log_tokenized/config.h which guards this macro with #ifndef.
#define PW_LOG_TOKENIZED_FORMAT_STRING(module, message) "[" module "] " message
#include "pw_log/log.h"
#include "pw_assert/check.h"
#include "pw_build_info/build_id.h"
#include "pw_status/status.h"
#include "pw_span/span.h"
#include "git_info.h"

// ── ETL ───────────────────────────────────────────────────────────────────────
#include <etl/vector.h>
#include <etl/string.h>
#include <etl/numeric.h>

#undef PW_LOG_MODULE_NAME
#define PW_LOG_MODULE_NAME "DEMO"

using namespace modm::literals;

// Hilfs-Alias für den Hardware-Namespace, falls Usart1 nicht direkt im globalen Scope liegt
using namespace modm::platform;

// ─────────────────────────────────────────────────────────────────────────────
// Data types
// ─────────────────────────────────────────────────────────────────────────────

struct SensorReading {
    uint32_t timestamp_ms;
    int16_t  raw_value;
};

// ─────────────────────────────────────────────────────────────────────────────
// Processing function – uses pw_status and pw_span
// ─────────────────────────────────────────────────────────────────────────────

pw::Status ProcessBatch(pw::span<const SensorReading> batch) {
    if (batch.empty()) {
        PW_LOG_WARN("ProcessBatch called with an empty span");
        return pw::Status::InvalidArgument();
    }

    int32_t sum = 0;
    for (const SensorReading& r : batch) {
        sum += r.raw_value;
        // Fix: %-6lu -> %-6u (uint32_t ist unter Clang/ARM 'unsigned int')
        PW_LOG_DEBUG("  t=%-6u  raw=%d", (unsigned int)r.timestamp_ms, r.raw_value);
    }

    const int32_t mean = sum / static_cast<int32_t>(batch.size());

    // Pigweed kümmert sich um alles: Formatierung, Typ-Sicherheit und Logging.
    PW_LOG_INFO("batch mean=%d  n=%u", (int)mean, (unsigned int)batch.size());
    return pw::OkStatus();
}

// ─────────────────────────────────────────────────────────────────────────────
// main
// ─────────────────────────────────────────────────────────────────────────────

int main() {
    // Initialisiert Clocks (180MHz), FPU und LEDs
    Board::initialize();

    // UART1 Konfiguration (Namespace modm::platform via using oben)
    // Nutze den Alias, den das Discovery-Board Profil bereitstellt:
    // UART-Initialisierung über den Board-spezifischen Pfad
    Board::stlink::Uart::connect<GpioA9::Tx, GpioA10::Rx>();
    Board::stlink::Uart::initialize<Board::SystemClock, 115200_Bd>();

    PW_LOG_INFO("=========================================");
    PW_LOG_INFO(" STM32F429I-DISCO  modm + Pigweed + ETL ");
    PW_LOG_INFO("=========================================");

    // ── pw_build_info: log the GNU build ID (SHA1, 20 bytes) ────────────────
    // The build ID is embedded by the linker (-Wl,--build-id=sha1) into the
    // .note.gnu.build-id ELF section.  pw::build_info::BuildId() reads it at
    // runtime via the gnu_build_id_begin linker symbol.  Each firmware image
    // gets a unique ID, making it easy to match a running binary to its ELF.
    {
        const pw::span<const std::byte> bid = pw::build_info::BuildId();
        // Format as a lowercase hex string without heap or printf("%x").
        char hex[pw::build_info::kMaxBuildIdSizeBytes * 2 + 1] = {};
        static constexpr char kNibble[] = "0123456789abcdef";
        for (size_t i = 0; i < bid.size(); ++i) {
            const auto b  = static_cast<uint8_t>(bid[i]);
            hex[i * 2]     = kNibble[b >> 4];
            hex[i * 2 + 1] = kNibble[b & 0x0F];
        }
        PW_LOG_INFO("Build ID: %s", hex);
    }

    // ── Git metadata and build timestamp ────────────────────────────────────
    // kCommit / kBranch / kDirty are captured at build time by cmake/GenGitInfo.cmake.
    // __DATE__ / __TIME__ are compiler built-ins expanded when main.cpp is compiled.
    PW_LOG_INFO("Git:   %s%s @ %s",
                git_info::kCommit,
                git_info::kDirty ? "-dirty" : "",
                git_info::kBranch);
    PW_LOG_INFO("Built: %s %s", __DATE__, __TIME__);

    // Fix: %lu -> %u für Board-Frequenz
    PW_LOG_INFO("System clock: %u Hz", (unsigned int)Board::SystemClock::Frequency);

    // ETL static vector: zero heap usage
    etl::vector<SensorReading, 16> readings;
    PW_LOG_INFO("ETL reading buffer capacity: %u", (unsigned int)readings.capacity());

    uint32_t tick_ms     = 0;
    uint32_t batch_count = 0;

    while (true) {
        // ── Heartbeat ───────────────────────────────────────────────────────
        Board::LedGreen::toggle();
        modm::delay(500ms);
        tick_ms += 500;

        // ── Simulate a sensor reading ────────────────────────────────────────
        const int16_t value = static_cast<int16_t>((tick_ms / 500u) % 100u) - 50;

        if (!readings.full()) {
            readings.push_back({tick_ms, value});
        }

        // ── Process a full batch ─────────────────────────────────────────────
        if (readings.full()) {
            ++batch_count;
            // Fix: %lu -> %u für Batch-Counter und Zeit
            PW_LOG_INFO("--- Batch #%u (t=%u ms) ---", (unsigned int)batch_count, (unsigned int)tick_ms);

            const pw::Status status = ProcessBatch(
                pw::span<const SensorReading>(readings.data(), readings.size()));

            PW_CHECK_OK(status, "ProcessBatch failed");

            readings.clear();

            // Rote LED kurz an als Verarbeitungs-Bestätigung
            Board::LedRed::set();
            modm::delay(100ms);
            Board::LedRed::reset();
        }
    }
}
