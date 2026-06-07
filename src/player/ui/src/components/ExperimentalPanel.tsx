import { Dropdown } from "@shared/components/Dropdown";
import { useComboBoxParam } from "@shared/hooks/useParam";
import { PanelSection } from "./PanelSection";
import { PARAM_OVERSAMPLING } from "../paramIds";
import "../styles/experimental.css";

// Home for controls that are wired to the engine but not yet functional, so
// they don't imply an audible effect where there is none. Oversampling is an
// inactive stub in the current sfizz core.
export function ExperimentalPanel() {
  const over = useComboBoxParam(PARAM_OVERSAMPLING, 0);

  return (
    <PanelSection title="EXPERIMENTAL" className="experimental-panel">
      <p className="experimental-intro">
        These controls are wired to the engine but not yet functional — they have no
        audible effect today and are exposed for testing only.
      </p>
      <Dropdown label="Oversampling" value={over.index} options={over.choices} onChange={over.setIndex} />
      <div className="experimental-note">
        Oversampling — inactive engine stub (no DSP effect in the current sfizz core).
      </div>
    </PanelSection>
  );
}
