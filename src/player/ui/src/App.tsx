import { useEffect, useState } from "react";
import { TopBar } from "./components/TopBar";
import { ParamModule } from "@shared/components/ParamModule";
import { Knob } from "@shared/components/Knob";
import { Segmented } from "@shared/components/Segmented";
import { LevelMeter } from "@shared/components/LevelMeter";
import { useSliderParam, useComboBoxParam } from "@shared/hooks/useParam";
import { callNative, onBackendEvent } from "@shared/juceBridge";
import {
  PARAM_GAIN_DB,
  PARAM_POLYPHONY,
  PARAM_MPE_MODE,
  FN_LOAD_SFZ,
  FN_GET_STATUS,
  EVT_VOICES,
  EVT_METER,
} from "./paramIds";
import "./styles/app.css";

type Status = {
  sfzPath: string;
  fileName: string;
  numRegions: number;
  numPreloadedSamples: number;
};

const EMPTY_STATUS: Status = {
  sfzPath: "",
  fileName: "",
  numRegions: 0,
  numPreloadedSamples: 0,
};

export default function App() {
  const [status, setStatus] = useState<Status>(EMPTY_STATUS);
  const [activeVoices, setActiveVoices] = useState(0);

  useEffect(() => {
    callNative<Status>(FN_GET_STATUS).then((s) => {
      if (s) setStatus(s);
    });
    const unsub = onBackendEvent<{ active: number }>(EVT_VOICES, (e) =>
      setActiveVoices(e.active)
    );
    return unsub;
  }, []);

  const loadSfz = async () => {
    const res = await callNative<Status & { ok: boolean }>(FN_LOAD_SFZ);
    if (res?.ok) {
      setStatus({
        sfzPath: res.sfzPath,
        fileName: res.fileName,
        numRegions: res.numRegions,
        numPreloadedSamples: res.numPreloadedSamples,
      });
    }
  };

  const sampleName = status.fileName || "No SFZ loaded";
  const meta =
    status.numRegions > 0
      ? `${status.numRegions} regions · ${status.numPreloadedSamples} samples`
      : "--";

  return (
    <div className="app noise">
      <TopBar sampleName={sampleName} meta={meta} onLoad={loadSfz} />

      <main className="app-main">
        <OutputModule />
      </main>

      <footer className="app-footer">
        <span className="brand">SAMPLE MACHINE PLAYER</span>
        <span className="voices-readout">
          ACTIVE VOICES <strong>{activeVoices}</strong>
        </span>
        <span>v0.1.0</span>
      </footer>
    </div>
  );
}

function OutputModule() {
  const gain = useSliderParam(PARAM_GAIN_DB, 0);
  const poly = useSliderParam(PARAM_POLYPHONY, 16);
  const mpe = useComboBoxParam(PARAM_MPE_MODE, 2);

  return (
    <ParamModule title="OUTPUT">
      <Knob
        label="GAIN"
        value={gain.value}
        normalised={gain.normalised}
        onChange={gain.setNormalised}
        onDragStart={gain.dragStart}
        onDragEnd={gain.dragEnd}
        large
        format={(v) => `${v.toFixed(1)} dB`}
      />
      <Knob
        label="VOICES"
        value={poly.value}
        normalised={poly.normalised}
        onChange={poly.setNormalised}
        onDragStart={poly.dragStart}
        onDragEnd={poly.dragEnd}
        large
        format={(v) => `${Math.round(v)}`}
      />
      <Segmented
        label="MPE"
        selected={mpe.index}
        options={
          mpe.choices.length > 0
            ? mpe.choices.map((label, i) => ({ label: label.toUpperCase(), value: i }))
            : [
                { label: "OFF", value: 0 },
                { label: "PRES", value: 1 },
                { label: "FULL", value: 2 },
              ]
        }
        onSelect={mpe.setIndex}
      />
      <div style={{ flexBasis: "100%" }}>
        <LevelMeter eventId={EVT_METER} />
      </div>
    </ParamModule>
  );
}
