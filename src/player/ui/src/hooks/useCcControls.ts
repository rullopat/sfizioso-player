import { useInstrumentControls } from "@shared/hooks/useInstrumentControls";
import { FN_GET_INSTRUMENT_PRESENTATION, FN_SET_CC, EVT_CC_VALUES, EVT_SFZ_LOADED } from "../paramIds";

export function useCcControls() {
  return useInstrumentControls({ getPresentation: FN_GET_INSTRUMENT_PRESENTATION,
    setCc: FN_SET_CC, ccValues: EVT_CC_VALUES, instrumentLoaded: EVT_SFZ_LOADED });
}
