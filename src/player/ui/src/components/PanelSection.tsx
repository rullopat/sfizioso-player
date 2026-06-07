import { ReactNode } from "react";
import "../styles/panel.css";

interface Props {
  /** Uppercase section label, e.g. "CONTROLS". */
  title: string;
  /**
   * grid-area name this section occupies in the shell layout. Omit for
   * sections nested inside a flex container (e.g. the centre stack), where
   * grid placement does not apply.
   */
  area?: string;
  /**
   * When true, the section renders an empty "coming soon" placeholder body
   * instead of children — used by the parity panels that aren't built yet.
   */
  placeholder?: boolean;
  /** One-line feature description shown under the placeholder label. */
  hint?: string;
  className?: string;
  children?: ReactNode;
}

/**
 * SMPL-82 — the framed card primitive for the player's multi-panel shell.
 *
 * Mirrors the shared `ParamModule` aesthetic (header title + hairline rule on
 * the same design tokens) but is grid-friendly: no 480px min-width, fills its
 * grid cell, and scrolls its body. `ParamModule` stays the right choice for a
 * fixed cluster of knobs; this wrapper is for the responsive shell slots.
 */
export function PanelSection({
  title,
  area,
  placeholder,
  hint,
  className,
  children,
}: Props) {
  return (
    <section
      className={`panel${className ? ` ${className}` : ""}`}
      style={area ? { gridArea: area } : undefined}
    >
      <div className="panel-header">
        <span className="panel-title">{title}</span>
        <span className="panel-rule" />
      </div>
      <div className="panel-body">
        {placeholder ? (
          <div className="panel-placeholder">
            <span className="panel-placeholder-code">COMING SOON</span>
            {hint ? <span className="panel-placeholder-hint">{hint}</span> : null}
          </div>
        ) : (
          children
        )}
      </div>
    </section>
  );
}
