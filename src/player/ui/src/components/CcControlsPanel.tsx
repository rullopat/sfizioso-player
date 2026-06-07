import { Knob } from "@shared/components/Knob";
import { Segmented } from "@shared/components/Segmented";
import { PanelSection } from "./PanelSection";
import { useCcControls } from "../hooks/useCcControls";
import "../styles/cc.css";

// SMPL-85 — auto-generated controls, one per labelled CC of the loaded SFZ.
export function CcControlsPanel() {
  const { controls, values, setCc } = useCcControls();

  return (
    <PanelSection title="CONTROLS">
      {controls.length === 0 ? (
        <div className="cc-empty">No mapped controls</div>
      ) : (
        <div className="cc-grid">
          {controls.map((c) => {
            const v = values[c.number] ?? c.value;
            if (c.isSwitch) {
              return (
                <Segmented
                  key={c.number}
                  label={c.label || `CC ${c.number}`}
                  selected={v >= 0.5 ? 1 : 0}
                  options={[
                    { label: "OFF", value: 0 },
                    { label: "ON", value: 1 },
                  ]}
                  onSelect={(i) => setCc(c.number, i === 1 ? 1 : 0)}
                />
              );
            }
            return (
              <Knob
                key={c.number}
                label={c.label || `CC ${c.number}`}
                value={v * 127}
                normalised={v}
                onChange={(n) => setCc(c.number, n)}
                format={(x) => `${Math.round(x)}`}
              />
            );
          })}
        </div>
      )}
    </PanelSection>
  );
}
