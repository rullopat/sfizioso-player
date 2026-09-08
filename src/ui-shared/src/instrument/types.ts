// SPDX-License-Identifier: BSD-2-Clause
// SMPL-85 — one auto-generated control per labelled CC of the loaded SFZ.
export interface CcControl {
  number: number;
  label: string;
  value: number;        // normalised 0..1
  widget: "knob" | "slider" | "toggle";
  section: string;
  sectionLabel: string;
}

export interface Presentation {
  controls: CcControl[];
  instrumentName: string;
  presetName: string;
  accent: string;
  artworkUrl: string;
  diagnostics: string[];
}

