/* =============================================================================
 * F.E.A.R. VR — Gemeinsamer IPC-Vertrag (ANWEISUNG.md §6)
 *
 * Dieser Header MUSS bit-identisch in x86 (Proxy/GameClient) und x64 (Host)
 * kompilieren. Nur fest breite POD-Typen. Keine STL, keine size_t, keine
 * Pointer/Handles in nativer Breite, keine C++-Exceptions.
 *
 * Layout-Regeln:
 *  - 8-Byte-Felder zuerst, dann 4-Byte, dann 2/1-Byte; explizites Padding.
 *  - Jeder gemappte Bereich beginnt mit Magic + Version + Strukturgröße.
 *  - Posen als Position + normalisiertes Quaternion; FOV als vier Winkel.
 *  - Shared-Texture-Handles als uint64_t serialisiert; Empfänger validiert.
 * ========================================================================== */
#ifndef FEARVR_COMMON_PROTOCOL_H
#define FEARVR_COMMON_PROTOCOL_H

#include <stdint.h>

/* ---- static_assert für C und C++ ---------------------------------------- */
#if defined(__cplusplus)
  #define FEARVR_STATIC_ASSERT(cond, msg) static_assert((cond), msg)
#elif defined(_MSC_VER) || (defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L)
  #define FEARVR_STATIC_ASSERT(cond, msg) _Static_assert((cond), msg)
#else
  #define FEARVR_STATIC_ASSERT(cond, msg) \
    typedef char fearvr_static_assert_[(cond) ? 1 : -1]
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Protokoll-Identität ------------------------------------------------- */
/* 'F','R','V','R' als Little-Endian-uint32 => 0x52565246 */
#define FEARVR_PROTOCOL_MAGIC   0x52565246u
#define FEARVR_PROTOCOL_VERSION 6u

/* Spielkamera und OpenXR-Projektionslayer verwenden dieselbe FOV-Skalierung.
 * Null steht fuer den kompatiblen Standard, falls eine alte Gegenstelle das
 * zuvor reservierte Feld nicht setzt. */
enum {
  FEARVR_FOV_SCALE_DEFAULT_PERCENT = 100u,
  FEARVR_FOV_SCALE_MIN_PERCENT = 100u,
  FEARVR_FOV_SCALE_MAX_PERCENT = 130u
};

/* ---- Augen & Ringpuffer -------------------------------------------------- */
enum {
  FEARVR_EYE_LEFT  = 0,
  FEARVR_EYE_RIGHT = 1,
  FEARVR_EYE_COUNT = 2
};

/* Mind. 2, besser 3 Slots pro Auge (§6). */
#define FEARVR_SLOTS_PER_EYE 3u

/* ---- OpenXR-Controller --------------------------------------------------- */
enum {
  FEARVR_HAND_LEFT  = 0,
  FEARVR_HAND_RIGHT = 1,
  FEARVR_HAND_COUNT = 2
};

enum {
  FEARVR_HAND_MASK_LEFT  = 0x00000001u,
  FEARVR_HAND_MASK_RIGHT = 0x00000002u
};

/* Physische, noch nicht auf Spielbefehle abgebildete Tasten. */
enum {
  FEARVR_IB_LEFT_PRIMARY   = 0x00000001u,
  FEARVR_IB_LEFT_SECONDARY = 0x00000002u,
  FEARVR_IB_LEFT_MENU      = 0x00000004u,
  FEARVR_IB_LEFT_STICK     = 0x00000008u,
  FEARVR_IB_RIGHT_PRIMARY  = 0x00000010u,
  FEARVR_IB_RIGHT_SECONDARY = 0x00000020u,
  FEARVR_IB_RIGHT_MENU     = 0x00000040u,
  FEARVR_IB_RIGHT_STICK    = 0x00000080u
};

enum {
  FEARVR_IF_VALID   = 0x00000001u,
  FEARVR_IF_FOCUSED = 0x00000002u
};

