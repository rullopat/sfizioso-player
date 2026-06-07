import { Knob } from "@shared/components/Knob";
import { Segmented } from "@shared/components/Segmented";
import { useSliderParam, useComboBoxParam, useToggleParam } from "@shared/hooks/useParam";
import { PanelSection } from "./PanelSection";
import {
  PARAM_OVERSAMPLING,
  PARAM_PRELOAD_SIZE,
  PARAM_SQ_LIVE,
  PARAM_SQ_FREEWHEEL,
  PARAM_FREEWHEEL,
} from "../paramIds";
import "../styles/engine.css";

// sfizz sample-quality menu (0..10).
const QUALITY = [
  "Nearest", "Linear", "Poly", "Sinc 8", "Sinc 12", "Sinc 16",
  "Sinc 24", "Sinc 36", "Sinc 48", "Sinc 60", "Sinc 72",
];
const qualityName = (v: number) => QUALITY[Math.round(v)] ?? `${Math.round(v)}`;

// SMPL-86 — engine quality / performance settings.
export function EnginePanel() {
  const over = useComboBoxParam(PARAM_OVERSAMPLING, 0);
  const preload = useSliderParam(PARAM_PRELOAD_SIZE, 8);
  const sqLive = useSliderParam(PARAM_SQ_LIVE, 2);
  const sqFw = useSliderParam(PARAM_SQ_FREEWHEEL, 10);
  const fw = useToggleParam(PARAM_FREEWHEEL, false);

  const overOptions = (over.choices.length > 0 ? over.choices : ["1x", "2x", "4x", "8x"]).map(
    (label, i) => ({ label, value: i })
  );

  return (
    <PanelSection title="ENGINE" className="engine-panel">
      <Knob
        label="PRELOAD"
        value={preload.value}
        normalised={preload.normalised}
        onChange={preload.setNormalised}
        onDragStart={preload.dragStart}
        onDragEnd={preload.dragEnd}
        format={(v) => `${Math.round(v)} kB`}
      />
      <Knob
        label="LIVE QUAL"
        value={sqLive.value}
        normalised={sqLive.normalised}
        onChange={sqLive.setNormalised}
        onDragStart={sqLive.dragStart}
        onDragEnd={sqLive.dragEnd}
        format={qualityName}
      />
      <Knob
        label="FW QUAL"
        value={sqFw.value}
        normalised={sqFw.normalised}
        onChange={sqFw.setNormalised}
        onDragStart={sqFw.dragStart}
        onDragEnd={sqFw.dragEnd}
        format={qualityName}
      />
      <Segmented label="OVERSAMPLE" selected={over.index} options={overOptions} onSelect={over.setIndex} />
      <Segmented
        label="FREEWHEEL"
        selected={fw.value ? 1 : 0}
        options={[
          { label: "OFF", value: 0 },
          { label: "ON", value: 1 },
        ]}
        onSelect={(i) => fw.setValue(i === 1)}
      />
      <div className="engine-note">Oversampling — experimental (engine stub, no audible effect)</div>
    </PanelSection>
  );
}
