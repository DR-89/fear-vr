// test_protocol — Protokollgrößen/-Offsets + Magic/Version-Ablehnung (§14).
// Muss in x86 UND x64 bestehen: die Größen sind in beiden Architekturen gleich.
#include <cstdio>
#include <cstddef>
#include <cstring>

#include "protocol.h"

static int g_failed = 0;

#define CHECK(cond)                                                            \
    do {                                                                       \
        if (!(cond)) {                                                         \
            std::printf("FAIL: %s (line %d)\n", #cond, __LINE__);              \
            ++g_failed;                                                        \
        }                                                                      \
    } while (0)

// Simuliert die Empfängerprüfung: lehnt falsche Magic/Version/Größe ab.
static bool AcceptHeader(const FearVrSharedHeader& h) {
    if (h.magic != FEARVR_PROTOCOL_MAGIC) return false;
    if (h.version != FEARVR_PROTOCOL_VERSION) return false;
    if (h.headerSize != (uint32_t)sizeof(FearVrSharedHeader)) return false;
    if (h.slotStructSize != (uint32_t)sizeof(FearVrSlot)) return false;
    if (h.slotsPerEye != FEARVR_SLOTS_PER_EYE) return false;
    return true;
}

int main(void) {
    // --- Feste Größen (identisch x86/x64) ---
    CHECK(sizeof(FearVrPose) == 28);
    CHECK(sizeof(FearVrFov) == 16);
    CHECK(sizeof(FearVrEyeView) == 44);
    CHECK(sizeof(FearVrRenderRequest) == 104);
    CHECK(sizeof(FearVrSlot) == 40);

    // --- Offsets im Shared-Header ---
    CHECK(offsetof(FearVrSharedHeader, magic) == 0);
    CHECK(offsetof(FearVrSharedHeader, version) == 4);
    CHECK(offsetof(FearVrSharedHeader, headerSize) == 8);
    CHECK(offsetof(FearVrSharedHeader, hostHeartbeat) == 24);
    CHECK(offsetof(FearVrSharedHeader, gameHeartbeat) == 32);

    // --- Gültiger Header wird akzeptiert ---
    FearVrSharedHeader h;
    std::memset(&h, 0, sizeof(h));
    h.magic = FEARVR_PROTOCOL_MAGIC;
    h.version = FEARVR_PROTOCOL_VERSION;
    h.headerSize = (uint32_t)sizeof(FearVrSharedHeader);
    h.slotStructSize = (uint32_t)sizeof(FearVrSlot);
    h.slotsPerEye = FEARVR_SLOTS_PER_EYE;
    CHECK(AcceptHeader(h));

    // --- Ungültige Varianten werden abgelehnt ---
    { FearVrSharedHeader b = h; b.magic ^= 0xFFu;      CHECK(!AcceptHeader(b)); }
    { FearVrSharedHeader b = h; b.version += 1;         CHECK(!AcceptHeader(b)); }
    { FearVrSharedHeader b = h; b.headerSize += 1;      CHECK(!AcceptHeader(b)); }
    { FearVrSharedHeader b = h; b.slotStructSize -= 1;  CHECK(!AcceptHeader(b)); }
    { FearVrSharedHeader b = h; b.slotsPerEye = 0;      CHECK(!AcceptHeader(b)); }

    if (g_failed == 0) {
        std::printf("test_protocol: OK (ptr=%zu bit)\n", sizeof(void*) * 8);
        return 0;
    }
    std::printf("test_protocol: %d Prüfung(en) fehlgeschlagen\n", g_failed);
    return 1;
}
