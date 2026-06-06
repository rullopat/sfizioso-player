import { useEffect, useState } from "react";
import { onBackendEvent } from "../juceBridge";
import "../styles/meter.css";

type Peak = { L: number; R: number };
const ZERO: Peak = { L: 0, R: 0 };

// Multiplicative decay applied to the previous frame's value when a fresh
// max-since-last-emit arrives. At 30 Hz this works out to ~22 dB/sec fall
// time, close to standard PPM behaviour.
const DECAY = 0.88;

// Bottom of the meter scale, in dB. Anything quieter is collapsed to silence.
const FLOOR_DB = -60;

function linearToDb (v: number): number {
  if (v <= 1e-6) return -Infinity;
  return 20 * Math.log10 (v);
}

function dbToPercent (db: number): number {
  if (!isFinite (db) || db <= FLOOR_DB) return 0;
  if (db >= 0) return 100;
  return ((db - FLOOR_DB) / -FLOOR_DB) * 100;
}

function widthFor (linear: number): string {
  return `${dbToPercent (linearToDb (linear)).toFixed (1)}%`;
}

interface Props {
  eventId: string;
  /** "horizontal" (default): L/R bars + dBFS scale. "vertical": two compact
   *  bottom-anchored bars, no scale — for tight inline placements (SMPL-74). */
  orientation?: "horizontal" | "vertical";
}

export function LevelMeter({ eventId, orientation = "horizontal" }: Props) {
  const [level, setLevel] = useState<Peak>(ZERO);

  useEffect(() => {
    let cur: Peak = { ...ZERO };
    return onBackendEvent<{ peakL?: number; peakR?: number }>(eventId, (d) => {
      cur = {
        L: Math.max (cur.L * DECAY, d.peakL ?? 0),
        R: Math.max (cur.R * DECAY, d.peakR ?? 0),
      };
      setLevel (cur);
    });
  }, [eventId]);

  if (orientation === "vertical") {
    return (
      <div className="meter meter-vertical" title="Output level (L / R)">
        <div className="meter-vbar"><div className="meter-vfill" style={{ height: widthFor (level.L) }} /></div>
        <div className="meter-vbar"><div className="meter-vfill" style={{ height: widthFor (level.R) }} /></div>
      </div>
    );
  }

  return (
    <div className="meter">
      <div className="meter-row">
        <span className="meter-channel-label">L</span>
        <div className="meter-channel">
          <div className="meter-fill" style={{ width: widthFor (level.L) }} />
        </div>
      </div>
      <div className="meter-row">
        <span className="meter-channel-label">R</span>
        <div className="meter-channel">
          <div className="meter-fill" style={{ width: widthFor (level.R) }} />
        </div>
      </div>

      {/* dB tick row — positions match (db - FLOOR) / -FLOOR. With FLOOR=-60:
            -60 = 0%, -40 = 33.3%, -20 = 66.6%, -12 = 80%, -6 = 90%, 0 = 100%. */}
      <div className="meter-scale-row">
        <span className="meter-scale-spacer" />
        <div className="meter-scale">
          <span className="meter-scale-tick is-start">-∞</span>
          <span className="meter-scale-tick" style={{ left: "33.33%" }}>-40</span>
          <span className="meter-scale-tick" style={{ left: "66.66%" }}>-20</span>
          <span className="meter-scale-tick" style={{ left: "80%" }}>-12</span>
          <span className="meter-scale-tick" style={{ left: "90%" }}>-6</span>
          <span className="meter-scale-tick is-end">0 dBFS</span>
        </div>
      </div>
    </div>
  );
}
