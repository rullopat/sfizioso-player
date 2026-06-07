import { useEffect, useState } from "react";
import { Knob } from "@shared/components/Knob";
import { useSliderParam } from "@shared/hooks/useParam";
import { callNative, onBackendEvent } from "@shared/juceBridge";
import { PanelSection } from "./PanelSection";
import {
  PARAM_SCALA_ROOT_KEY,
  PARAM_TUNING_FREQ,
  PARAM_STRETCH,
  FN_LOAD_SCALA,
  FN_RESET_SCALA,
  FN_GET_TUNING,
  EVT_TUNING,
} from "../paramIds";
import "../styles/tuning.css";

interface Tuning {
  scaleName: string;
  rootKey: number;
  tuningHz: number;
  stretch: number;
}

const NOTE_NAMES = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"];
const noteName = (n: number) => `${NOTE_NAMES[((n % 12) + 12) % 12]}${Math.floor(n / 12) - 1}`;

// SMPL-87 — Scala scale, root key, A4 frequency, stretch tuning.
export function TuningPanel() {
  const root = useSliderParam(PARAM_SCALA_ROOT_KEY, 60);
  const freq = useSliderParam(PARAM_TUNING_FREQ, 440);
  const stretch = useSliderParam(PARAM_STRETCH, 0);
  const [scaleName, setScaleName] = useState("12-TET / Default");

  useEffect(() => {
    callNative<Tuning>(FN_GET_TUNING).then((t) => {
      if (t?.scaleName) setScaleName(t.scaleName);
    });
    return onBackendEvent<Tuning>(EVT_TUNING, (t) => {
      if (t?.scaleName) setScaleName(t.scaleName);
    });
  }, []);

  const loadScala = async () => {
    const t = await callNative<Tuning>(FN_LOAD_SCALA);
    if (t?.scaleName) setScaleName(t.scaleName);
  };
  const resetScala = async () => {
    const t = await callNative<Tuning>(FN_RESET_SCALA);
    if (t?.scaleName) setScaleName(t.scaleName);
  };

  return (
    <PanelSection title="TUNING" className="tuning-panel">
      <Knob
        label="ROOT"
        value={root.value}
        normalised={root.normalised}
        onChange={root.setNormalised}
        onDragStart={root.dragStart}
        onDragEnd={root.dragEnd}
        format={(v) => noteName(Math.round(v))}
      />
      <Knob
        label="A4 Hz"
        value={freq.value}
        normalised={freq.normalised}
        onChange={freq.setNormalised}
        onDragStart={freq.dragStart}
        onDragEnd={freq.dragEnd}
        format={(v) => v.toFixed(1)}
      />
      <Knob
        label="STRETCH"
        value={stretch.value}
        normalised={stretch.normalised}
        onChange={stretch.setNormalised}
        onDragStart={stretch.dragStart}
        onDragEnd={stretch.dragEnd}
        bipolar={false}
        format={(v) => v.toFixed(2)}
      />
      <div className="tuning-scale">
        <span className="tuning-scale-label">SCALE</span>
        <span className="tuning-scale-name" title={scaleName}>{scaleName}</span>
        <div className="tuning-scale-btns">
          <button className="tuning-btn" onClick={loadScala}>Load Scala…</button>
          <button className="tuning-btn" onClick={resetScala}>Reset</button>
        </div>
      </div>
    </PanelSection>
  );
}