enum {
  FEARVR_HF_VALID = 0x00000001u
};

/* Spielkamera-Metadaten fuer die Lokomotions-Nachprojektion. */
enum {
  FEARVR_GCF_VALID = 0x00000001u
};

/* ---- Slot-/Frame-Zustände ------------------------------------------------ */
enum {
  FEARVR_SLOT_EMPTY     = 0u, /* frei, vom Host konsumiert                    */
  FEARVR_SLOT_WRITING   = 1u, /* Game rendert/kopiert gerade hinein          */
  FEARVR_SLOT_READY     = 2u, /* GPU-Arbeit fertig (Query signalisiert)      */
  FEARVR_SLOT_CONSUMING = 3u  /* Host liest gerade                           */
};

/* Session-Flags in FearVrRenderRequest.flags */
enum {
  FEARVR_RF_VALID          = 0x00000001u, /* Auftrag gültig                  */
  FEARVR_RF_TRANSLATION_ON = 0x00000002u, /* 6DoF-Translation aktiv          */
  FEARVR_RF_FLATSCREEN     = 0x00000004u, /* Game soll Flat rendern          */
  FEARVR_RF_FLOOR_SPACE    = 0x00000008u  /* Pose relativ zum Boden          */
};

/* Verbindungs-/Diagnoseflags in FearVrSharedHeader.bridgeFlags. */
enum {
  FEARVR_BF_HOST_READY       = 0x00000001u,
  FEARVR_BF_GAME_READY       = 0x00000002u,
  FEARVR_BF_ADAPTER_MATCH    = 0x00000004u,
  FEARVR_BF_SHARED_SUPPORTED = 0x00000008u,
  FEARVR_BF_DEVICE_LOST      = 0x00000010u,
  FEARVR_BF_PROTOCOL_ERROR   = 0x00000020u,
  FEARVR_BF_CPU_FALLBACK     = 0x00000040u,
  FEARVR_BF_STEREO_ACTIVE    = 0x00000080u
};

/* ---- Geometrie ----------------------------------------------------------- */
/* Position in Metern; Quaternion normalisiert (xyzw). */
typedef struct FearVrPose {
  float px, py, pz;        /* Position                                       */
  float qx, qy, qz, qw;    /* Rotation (Quaternion)                          */
} FearVrPose;
FEARVR_STATIC_ASSERT(sizeof(FearVrPose) == 28, "FearVrPose size");

/* FOV als vier Winkel in Radiant (OpenXR-Konvention, ggf. negativ). */
typedef struct FearVrFov {
  float angleLeft;
  float angleRight;
  float angleUp;
  float angleDown;
} FearVrFov;
FEARVR_STATIC_ASSERT(sizeof(FearVrFov) == 16, "FearVrFov size");

typedef struct FearVrEyeView {
  FearVrPose pose;
  FearVrFov  fov;
} FearVrEyeView;
FEARVR_STATIC_ASSERT(sizeof(FearVrEyeView) == 44, "FearVrEyeView size");

/* Weltpose der Basis-Spielkamera (ohne HMD-/Augenversatz).
 * Die Position ist in Metern, die Rotation in LithTech-Weltachsen. */
typedef struct FearVrGameCameraSample {
  uint64_t frameId;
  FearVrPose pose;
  uint32_t flags;                   /* FEARVR_GCF_*                          */
  uint32_t reserved0[2];
} FearVrGameCameraSample;
FEARVR_STATIC_ASSERT(sizeof(FearVrGameCameraSample) == 48,
                     "FearVrGameCameraSample size (48)");

