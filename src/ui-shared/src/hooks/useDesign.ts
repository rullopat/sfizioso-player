import { useEffect, useState } from "react";

export type DesignFamily = "modern" | "vintage" | "studio" | "panel";

// Reads the brand-locked design family from the `data-design` attribute
// on <html>, set by the rompler App on mount from the brand's manifest
// (defaults to "modern" if absent). The MutationObserver covers two
// edge cases:
//   1) Components that render before App's brand-info effect resolves
//      see the default at first; the observer fires when the attribute
//      is later applied, triggering a re-render with the real value.
//   2) Future hot-swap of design (if we ever expose user override) just
//      works without changes to consumers.
// Used by the shared Knob component to swap to design-specific SVG
// decorations (vintage knurled ring, studio cardinal dots, panel
// fluted chrome cap + 4-point cross indicator).
export function useDesign(): DesignFamily
{
    const [design, setDesign] = useState<DesignFamily>(() => readDesign());

    useEffect(() => {
        const update = () => setDesign (readDesign());
        const obs = new MutationObserver (update);
        obs.observe (document.documentElement, {
            attributes: true,
            attributeFilter: ["data-design"],
        });
        update();
        return () => obs.disconnect();
    }, []);

    return design;
}

function readDesign(): DesignFamily
{
    const v = document.documentElement.dataset.design;
    return v === "vintage" || v === "studio" || v === "panel" ? v : "modern";
}
