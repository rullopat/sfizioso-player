// Mirrors src/player_core/PlayerEngine.h PlayerEngineParamIds.
// Keep these strings byte-identical to the C++ side — the editor relays
// register on the same IDs and any mismatch silently breaks the binding.
export const PARAM_GAIN_DB = "gainDb";
export const PARAM_POLYPHONY = "polyphony";
export const PARAM_MPE_MODE = "mpeMode";

// Native function names registered inline in the PlayerEditor constructor
// (juce::WebBrowserComponent::Options::withNativeFunction). Keep byte-identical
// to the C++ side.
export const FN_LOAD_SFZ = "loadSfz";
export const FN_GET_STATUS = "getStatus";
export const FN_GET_APP_INFO = "getAppInfo";

// Event IDs emitted from PlayerEditor::timerCallback / native fn responses.
export const EVT_VOICES = "voices";
export const EVT_METER = "meter";
