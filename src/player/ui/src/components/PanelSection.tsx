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
        {children}
      </div>
    </section>
  );
}
