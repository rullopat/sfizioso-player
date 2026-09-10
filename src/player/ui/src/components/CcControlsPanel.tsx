import { InstrumentControls } from "@shared/components/InstrumentControls";
import { useCcControls } from "../hooks/useCcControls";
import { PanelSection } from "./PanelSection";
import "../styles/cc.css";

export function CcControlsPanel() {
  const { presentation, values, setCc } = useCcControls();
  return <PanelSection title="CONTROLS" className="cc-panel">
    <InstrumentControls presentation={presentation} values={values} setCc={setCc} />
  </PanelSection>;
}
