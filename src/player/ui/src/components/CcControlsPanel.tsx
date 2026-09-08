import { CSSProperties } from "react";
import { Knob } from "@shared/components/Knob";
import { Segmented } from "@shared/components/Segmented";
import { PanelSection } from "./PanelSection";
import { CcControl, useCcControls } from "../hooks/useCcControls";
import "../styles/cc.css";

export function CcControlsPanel() {
  const { controls, values, setCc, presentation } = useCcControls();
  const sections = [...new Set(controls.map(c => c.section))];
  const renderControl = (c: CcControl) => {
    const v = values[c.number] ?? c.value;
    if (c.widget === "toggle") return (
      <Segmented key={c.number} label={c.label} selected={v * 127 >= 64 ? 1 : 0}
        options={[{ label: "OFF", value: 0 }, { label: "ON", value: 1 }]}
        onSelect={i => setCc(c.number, i === 1 ? 1 : 0)} />
    );
    if (c.widget === "slider") return (
      <label className="cc-slider" key={c.number}>
        <span>{c.label}</span>
        <input type="range" min={0} max={127} step={1} value={Math.round(v * 127)}
          aria-label={c.label} onChange={e => setCc(c.number, Number(e.target.value) / 127)} />
        <output>{Math.round(v * 127)}</output>
      </label>
    );
    return <Knob key={c.number} label={c.label} value={v * 127} normalised={v}
      onChange={n => setCc(c.number, n)} format={x => `${Math.round(x)}`} />;
  };
  const style = {
    ...(presentation.accent ? { "--accent": presentation.accent } : {}),
    ...(presentation.artworkUrl ? { backgroundImage: `linear-gradient(#090b10b8, #090b10b8), url("${presentation.artworkUrl}")` } : {}),
  } as CSSProperties;
  return (
    <PanelSection title="CONTROLS" className="cc-panel">
      <div className="instrument-presentation" style={style}>
        {(presentation.instrumentName || presentation.presetName) && (
          <div className="instrument-identity">
            <strong>{presentation.instrumentName}</strong>
            <span>{presentation.presetName}</span>
          </div>
        )}
        {controls.length === 0 ? <div className="cc-empty">No mapped controls</div> : sections.map(id => (
          <div className="cc-section" key={id}>
            {id && <h3>{controls.find(c => c.section === id)?.sectionLabel}</h3>}
            <div className="cc-grid">{controls.filter(c => c.section === id).map(renderControl)}</div>
          </div>
        ))}
        {presentation.diagnostics.length > 0 && (
          <details className="instrument-diagnostics">
            <summary>Some instrument details could not be loaded</summary>
            <ul>{presentation.diagnostics.map((message, i) => <li key={i}>{message}</li>)}</ul>
          </details>
        )}
      </div>
    </PanelSection>
  );
}
