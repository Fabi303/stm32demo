// Build-time metadata embedded in a dedicated ELF section (.build_metadata).
//
// The struct is placed in FLASH and is readable with:
//   arm-none-eabi-objcopy --only-section=.build_metadata -O binary <elf> meta.bin
// or via the host script:
//   python tools/read_build_meta.py build/debug/stm32f429i_demo
//
// The CRC-32 field is computed at compile time by a constexpr lambda using the
// IEEE 802.3 polynomial — the same algorithm as Python's zlib.crc32() — so
// the host script can verify integrity without any external dependencies.

#include <array>
#include <cstdint>

#include "git_info.h"

// ── Constexpr CRC-32 (IEEE 802.3 / gzip polynomial) ─────────────────────────
// Identical to Python's zlib.crc32(): init=0xFFFFFFFF, poly=0xEDB88320, finalXOR=0xFFFFFFFF.
namespace {

constexpr uint32_t crc32_byte(uint32_t crc, uint8_t b) noexcept {
    crc ^= b;
    for (int i = 0; i < 8; ++i)
        crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
    return crc;
}

constexpr uint32_t crc32_str(const char* s, uint32_t crc) noexcept {
    for (; *s; ++s)
        crc = crc32_byte(crc, static_cast<uint8_t>(*s));
    return crc;
}

// Copy a C-string into a fixed-size array; unused trailing bytes are zero.
template<std::size_t N>
constexpr std::array<char, N> str_arr(const char* s) noexcept {
    std::array<char, N> a{};
    for (std::size_t i = 0; i < N - 1 && s[i]; ++i)
        a[i] = s[i];
    return a;
}

// CRC-32 over the metadata content fields (without padding/null bytes).
// Python equivalent:
//   payload = commit + bytes([dirty]) + branch + date + time   (all without nulls)
//   crc = zlib.crc32(payload) & 0xFFFFFFFF
constexpr uint32_t kMetaCrc = [] {
    uint32_t c = 0xFFFF'FFFFu;
    c = crc32_str(git_info::kCommit, c);
    c = crc32_byte(c, git_info::kDirty ? uint8_t{1} : uint8_t{0});
    c = crc32_str(git_info::kBranch, c);
    c = crc32_str(__DATE__, c);
    c = crc32_str(__TIME__, c);
    return c ^ 0xFFFF'FFFFu;
}();

}  // namespace

// ── Packed struct in .build_metadata ELF section ─────────────────────────────
// Binary layout (71 bytes, little-endian, no padding between fields):
//
//   Offset  Size  Field
//   ------  ----  -----
//    0       4    magic    "META"  (no null terminator; parser sentinel)
//    4       9    commit   8-char git hash + '\0'
//   13       1    dirty    0 = clean, 1 = working tree modified at build time
//   14      32    branch   branch name (up to 31 chars) + '\0'
//   46      12    date     __DATE__  "Mmm DD YYYY" + '\0'
//   58       9    time     __TIME__  "HH:MM:SS"   + '\0'
//   67       4    crc32    CRC-32 over commit+dirty+branch+date+time (LE uint32)
//                          Verify in Python: zlib.crc32(payload) & 0xFFFFFFFF

struct __attribute__((packed)) BuildMetadata {
    std::array<char, 4>  magic;
    std::array<char, 9>  commit;
    uint8_t              dirty;
    std::array<char, 32> branch;
    std::array<char, 12> date;
    std::array<char, 9>  time;
    uint32_t             crc32;
};

static_assert(sizeof(BuildMetadata) == 71,
              "BuildMetadata layout changed – update tools/read_build_meta.py");

__attribute__((section(".build_metadata"), used))
constexpr BuildMetadata kBuildMeta = {
    /* magic  */ {'M', 'E', 'T', 'A'},
    /* commit */ str_arr<9>(git_info::kCommit),
    /* dirty  */ git_info::kDirty ? uint8_t{1} : uint8_t{0},
    /* branch */ str_arr<32>(git_info::kBranch),
    /* date   */ str_arr<12>(__DATE__),
    /* time   */ str_arr<9>(__TIME__),
    /* crc32  */ kMetaCrc,
};