/* ---- Renderauftrag (Host -> Game) ---------------------------------------- */
typedef struct FearVrRenderRequest {
  uint64_t frameId;                 /* monoton steigend                      */
  uint64_t predictedDisplayTimeNs;  /* vorhergesagte Anzeigezeit             */
  FearVrEyeView eye[FEARVR_EYE_COUNT];
  uint32_t recenterGeneration;      /* +1 bei jedem Recenter                 */
  uint32_t flags;                   /* FEARVR_RF_*                           */
} FearVrRenderRequest;
FEARVR_STATIC_ASSERT(sizeof(FearVrRenderRequest) == 8 + 8 + 88 + 4 + 4,
                     "FearVrRenderRequest size (112)");

/* ---- Controllerzustand (Host -> Game) ----------------------------------- */
typedef struct FearVrInputState {
  uint64_t sampleId;                /* monoton pro Veröffentlichung          */
  uint64_t predictedDisplayTimeNs;  /* OpenXR-Zeit der Abtastung             */
  float moveX, moveY;               /* linker Stick/Trackpad, [-1,+1]        */
  float turnX, turnY;               /* rechter Stick/Trackpad, [-1,+1]       */
  float trigger[FEARVR_HAND_COUNT]; /* analog, [0,1]                         */
  float squeeze[FEARVR_HAND_COUNT]; /* analog, [0,1]                         */
  uint32_t buttons;                 /* FEARVR_IB_*                           */
  uint32_t activeHands;             /* FEARVR_HAND_MASK_*                    */
  uint32_t flags;                   /* FEARVR_IF_*                           */
  uint32_t aimPoseValidHands;       /* gültige handAimPose, FEARVR_HAND_*    */
  uint32_t gripPoseValidHands;      /* gültige handGripPose, FEARVR_HAND_*   */
  FearVrPose handAimPose[FEARVR_HAND_COUNT]; /* OpenXR LOCAL aim/pose          */
  FearVrPose handGripPose[FEARVR_HAND_COUNT]; /* OpenXR LOCAL grip/pose        */
  uint32_t reserved0[2];
} FearVrInputState;
FEARVR_STATIC_ASSERT(sizeof(FearVrInputState) == 192,
                     "FearVrInputState size (192)");

/* ---- Haptikanforderung (Game -> Host) ----------------------------------- */
typedef struct FearVrHapticRequest {
  uint64_t requestId;       /* monoton; 0 bedeutet keine Anforderung         */
  uint64_t durationNs;      /* OpenXR-Dauer in Nanosekunden                   */
  float amplitude;          /* [0,1]                                          */
  float frequency;          /* Hz oder XR_FREQUENCY_UNSPECIFIED              */
  uint32_t handMask;        /* FEARVR_HAND_MASK_*                             */
  uint32_t flags;           /* FEARVR_HF_*                                    */
} FearVrHapticRequest;
FEARVR_STATIC_ASSERT(sizeof(FearVrHapticRequest) == 32,
                     "FearVrHapticRequest size (32)");

/* ---- Slot-Deskriptor (ein Ring-Slot, ein Auge) --------------------------- */
typedef struct FearVrSlot {
  uint64_t sharedHandle;   /* D3D9-Shared-Texture-Handle (validieren!)       */
  uint64_t frameId;        /* zu welchem Frame dieser Inhalt gehört          */
  uint32_t state;          /* FEARVR_SLOT_*                                  */
  uint32_t width;
  uint32_t height;
  uint32_t format;         /* protokoll-eigener Formatcode (siehe unten)     */
  uint64_t generation;     /* monoton, gegen recycelte Slots                 */
  FearVrGameCameraSample camera; /* Basis-Spielkamera dieses Bildes           */
} FearVrSlot;
FEARVR_STATIC_ASSERT(
  sizeof(FearVrSlot) == 8 + 8 + 4 + 4 + 4 + 4 + 8
    + sizeof(FearVrGameCameraSample),
  "FearVrSlot size (88)");

