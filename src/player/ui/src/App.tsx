import { useEffect, useState } from "react";
import { TopBar } from "./components/TopBar";
import { PanelSection } from "./components/PanelSection";
import { InfoPanel } from "./components/InfoPanel";
import { EnginePanel } from "./components/EnginePanel";
import { TuningPanel } from "./components/TuningPanel";
import { MpePanel } from "./components/MpePanel";
import { ExperimentalPanel } from "./components/ExperimentalPanel";
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

const TABS = [
  { id: "output", label: "OUTPUT" },
  { id: "engine", label: "ENGINE" },
  { id: "tuning", label: "TUNING" },
  { id: "mpe", label: "MPE" },
  { id: "info", label: "INFO" },
  { id: "experimental", label: "EXPERIMENTAL" },
] as const;
type TabId = (typeof TABS)[number]["id"];

export default function App() {
  const [status, setStatus] = useState<Status>(EMPTY_STATUS);
  const [activeVoices, setActiveVoices] = useState(0);
  const [appInfo, setAppInfo] = useState<AppInfo>(DEFAULT_APP_INFO);
  const [tab, setTab] = useState<TabId>("output");

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

      {/* Each section is its own tab; the keyboard stays docked below so it's
          playable from any tab. */}
      <nav className="tab-bar">
        {TABS.map((t) => (
          <button
            key={t.id}
            className={`tab-btn ${tab === t.id ? "is-active" : ""}`}
            onClick={() => setTab(t.id)}
          >
            {t.label}
          </button>
        ))}
      </nav>

      <main className="tab-content">
        {tab === "output" && (
          <>
            <OutputModule />
            <CcControlsPanel />
          </>
        )}
        {tab === "engine" && <EnginePanel />}
        {tab === "tuning" && <TuningPanel />}
        {tab === "mpe" && <MpePanel />}
        {tab === "info" && <InfoPanel status={status} activeVoices={activeVoices} />}
        {tab === "experimental" && <ExperimentalPanel />}
      </main>

      {/* Docked, always-playable keyboard (SMPL-88). */}
      <div className="keyboard-dock">
        <Keyboard
          noteOnFn={FN_NOTE_ON}
          noteOffFn={FN_NOTE_OFF}
          getKeyLabelsFn={FN_GET_KEY_LABELS}
          notesEvent={EVT_NOTES}
          loadedEvent={EVT_SFZ_LOADED}
        />
      </div>

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
