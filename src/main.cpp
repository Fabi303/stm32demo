/**
 * STM32F429I-DISCO Demo (Clang/Pigweed/modm/ETL)
 */

#include <modm/board.hpp>

// ── Pigweed ──────────────────────────────────────────────────────────────────
#include "pw_log/log.h"
#include "pw_assert/check.h"
#include "pw_status/status.h"
#include "pw_span/span.h"

// ── ETL ───────────────────────────────────────────────────────────────────────
#include <etl/vector.h>
#include <etl/string.h>
#include <etl/numeric.h>

// Fix für Clang: Vorherige Definition von Pigweed aufheben, um Warnung zu vermeiden
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