/* Protokoll-eigene Formatcodes (keine D3D-Enums über die Grenze schicken). */
enum {
  FEARVR_FMT_UNKNOWN    = 0u,
  FEARVR_FMT_B8G8R8A8   = 1u,
  FEARVR_FMT_R8G8B8A8   = 2u,
  FEARVR_FMT_R10G10B10A2 = 3u
};

/* ---- Shared-Header (Anfang des File-Mappings) ---------------------------- */
typedef struct FearVrSharedHeader {
  uint32_t magic;          /* == FEARVR_PROTOCOL_MAGIC                       */
  uint32_t version;        /* == FEARVR_PROTOCOL_VERSION                     */
  uint32_t headerSize;     /* == sizeof(FearVrSharedHeader)                  */
  uint32_t slotStructSize; /* == sizeof(FearVrSlot)                          */
  uint32_t slotsPerEye;    /* == FEARVR_SLOTS_PER_EYE                        */
  uint32_t panelRecenterGeneration; /* Game -> Host: 2D-Panel neu verankern   */
  uint64_t hostHeartbeat;  /* vom Host hochgezählt                          */
  uint64_t gameHeartbeat;  /* vom Game hochgezählt                          */
  uint64_t requestSequence; /* Seqlock: ungerade=Schreibvorgang, gerade=stabil */
  uint64_t inputSequence;  /* Host -> Game, Seqlock für input                */
  uint64_t hapticSequence; /* Game -> Host, Seqlock für haptic              */
  uint64_t cameraSequence; /* Game -> Host, Seqlock fuer latestCamera        */
  uint64_t hostAdapterLuid; /* HighPart: obere 32 Bit, LowPart: untere 32 Bit */
  uint64_t gameAdapterLuid; /* HighPart: obere 32 Bit, LowPart: untere 32 Bit */
  uint32_t hostProcessId;
  uint32_t gameProcessId;
  uint32_t bridgeFlags;     /* FEARVR_BF_*                                   */
  uint32_t fovScalePercent; /* Game -> Host; 0 = kompatibler Standard 100 %  */
  FearVrRenderRequest request; /* neuester vollständig veröffentlichter Auftrag */
  FearVrInputState input;   /* neuester vollständiger Controllerzustand      */
  FearVrHapticRequest haptic; /* neueste Haptikanforderung                   */
  FearVrGameCameraSample latestCamera; /* neueste gerenderte Spielkamera     */
  /* Slots: [eye][slot] direkt nach dem Header im Mapping. */
  FearVrSlot slot[FEARVR_EYE_COUNT][FEARVR_SLOTS_PER_EYE];
} FearVrSharedHeader;
FEARVR_STATIC_ASSERT(
  sizeof(FearVrSharedHeader) ==
    24 /* 6x uint32 */ + 64 /* 8x uint64 */ + 16 /* 4x uint32 */
    + sizeof(FearVrRenderRequest)
    + sizeof(FearVrInputState)
    + sizeof(FearVrHapticRequest)
    + sizeof(FearVrGameCameraSample)
    + (uint32_t)(FEARVR_EYE_COUNT * FEARVR_SLOTS_PER_EYE) * sizeof(FearVrSlot),
  "FearVrSharedHeader size");

/* ---- C-ABI der d3d9.dll-Bridge (§5.2) ------------------------------------
 * Vom GameClient dynamisch via GetModuleHandleW(L"d3d9.dll") + GetProcAddress
 * aufgelöst. Fehlen sie, bleibt der Original-Renderpfad unverändert.
 * Nur POD über die Grenze. */
typedef struct FearVrRenderRequestOut {
  FearVrRenderRequest request;
  uint32_t hasRequest;   /* 0/1                                             */
  uint32_t reserved0;
} FearVrRenderRequestOut;
FEARVR_STATIC_ASSERT(sizeof(FearVrRenderRequestOut)
                     == sizeof(FearVrRenderRequest) + 8,
                     "FearVrRenderRequestOut size");

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* FEARVR_COMMON_PROTOCOL_H */
