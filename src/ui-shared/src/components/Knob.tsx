import { useCallback, useEffect, useRef, useState } from "react";
import { useDesign } from "../hooks/useDesign";
import "../styles/knob.css";

interface KnobProps {
  label: string;
  value: number;
  normalised: number;
  onChange: (normalised: number) => void;
  onDragStart?: () => void;
  onDragEnd?: () => void;
  format?: (v: number) => string;
  unit?: string;
  large?: boolean;
  bipolar?: boolean;
}

const SIZE_NORMAL = 64;
const SIZE_LARGE = 80;
const SENSITIVITY = 220;

export function Knob({
  label,
  value,
  normalised,
  onChange,
  onDragStart,
  onDragEnd,
  format,
  unit,
  large,
  bipolar,
}: KnobProps) {
  const design = useDesign();
  const size = large ? SIZE_LARGE : SIZE_NORMAL;
  const radius = size / 2;
  const strokeWidth = large ? 3 : 2.5;
  const arcRadius = radius - strokeWidth - 2;
  // Cap inner radius — studio uses a tighter cap to make room for the
  // cardinal index dots that sit just inside the outer ring; panel
  // makes deeper room so a faceted bezel sits between the brushed cap
  // and the outer tick ring; vintage keeps the standard size since the
  // chrome+brass cap is the visual focus; modern matches the legacy size.
  const capRadius =
    design === "studio" ? arcRadius - 8
  : design === "panel"  ? arcRadius - 11
  : arcRadius - 6;

  // Panel bezel — a faceted dark skirt around the brushed cap. 8 lobes
  // alternating between outer and inner radii so the silhouette reads
  // as notched without being too gear-like.
  const panelBezelOuter = capRadius + 5;
  const panelBezelInner = capRadius + 2.5;
  const panelTickCount = 13;

  const [drag, setDrag] = useState<{ startY: number; startNorm: number } | null>(null);
  const localRef = useRef(normalised);
  useEffect(() => {
    if (drag == null) localRef.current = normalised;
  }, [normalised, drag]);

  const norm = drag ? localRef.current : normalised;

  const handlePointerDown = useCallback(
    (e: React.PointerEvent) => {
      (e.target as HTMLElement).setPointerCapture(e.pointerId);
      localRef.current = norm;
      setDrag({ startY: e.clientY, startNorm: norm });
      onDragStart?.();
    },
    [norm, onDragStart]
  );

  const handlePointerMove = useCallback(
    (e: React.PointerEvent) => {
      if (drag == null) return;
      const dy = drag.startY - e.clientY;
      const sens = e.shiftKey ? SENSITIVITY * 4 : SENSITIVITY;
      const next = Math.max(0, Math.min(1, drag.startNorm + dy / sens));
      localRef.current = next;
      onChange(next);
    },
    [drag, onChange]
  );

  const handlePointerUp = useCallback(() => {
    if (drag == null) return;
    setDrag(null);
    onDragEnd?.();
  }, [drag, onDragEnd]);

  const handleDoubleClick = useCallback(() => {
    const reset = bipolar ? 0.5 : 0;
    localRef.current = reset;
    onChange(reset);
  }, [bipolar, onChange]);

  const angle = -135 + norm * 270;

  const startAngle = bipolar ? -90 : -135;
  const arcStart = polar(arcRadius, startAngle);
  const arcEnd = polar(arcRadius, angle);
  const largeArc = Math.abs(angle - startAngle) > 180 ? 1 : 0;
  const sweep = angle > startAngle ? 1 : 0;

  const indicatorEnd = polar(arcRadius - 4, angle);
  const indicatorStart = polar(arcRadius - 14, angle);

  const display = format ? format(value) : formatDefault(value, unit);

  return (
    <div className={`knob ${large ? "knob-large" : ""}`}>
      <div
        className="knob-body"
        style={{ width: size, height: size }}
        onPointerDown={handlePointerDown}
        onPointerMove={handlePointerMove}
        onPointerUp={handlePointerUp}
        onPointerCancel={handlePointerUp}
        onDoubleClick={handleDoubleClick}
      >
        <svg width={size} height={size} viewBox={`0 0 ${size} ${size}`}>
          {/* Vintage: knurled outer ring — 12 short radial ticks just
              outside the arc, evoking a milled knob edge. */}
          {design === "vintage" && Array.from({ length: 12 }).map((_, i) => {
            const a = i * 30;
            const tickInner = polar(arcRadius - 2, a);
            const tickOuter = polar(arcRadius + 3, a);
            return (
              <line
                key={`knurl-${i}`}
                x1={radius + tickInner.x}
                y1={radius + tickInner.y}
                x2={radius + tickOuter.x}
                y2={radius + tickOuter.y}
                stroke="var(--stroke-bright)"
                strokeWidth={1}
                strokeLinecap="round"
              />
            );
          })}

          {/* Studio: 4 cardinal index dots at -135 / -45 / +45 / +135
              degrees from the top, sitting between the cap and the arc
              ring. Visual cue for "centered" / "extremes" without text. */}
          {design === "studio" && [-135, -45, 45, 135].map((a) => {
            const dot = polar(arcRadius + 2, a);
            return (
              <circle
                key={`dot-${a}`}
                cx={radius + dot.x}
                cy={radius + dot.y}
                r={1.4}
                fill="var(--stroke-bright)"
              />
            );
          })}

          {/* Panel: outer tick ring radiating outward — 13 marks across
              270°. Unipolar lights from -135° up to the current angle;
              bipolar lights from center (0°) outward toward the current
              angle, so a center-detented knob reads as "rest" with only
              the top tick highlighted. First and last ticks are longer/
              thicker to read as the rotation extremes. */}
          {design === "panel" && Array.from({ length: panelTickCount }).map((_, i) => {
            const a = -135 + (i / (panelTickCount - 1)) * 270;
            const litFrom = bipolar ? 0 : -135;
            const lo = Math.min(litFrom, angle);
            const hi = Math.max(litFrom, angle);
            const isLit = a >= lo - 0.001 && a <= hi + 0.001;
            const isExtreme = i === 0 || i === panelTickCount - 1;
            const tickLen = isExtreme ? 6 : 4;
            const tickOuter = polar(arcRadius, a);
            const tickInner = polar(arcRadius - tickLen, a);
            return (
              <line
                key={`tick-${i}`}
                x1={radius + tickInner.x}
                y1={radius + tickInner.y}
                x2={radius + tickOuter.x}
                y2={radius + tickOuter.y}
                stroke={isLit ? "var(--accent)" : "var(--tick-unlit)"}
                strokeWidth={isExtreme ? 1.8 : 1.2}
                strokeLinecap="round"
                style={isLit
                  ? { filter: "drop-shadow(0 0 2px var(--accent-glow))" }
                  : undefined}
              />
            );
          })}

          {/* Non-panel: dim outer ring + accent value arc on top. Panel
              communicates value through lit ticks, so it skips both. */}
          {design !== "panel" && (
            <>
              <circle
                cx={radius}
                cy={radius}
                r={arcRadius}
                fill="none"
                stroke="var(--stroke)"
                strokeWidth={strokeWidth}
              />
              <path
                d={`M ${radius + arcStart.x} ${radius + arcStart.y}
                    A ${arcRadius} ${arcRadius} 0 ${largeArc} ${sweep}
                      ${radius + arcEnd.x} ${radius + arcEnd.y}`}
                fill="none"
                stroke="var(--accent)"
                strokeWidth={strokeWidth}
                strokeLinecap="round"
                style={{ filter: "drop-shadow(0 0 4px var(--accent-glow))" }}
              />
            </>
          )}

          {/* Panel: faceted dark bezel skirt — 16-vertex polygon with
              alternating outer/inner radii at 22.5° steps gives 8
              subtle lobes around the brushed cap. */}
          {design === "panel" && (
            <polygon
              points={Array.from({ length: 16 }).map((_, j) => {
                const r = j % 2 === 0 ? panelBezelOuter : panelBezelInner;
                const pt = polar(r, j * 22.5);
                return `${radius + pt.x},${radius + pt.y}`;
              }).join(" ")}
              fill="var(--bezel-fill)"
              stroke="var(--bezel-edge)"
              strokeWidth={0.8}
              strokeLinejoin="round"
              style={{ filter: "drop-shadow(0 1px 2px rgba(0,0,0,0.6))" }}
            />
          )}

          {/* Cap — radialGradient stops bind to CSS variables so the
              cap auto-themes per design / theme combination without
              touching this component. */}
          <defs>
            <radialGradient id={`cap-${label}`} cx="50%" cy="35%" r="60%">
              <stop offset="0%"   style={{ stopColor: "var(--knob-cap-top)" }} />
              <stop offset="60%"  style={{ stopColor: "var(--knob-cap-mid)" }} />
              <stop offset="100%" style={{ stopColor: "var(--knob-cap-bottom)" }} />
            </radialGradient>
          </defs>
          <circle
            cx={radius}
            cy={radius}
            r={capRadius}
            fill={`url(#cap-${label})`}
            stroke={design === "panel" ? "var(--chrome-ring)" : "var(--bg-0)"}
            strokeWidth={design === "panel" ? 0.8 : 1}
          />

          {/* Panel: brushed-aluminium specular streak — narrow elliptical
              highlight at the top of the cap, gives the cone shape a
              focal sheen without painting over the radial gradient. */}
          {design === "panel" && (
            <ellipse
              cx={radius}
              cy={radius - capRadius * 0.45}
              rx={capRadius * 0.55}
              ry={capRadius * 0.12}
              fill="var(--cap-highlight)"
              opacity={0.55}
            />
          )}

          {/* Panel: bright indicator dot sitting on the faceted bezel
              rim at the current value's angle. Reads as the rotation
              pointer the way an inset white pip does on hardware knobs. */}
          {design === "panel" && (() => {
            const dot = polar(panelBezelOuter - 1.8, angle);
            return (
              <circle
                cx={radius + dot.x}
                cy={radius + dot.y}
                r={1.8}
                fill="var(--indicator-dot)"
                style={{ filter: "drop-shadow(0 0 2px rgba(255,255,255,0.4))" }}
              />
            );
          })()}

          {design !== "panel" && (
            <line
              x1={radius + indicatorStart.x}
              y1={radius + indicatorStart.y}
              x2={radius + indicatorEnd.x}
              y2={radius + indicatorEnd.y}
              stroke="var(--accent)"
              strokeWidth={2}
              strokeLinecap="round"
              style={{ filter: "drop-shadow(0 0 3px var(--accent-glow))" }}
            />
          )}

          {/* Vintage: brass dot at the indicator tip (the chrome cap
              gets a warm focal accent that ties to the brass details
              elsewhere). */}
          {design === "vintage" && (
            <circle
              cx={radius + indicatorEnd.x}
              cy={radius + indicatorEnd.y}
              r={2}
              fill="var(--brass-bright)"
              stroke="var(--brass-shadow)"
              strokeWidth={0.5}
            />
          )}
        </svg>
      </div>
      <div className="knob-label">{label}</div>
      <div className="knob-value">{display}</div>
    </div>
  );
}

function polar(r: number, deg: number) {
  const rad = ((deg - 90) * Math.PI) / 180;
  return { x: r * Math.cos(rad), y: r * Math.sin(rad) };
}

function formatDefault(v: number, unit?: string): string {
  if (v === undefined || v === null || Number.isNaN(v)) return "--";
  const abs = Math.abs(v);
  let str: string;
  if (abs >= 1000) str = v.toFixed(0);
  else if (abs >= 100) str = v.toFixed(1);
  else if (abs >= 10) str = v.toFixed(2);
  else str = v.toFixed(3);
  return unit ? `${str} ${unit}` : str;
}
