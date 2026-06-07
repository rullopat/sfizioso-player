import { useCallback, useEffect, useState } from "react";
import { callNative, onBackendEvent } from "@shared/juceBridge";
import {
  FN_GET_CC_CONTROLS,
  FN_SET_CC,
  EVT_CC_VALUES,
  EVT_SFZ_LOADED,
} from "../paramIds";

// SMPL-85 — one auto-generated control per labelled CC of the loaded SFZ.
export interface CcControl {
  number: number;
  label: string;
  value: number;        // normalised 0..1
  defaultValue: number; // normalised 0..1
  isSwitch: boolean;
}

/**
 * Fetches the CC-control list for the loaded instrument, tracks live values
 * (UI edits + incoming MIDI/automation reflected via the ccValues event), and
 * rebuilds whenever an SFZ (re)loads.
 */
export function useCcControls() {
  const [controls, setControls] = useState<CcControl[]>([]);
  const [values, setValues] = useState<Record<number, number>>({});

  const refresh = useCallback(async () => {
    const list = await callNative<CcControl[]>(FN_GET_CC_CONTROLS);
    if (list) {
      setControls(list);
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
      offVals();
      offLoaded();
    };
  }, [refresh]);

  const setCc = useCallback((number: number, value: number) => {
    setValues((prev) => ({ ...prev, [number]: value }));
    callNative(FN_SET_CC, number, value);
  }, []);

  return { controls, values, setCc };
}
