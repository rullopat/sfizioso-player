import { useCallback, useEffect, useState, useRef } from "react";
import { callNative, onBackendEvent } from "@shared/juceBridge";
import {
  FN_SET_CC,
  FN_GET_INSTRUMENT_PRESENTATION,
  EVT_CC_VALUES,
  EVT_SFZ_LOADED,
} from "../paramIds";

// SMPL-85 — one auto-generated control per labelled CC of the loaded SFZ.
export interface CcControl {
  number: number;
  label: string;
  value: number;        // normalised 0..1
  widget: "knob" | "slider" | "toggle";
  section: string;
  sectionLabel: string;
}

interface Presentation {
  controls: CcControl[];
  instrumentName: string;
  presetName: string;
  accent: string;
  artworkUrl: string;
  diagnostics: string[];
}

/**
 * Fetches the CC-control list for the loaded instrument, tracks live values
 * (UI edits + incoming MIDI/automation reflected via the ccValues event), and
 * rebuilds whenever an SFZ (re)loads.
 */
export function useCcControls() {
  const [presentation, setPresentation] = useState<Presentation>({
    controls: [], instrumentName: "", presetName: "", accent: "", artworkUrl: "", diagnostics: [],
  });
  const generation = useRef(0);
  const controls = presentation.controls;
  const [values, setValues] = useState<Record<number, number>>({});

  const refresh = useCallback(async () => {
    const request = ++generation.current;
    const snapshot = await callNative<Presentation>(FN_GET_INSTRUMENT_PRESENTATION);
    if (snapshot && request === generation.current) {
      const list = snapshot.controls;
      setPresentation(snapshot);
      const init: Record<number, number> = {};
      for (const c of list) init[c.number] = c.value;
      setValues(init);
    }
  }, []);

  useEffect(() => {
    refresh();
    const offVals = onBackendEvent<Record<string, number>>(EVT_CC_VALUES, (d) => {
      setValues((prev) => {
        const next = { ...prev };
        for (const k in d) next[Number(k)] = d[k];
        return next;
      });
    });
    const offLoaded = onBackendEvent(EVT_SFZ_LOADED, () => {
      refresh();
    });
    return () => {
      ++generation.current;
      offVals();
      offLoaded();
    };
  }, [refresh]);

  const setCc = useCallback((number: number, value: number) => {
    setValues((prev) => ({ ...prev, [number]: value }));
    callNative(FN_SET_CC, number, value);
  }, []);

  return { controls, values, setCc, presentation };
}
