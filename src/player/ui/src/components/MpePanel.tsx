import { useEffect, useState } from "react";
import { Knob } from "@shared/components/Knob";
import { Segmented } from "@shared/components/Segmented";
import { useSliderParam, useComboBoxParam, useToggleParam } from "@shared/hooks/useParam";
import { onBackendEvent } from "@shared/juceBridge";
import { PanelSection } from "./PanelSection";
import {
  PARAM_MPE_MODE,
  PARAM_MPE_MASTER_BEND,
  PARAM_MPE_PERNOTE_BEND,
  PARAM_MPE_IGNORE_MASTER,
  PARAM_MPE_IGNORE_PERNOTE,
  EVT_MPE,
} from "../paramIds";
import "../styles/mpe.css";

interface MpeState {
  enabled: boolean;
  masterBend: number;
  perNoteBend: number;
}

// SMPL-90 — MPE settings: mode, master/per-note bend range, RPN auto-config
// opt-outs, and live effective-bend readouts. Matches the rullopat/sfizz-ui
// fork's MPE panel (minus the wrapper-only "Ignore MCM" + last-RPN display).
export function MpePanel() {
  const mode = useComboBoxParam(PARAM_MPE_MODE, 2);
  const master = useSliderParam(PARAM_MPE_MASTER_BEND, 2);
  const perNote = useSliderParam(PARAM_MPE_PERNOTE_BEND, 48);
  const ignoreMaster = useToggleParam(PARAM_MPE_IGNORE_MASTER, false);
  const ignorePerNote = useToggleParam(PARAM_MPE_IGNORE_PERNOTE, false);
  const [eff, setEff] = useState<MpeState>({ enabled: false, masterBend: 0, perNoteBend: 0 });

  useEffect(() => {
    return onBackendEvent<MpeState>(EVT_MPE, (s) => setEff(s));
  }, []);

  const modeOptions =
    mode.choices.length > 0
      ? mode.choices.map((label, i) => ({ label: label.toUpperCase(), value: i }))
      : [
          { label: "OFF", value: 0 },
          { label: "PRES", value: 1 },
          { label: "FULL", value: 2 },
        ];

  return (
    <PanelSection title="MPE" className="mpe-panel">
      <Segmented label="MODE" selected={mode.index} options={modeOptions} onSelect={mode.setIndex} />
      <Knob
        label="MASTER BEND"
        value={master.value}
        normalised={master.normalised}
        onChange={master.setNormalised}
        onDragStart={master.dragStart}
        onDragEnd={master.dragEnd}
        format={(v) => `${Math.round(v)} st`}
      />
      <Knob
        label="PER-NOTE BEND"
        value={perNote.value}
        normalised={perNote.normalised}
        onChange={perNote.setNormalised}
        onDragStart={perNote.dragStart}
        onDragEnd={perNote.dragEnd}
        format={(v) => `${Math.round(v)} st`}
      />
      <Segmented
        label="IGNORE MASTER RPN"
        selected={ignoreMaster.value ? 1 : 0}
        options={[
          { label: "OFF", value: 0 },
          { label: "ON", value: 1 },
        ]}
        onSelect={(i) => ignoreMaster.setValue(i === 1)}
      />
      <Segmented
        label="IGNORE PER-NOTE RPN"
        selected={ignorePerNote.value ? 1 : 0}
        options={[
          { label: "OFF", value: 0 },
          { label: "ON", value: 1 },
        ]}
        onSelect={(i) => ignorePerNote.setValue(i === 1)}
      />
      <div className="mpe-readout">
        <span className={`mpe-dot ${eff.enabled ? "is-on" : ""}`} />
        <span className="mpe-eff">
          EFFECTIVE&nbsp;&nbsp;master {Math.round(eff.masterBend)} st&nbsp;·&nbsp;per-note {Math.round(eff.perNoteBend)} st
        </span>
      </div>
    </PanelSection>
  );
}
