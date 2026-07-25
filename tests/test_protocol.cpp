// test_protocol — Protokollgrößen/-Offsets + Magic/Version-Ablehnung (§14).
// Muss in x86 UND x64 bestehen: die Größen sind in beiden Architekturen gleich.
#include <cstdio>
#include <cstddef>
#include <cstring>

#include "protocol.h"
#include "protocol_utils.h"

// --- Feste Größen & Offsets: kompilezeit geprüft (identisch x86/x64) ---------
static_assert(sizeof(FearVrPose) == 28, "FearVrPose");
static_assert(sizeof(FearVrFov) == 16, "FearVrFov");
static_assert(sizeof(FearVrEyeView) == 44, "FearVrEyeView");
static_assert(sizeof(FearVrRenderRequest) == 112, "FearVrRenderRequest");
static_assert(sizeof(FearVrInputState) == 192, "FearVrInputState");
static_assert(sizeof(FearVrHapticRequest) == 32, "FearVrHapticRequest");
static_assert(sizeof(FearVrSlot) == 40, "FearVrSlot");
static_assert(offsetof(FearVrSharedHeader, magic) == 0, "off magic");
static_assert(offsetof(FearVrSharedHeader, version) == 4, "off version");
static_assert(offsetof(FearVrSharedHeader, headerSize) == 8, "off headerSize");
static_assert(offsetof(FearVrSharedHeader, hostHeartbeat) == 24, "off hostHb");
static_assert(offsetof(FearVrSharedHeader, gameHeartbeat) == 32, "off gameHb");
static_assert(offsetof(FearVrSharedHeader, requestSequence) == 40,
              "off requestSequence");
static_assert(offsetof(FearVrSharedHeader, inputSequence) == 48,
              "off inputSequence");
static_assert(offsetof(FearVrSharedHeader, hapticSequence) == 56,
              "off hapticSequence");
static_assert(offsetof(FearVrSharedHeader, hostAdapterLuid) == 64,
              "off hostAdapterLuid");
static_assert(offsetof(FearVrSharedHeader, gameAdapterLuid) == 72,
              "off gameAdapterLuid");
static_assert(offsetof(FearVrSharedHeader, request) == 96, "off request");
static_assert(offsetof(FearVrSharedHeader, input) == 208, "off input");
static_assert(offsetof(FearVrSharedHeader, haptic) == 400, "off haptic");
static_assert(offsetof(FearVrSharedHeader, slot) == 432, "off slot");
static_assert(sizeof(FearVrSharedHeader) == 672, "shared header size");

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
    return fearvr::IsProtocolHeaderValid(h);
}

int main(void) {
    // --- Gültiger Header wird akzeptiert ---
    FearVrSharedHeader h;
    fearvr::InitializeProtocolHeader(h);
    CHECK(AcceptHeader(h));

    // --- Ungültige Varianten werden abgelehnt (Laufzeitwerte) ---
    { FearVrSharedHeader b = h; b.magic ^= 0xFFu;      CHECK(!AcceptHeader(b)); }
    { FearVrSharedHeader b = h; b.version += 1;         CHECK(!AcceptHeader(b)); }
    { FearVrSharedHeader b = h; b.headerSize += 1;      CHECK(!AcceptHeader(b)); }
    { FearVrSharedHeader b = h; b.slotStructSize -= 1;  CHECK(!AcceptHeader(b)); }
    { FearVrSharedHeader b = h; b.slotsPerEye = 0;      CHECK(!AcceptHeader(b)); }

    const std::uint64_t packed =
        fearvr::PackLuid(0x81234567U, 0x89ABCDEFU);
    CHECK(fearvr::LuidHigh(packed) == 0x81234567U);
    CHECK(fearvr::LuidLow(packed) == 0x89ABCDEFU);

    h.slot[FEARVR_EYE_LEFT][0].state = FEARVR_SLOT_READY;
    h.slot[FEARVR_EYE_RIGHT][0].state = FEARVR_SLOT_READY;
    h.slot[FEARVR_EYE_LEFT][0].frameId = 7;
    h.slot[FEARVR_EYE_RIGHT][0].frameId = 7;
    h.slot[FEARVR_EYE_LEFT][0].generation = 2;
    h.slot[FEARVR_EYE_RIGHT][0].generation = 2;
    auto pair = fearvr::FindNewestReadyPair(h);
    CHECK(pair.found);
    CHECK(pair.slotIndex == 0);
    CHECK(pair.frameId == 7);

    h.slot[FEARVR_EYE_LEFT][1] = h.slot[FEARVR_EYE_LEFT][0];
    h.slot[FEARVR_EYE_RIGHT][1] = h.slot[FEARVR_EYE_RIGHT][0];
    h.slot[FEARVR_EYE_LEFT][1].frameId = 9;
    h.slot[FEARVR_EYE_RIGHT][1].frameId = 9;
    h.slot[FEARVR_EYE_LEFT][1].generation = 4;
    h.slot[FEARVR_EYE_RIGHT][1].generation = 4;
    pair = fearvr::FindNewestReadyPair(h);
    CHECK(pair.found);
    CHECK(pair.slotIndex == 1);
    CHECK(pair.frameId == 9);

    h.slot[FEARVR_EYE_RIGHT][1].frameId = 8;
    pair = fearvr::FindNewestReadyPair(h);
    CHECK(pair.found);
    CHECK(pair.slotIndex == 0);

    if (g_failed == 0) {
        std::printf("test_protocol: OK (ptr=%zu bit)\n", sizeof(void*) * 8);
        return 0;
    }
    std::printf("test_protocol: %d Pruefung(en) fehlgeschlagen\n", g_failed);
    return 1;
}
