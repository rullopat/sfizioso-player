import { useCallback, useEffect, useRef, useState } from "react";
import { callNative, onBackendEvent } from "../juceBridge";
import "../styles/keyboard.css";

// SMPL-88 — playable on-screen keyboard. Lives in the shared kit so the
// autosampler / romplers can reuse it. Note injection and the lit-key
// highlight cross the bridge via native functions + a backend event whose
// names are passed in (defaulting to the player's IDs) so the component stays
// decoupled from any one target's paramIds.
interface Props {
  lowNote?: number;
  highNote?: number;
  velocity?: number;
  noteOnFn?: string;
  noteOffFn?: string;
  getKeyLabelsFn?: string;
  notesEvent?: string;
  loadedEvent?: string;
}

const WHITE_SEMITONES = new Set([0, 2, 4, 5, 7, 9, 11]);
const isBlack = (n: number) => !WHITE_SEMITONES.has(((n % 12) + 12) % 12);

export function Keyboard({
  lowNote = 24,   // C1
  highNote = 96,  // C7
  velocity = 100,
  noteOnFn = "noteOn",
  noteOffFn = "noteOff",
  getKeyLabelsFn = "getKeyLabels",
  notesEvent = "notes",
  loadedEvent = "sfzLoaded",
}: Props) {
  const [lit, setLit] = useState<Set<number>>(new Set());
  const [mapped, setMapped] = useState<Set<number> | null>(null);
  const held = useRef<Set<number>>(new Set());

  // Lit keys = the set this UI is holding (echoed back from the processor).
  useEffect(() => {
    return onBackendEvent<{ on: number[] }>(notesEvent, (d) =>
      setLit(new Set(d.on ?? []))
    );
  }, [notesEvent]);

  // Mapped keys (greying) from getKeyLabels, refreshed on each SFZ (re)load.
  const refreshLabels = useCallback(async () => {
    const res = await callNative<{ usedKeys: number[] }>(getKeyLabelsFn);
    if (res?.usedKeys && res.usedKeys.length > 0) setMapped(new Set(res.usedKeys));
    else setMapped(null);
  }, [getKeyLabelsFn]);

  useEffect(() => {
    refreshLabels();
    return onBackendEvent(loadedEvent, () => refreshLabels());
  }, [refreshLabels, loadedEvent]);

  const press = useCallback(
    (n: number) => {
      if (held.current.has(n)) return;
      held.current.add(n);
      callNative(noteOnFn, n, velocity);
    },
    [noteOnFn, velocity]
  );
  const release = useCallback(
    (n: number) => {
      if (!held.current.has(n)) return;
      held.current.delete(n);
      callNative(noteOffFn, n);
    },
    [noteOffFn]
  );
  const releaseAll = useCallback(() => {
    held.current.forEach((n) => callNative(noteOffFn, n));
    held.current.clear();
  }, [noteOffFn]);

  // Belt-and-braces: release everything on unmount.
  useEffect(() => releaseAll, [releaseAll]);

  const whites: number[] = [];
  const blacks: Array<{ n: number; idx: number }> = [];
  for (let n = lowNote; n <= highNote; n++) {
    if (isBlack(n)) blacks.push({ n, idx: whites.length });
    else whites.push(n);
  }
  const w = whites.length;
  const blackWidthPct = (100 / w) * 0.62;

  const keyClass = (n: number, base: string) =>
    `${base}` +
    (lit.has(n) ? " is-lit" : "") +
    (mapped && !mapped.has(n) ? " is-unmapped" : "");

  return (
    <div
      className="keyboard"
      onPointerUp={releaseAll}
      onPointerLeave={releaseAll}
    >
      <div className="keyboard-keys">
        {whites.map((n) => (
          <div
            key={n}
            className={keyClass(n, "kbd-white")}
            onPointerDown={(e) => {
              (e.currentTarget as HTMLElement).releasePointerCapture?.(e.pointerId);
              press(n);
            }}
            onPointerEnter={(e) => {
              if (e.buttons & 1) press(n);
            }}
            onPointerLeave={() => release(n)}
            onPointerUp={() => release(n)}
          />
        ))}
        {blacks.map(({ n, idx }) => (
          <div
            key={n}
            className={keyClass(n, "kbd-black")}
            style={{ left: `${idx * (100 / w)}%`, width: `${blackWidthPct}%` }}
            onPointerDown={(e) => {
              (e.currentTarget as HTMLElement).releasePointerCapture?.(e.pointerId);
              press(n);
            }}
            onPointerEnter={(e) => {
              if (e.buttons & 1) press(n);
            }}
            onPointerLeave={() => release(n)}
            onPointerUp={() => release(n)}
          />
        ))}
      </div>
    </div>
  );
}
