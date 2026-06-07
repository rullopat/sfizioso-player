// Shape of the status object returned by getStatus / loadSfz / loadSfzPath and
// pushed on the sfzLoaded / sfzReloaded events. Byte-identical keys to
// PlayerEditor::makeStatusObject (C++).
export interface Status {
  ok?: boolean;
  sfzPath: string;
  fileName: string;
  numRegions: number;
  numMasters: number;
  numGroups: number;
  numCurves: number;
  numPreloadedSamples: number;
  numVoices: number;
  sampleRate: number;
}

export const EMPTY_STATUS: Status = {
  sfzPath: "",
  fileName: "",
  numRegions: 0,
  numMasters: 0,
  numGroups: 0,
  numCurves: 0,
  numPreloadedSamples: 0,
  numVoices: 0,
  sampleRate: 0,
};
