import { Dropdown } from "@shared/components/Dropdown";
import { Segmented } from "@shared/components/Segmented";
import { useComboBoxParam, useToggleParam } from "@shared/hooks/useParam";
import { PanelSection } from "./PanelSection";
import {
  PARAM_OVERSAMPLING,
  PARAM_PRELOAD_SIZE,
  PARAM_SQ_LIVE,
  PARAM_SQ_FREEWHEEL,
  PARAM_OSC_Q_LIVE,
  PARAM_OSC_Q_FREEWHEEL,
  PARAM_FREEWHEEL,
  PARAM_SUSTAIN_CANCELS,
} from "../paramIds";
import "../styles/engine.css";

const TOGGLE = [
  { label: "OFF", value: 0 },
  { label: "ON", value: 1 },
];

// SMPL-86 — engine quality / performance. Selections are comboboxes (labels
// come from the Choice params); booleans are toggles. Option labels mirror
// sfizz-ui (sample 0..10, oscillator 0..3).
export function EnginePanel() {
  const over = useComboBoxParam(PARAM_OVERSAMPLING, 0);
  const preload = useComboBoxParam(PARAM_PRELOAD_SIZE, 1);
  const sampleQ = useComboBoxParam(PARAM_SQ_LIVE, 2);
  const sampleQFw = useComboBoxParam(PARAM_SQ_FREEWHEEL, 10);
  const oscQ = useComboBoxParam(PARAM_OSC_Q_LIVE, 1);
  const oscQFw = useComboBoxParam(PARAM_OSC_Q_FREEWHEEL, 3);
  const fw = useToggleParam(PARAM_FREEWHEEL, false);
  const sustain = useToggleParam(PARAM_SUSTAIN_CANCELS, false);

  return (
    <PanelSection title="ENGINE" className="engine-panel">
      <Dropdown label="Oversampling" value={over.index} options={over.choices} onChange={over.setIndex} />
      <Dropdown label="Preload" value={preload.index} options={preload.choices} onChange={preload.setIndex} />

      <Dropdown label="Sample Quality" value={sampleQ.index} options={sampleQ.choices} onChange={sampleQ.setIndex} />
      <Dropdown label="…When Freewheeling" value={sampleQFw.index} options={sampleQFw.choices} onChange={sampleQFw.setIndex} />

      <Dropdown label="Oscillator Quality" value={oscQ.index} options={oscQ.choices} onChange={oscQ.setIndex} />
      <Dropdown label="…When Freewheeling" value={oscQFw.index} options={oscQFw.choices} onChange={oscQFw.setIndex} />

      <Segmented label="Freewheel" selected={fw.value ? 1 : 0} options={TOGGLE} onSelect={(i) => fw.setValue(i === 1)} />
      <Segmented
        label="Sustain Cancels Release"
        selected={sustain.value ? 1 : 0}
        options={TOGGLE}
        onSelect={(i) => sustain.setValue(i === 1)}
      />

      <div className="engine-note">Oversampling — experimental (engine stub, no audible effect)</div>
    </PanelSection>
  );
}
