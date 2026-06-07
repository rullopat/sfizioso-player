import { useEffect, useState } from "react";
import { TopBar } from "./components/TopBar";
import { PanelSection } from "./components/PanelSection";
import { InfoPanel } from "./components/InfoPanel";
import { EnginePanel } from "./components/EnginePanel";
import { TuningPanel } from "./components/TuningPanel";
import { MpePanel } from "./components/MpePanel";
import { CcControlsPanel } from "./components/CcControlsPanel";
import { Knob } from "@shared/components/Knob";
import { LevelMeter } from "@shared/components/LevelMeter";
import { Keyboard } from "@shared/components/Keyboard";
import { useSliderParam } from "@shared/hooks/useParam";
import { callNative, onBackendEvent } from "@shared/juceBridge";
import {
  PARAM_GAIN_DB,
  PARAM_POLYPHONY,
  FN_LOAD_SFZ,
  FN_GET_STATUS,
  FN_GET_APP_INFO,
  FN_GET_KEY_LABELS,
  FN_NOTE_ON,
  FN_NOTE_OFF,
  EVT_VOICES,
  EVT_METER,
  EVT_NOTES,
  EVT_SFZ_LOADED,
  EVT_SFZ_RELOADED,
} from "./paramIds";
import { Status, EMPTY_STATUS } from "./types";
import "./styles/app.css";

type AppInfo = {
  productName: string;
  version: string;
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
    const offVoices = onBackendEvent<{ active: number }>(EVT_VOICES, (e) =>
      setActiveVoices(e.active)
    );
    // SMPL-89 — re-hydrate the status strip + info panel on any (re)load,
    // including recent-files clicks, drag-drop, and on-disk auto-reload.
    const apply = (s: Status) => {
      if (s) setStatus(s);
    };
    const offLoaded = onBackendEvent<Status>(EVT_SFZ_LOADED, apply);
    const offReloaded = onBackendEvent<Status>(EVT_SFZ_RELOADED, apply);
    return () => {
      offVoices();
      offLoaded();
      offReloaded();
    };
  }, []);

  const loadSfz = async () => {
    const res = await callNative<Status>(FN_LOAD_SFZ);
    if (res?.ok) setStatus(res);
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
        <CcControlsPanel />

        <div className="center-stack">
          <OutputModule />
          <EnginePanel />
          <TuningPanel />
        </div>

        {/* Right column: instrument info (SMPL-83) over the MPE panel (SMPL-90). */}
        <div className="right-stack">
          <InfoPanel status={status} activeVoices={activeVoices} />
          <MpePanel />
        </div>

        {/* KEYBOARD — playable on-screen keyboard (SMPL-88). */}
        <PanelSection title="KEYBOARD" area="keyboard" className="keyboard-panel">
          <Keyboard
            noteOnFn={FN_NOTE_ON}
            noteOffFn={FN_NOTE_OFF}
            getKeyLabelsFn={FN_GET_KEY_LABELS}
            notesEvent={EVT_NOTES}
            loadedEvent={EVT_SFZ_LOADED}
          />
        </PanelSection>
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
      {/* SMPL-84 — output level meter, in the OUTPUT monitor region near gain. */}
      <div style={{ flexBasis: "100%" }}>
        <LevelMeter eventId={EVT_METER} />
      </div>
    </PanelSection>
  );
}
