import { useEffect, useState } from "react";
import { TopBar } from "./components/TopBar";
import { PanelSection } from "./components/PanelSection";
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
  FN_GET_APP_INFO,
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

type AppInfo = {
  productName: string;
  version: string;
};

const EMPTY_STATUS: Status = {
  sfzPath: "",
  fileName: "",
  numRegions: 0,
  numPreloadedSamples: 0,
};

// Fallback until getAppInfo resolves; the real values come from the JUCE
// plugin defines (CMake PRODUCT_NAME / VERSION) — never hardcode the brand.
const DEFAULT_APP_INFO: AppInfo = { productName: "Sfizioso Player", version: "" };

export default function App() {
  const [status, setStatus] = useState<Status>(EMPTY_STATUS);
  const [activeVoices, setActiveVoices] = useState(0);
  const [appInfo, setAppInfo] = useState<AppInfo>(DEFAULT_APP_INFO);

  useEffect(() => {
    callNative<Status>(FN_GET_STATUS).then((s) => {
      if (s) setStatus(s);
    });
    callNative<AppInfo>(FN_GET_APP_INFO).then((a) => {
      if (a?.productName) setAppInfo(a);
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
      <TopBar
        productName={appInfo.productName}
        sampleName={sampleName}
        meta={meta}
        onLoad={loadSfz}
      />

      <main className="app-main">
        {/* CONTROLS — auto-generated CC controls (SMPL-85). */}
        <PanelSection
          title="CONTROLS"
          area="cc"
          placeholder
          hint="Auto-generated CC controls"
        />

        <div className="center-stack">
          <OutputModule />
          {/* ENGINE — oversampling / preload / quality (SMPL-86). */}
          <PanelSection
            title="ENGINE"
            placeholder
            hint="Oversampling · preload · quality"
          />
          {/* TUNING — Scala / root key / A4 / stretch (SMPL-87). */}
          <PanelSection
            title="TUNING"
            placeholder
            hint="Scala · root key · A4 · stretch"
          />
        </div>

        {/* INSTRUMENT INFO — full sfizz stats (SMPL-83). */}
        <PanelSection
          title="INSTRUMENT INFO"
          area="info"
          placeholder
          hint="Regions · groups · sample rate"
        />

        {/* KEYBOARD — playable on-screen keyboard (SMPL-88). */}
        <PanelSection
          title="KEYBOARD"
          area="keyboard"
          placeholder
          hint="Playable on-screen keyboard"
        />
      </main>

      <footer className="app-footer">
        <span className="brand">{appInfo.productName.toUpperCase()}</span>
        <span className="voices-readout">
          ACTIVE VOICES <strong>{activeVoices}</strong>
        </span>
        <span>{appInfo.version ? `v${appInfo.version}` : ""}</span>
      </footer>
    </div>
  );
}

function OutputModule() {
  const gain = useSliderParam(PARAM_GAIN_DB, 0);
  const poly = useSliderParam(PARAM_POLYPHONY, 16);
  const mpe = useComboBoxParam(PARAM_MPE_MODE, 2);

  return (
    <PanelSection title="OUTPUT" className="output-panel">
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
    </PanelSection>
  );
}
