// Mirrors src/player_core/PlayerEngine.h PlayerEngineParamIds and the native
// function / event names registered in PlayerEditor. Keep these strings
// byte-identical to the C++ side — relays register on the same IDs and any
// mismatch silently breaks the binding.

// --- APVTS params ----------------------------------------------------------
export const PARAM_GAIN_DB = "gainDb";
export const PARAM_POLYPHONY = "polyphony";
export const PARAM_MPE_MODE = "mpeMode";

// SMPL-86 engine / quality.
export const PARAM_OVERSAMPLING = "oversampling";
export const PARAM_PRELOAD_SIZE = "preloadSize";
export const PARAM_SQ_LIVE = "sampleQualityLive";
export const PARAM_SQ_FREEWHEEL = "sampleQualityFreewheel";
export const PARAM_FREEWHEEL = "freewheel";

// SMPL-87 tuning.
export const PARAM_SCALA_ROOT_KEY = "scalaRootKey";
export const PARAM_TUNING_FREQ = "tuningFrequency";
export const PARAM_STRETCH = "stretchTuning";

// SMPL-90 MPE settings.
export const PARAM_MPE_MASTER_BEND = "mpeMasterBend";
export const PARAM_MPE_PERNOTE_BEND = "mpePerNoteBend";
export const PARAM_MPE_IGNORE_MASTER = "mpeIgnoreMasterRpn";
export const PARAM_MPE_IGNORE_PERNOTE = "mpeIgnorePerNoteRpn";

// --- native functions ------------------------------------------------------
export const FN_LOAD_SFZ = "loadSfz";
export const FN_LOAD_SFZ_PATH = "loadSfzPath";   // SMPL-89
export const FN_GET_STATUS = "getStatus";
export const FN_GET_APP_INFO = "getAppInfo";
export const FN_GET_RECENT = "getRecentFiles";   // SMPL-89
export const FN_GET_CC_CONTROLS = "getCcControls"; // SMPL-85
export const FN_SET_CC = "setCc";                // SMPL-85
export const FN_GET_KEY_LABELS = "getKeyLabels"; // SMPL-88
export const FN_NOTE_ON = "noteOn";              // SMPL-88
export const FN_NOTE_OFF = "noteOff";            // SMPL-88
export const FN_LOAD_SCALA = "loadScala";        // SMPL-87
export const FN_RESET_SCALA = "resetScala";      // SMPL-87
export const FN_GET_TUNING = "getTuning";        // SMPL-87

// --- events (C++ -> UI) ----------------------------------------------------
export const EVT_VOICES = "voices";
export const EVT_METER = "meter";
export const EVT_CC_VALUES = "ccValues";         // SMPL-85
export const EVT_SFZ_LOADED = "sfzLoaded";       // SMPL-85 / 89
export const EVT_SFZ_RELOADED = "sfzReloaded";   // SMPL-89
export const EVT_NOTES = "notes";                // SMPL-88
export const EVT_TUNING = "tuning";              // SMPL-87
export const EVT_MPE = "mpe";                    // SMPL-90 effective-bend readouts
