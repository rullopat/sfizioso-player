// SPDX-License-Identifier: BSD-2-Clause
import { useCallback, useEffect, useState, useRef } from "react";
import { callNative, onBackendEvent } from "@shared/juceBridge";
import { Presentation } from "../instrument/types";

export interface PresentationProtocol {
  getPresentation: string;
  setCc: string;
  ccValues: string;
  instrumentLoaded: string;
}

/**
 * Fetches the CC-control list for the loaded instrument, tracks live values
 * (UI edits + incoming MIDI/automation reflected via the ccValues event), and
 * rebuilds whenever an SFZ (re)loads.
 */
export function useInstrumentControls(protocol: PresentationProtocol) {
  const [presentation, setPresentation] = useState<Presentation>({
    controls: [], instrumentName: "", presetName: "", accent: "", artworkUrl: "", diagnostics: [],
  });
  const generation = useRef(0);
  const controls = presentation.controls;
  const [values, setValues] = useState<Record<number, number>>({});

  const refresh = useCallback(async () => {
    const request = ++generation.current;
    const snapshot = await callNative<Presentation>(protocol.getPresentation);
    if (snapshot && request === generation.current) {
      const list = snapshot.controls;
      setPresentation(snapshot);
      const init: Record<number, number> = {};
      for (const c of list) init[c.number] = c.value;
      setValues(init);
    }
  }, [protocol.getPresentation]);

  useEffect(() => {
    refresh();
    const offVals = onBackendEvent<Record<string, number>>(protocol.ccValues, (d) => {
      setValues((prev) => {
        const next = { ...prev };
        for (const k in d) next[Number(k)] = d[k];
        return next;
      });
    });
    const offLoaded = onBackendEvent(protocol.instrumentLoaded, () => {
      refresh();
    });
    return () => {
      ++generation.current;
      offVals();
      offLoaded();
    };
  }, [refresh, protocol.ccValues, protocol.instrumentLoaded]);

  const setCc = useCallback((number: number, value: number) => {
    setValues((prev) => ({ ...prev, [number]: value }));
    callNative(protocol.setCc, number, value);
  }, [protocol.setCc]);

  return { controls, values, setCc, presentation };
}
