import { PanelSection } from "./PanelSection";
import { Status } from "../types";
import "../styles/info.css";

interface Props {
  status: Status;
  activeVoices: number;
}

// SMPL-83 — read-only instrument stat grid (full sfizz parse/runtime counts).
export function InfoPanel({ status, activeVoices }: Props) {
  const rows: Array<[string, string | number]> = [
    ["Regions", status.numRegions],
    ["Masters", status.numMasters],
    ["Groups", status.numGroups],
    ["Curves", status.numCurves],
    ["Preloaded", status.numPreloadedSamples],
    ["Voices", status.numVoices],
    ["Active", activeVoices],
    ["Sample Rate", status.sampleRate ? `${(status.sampleRate / 1000).toFixed(1)} kHz` : "--"],
  ];

  return (
    <PanelSection title="INSTRUMENT INFO">
      <div className="info-grid">
        {rows.map(([label, value]) => (
          <div className="info-row" key={label}>
            <span className="info-label">{label}</span>
            <span className="info-value">{value}</span>
          </div>
        ))}
      </div>
    </PanelSection>
  );
}
