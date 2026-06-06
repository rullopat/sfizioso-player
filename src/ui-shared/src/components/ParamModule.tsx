import { ReactNode } from "react";
import "../styles/module.css";

interface Props {
  title: string;
  children: ReactNode;
}

export function ParamModule({ title, children }: Props) {
  return (
    <div className="module">
      <div className="module-header">
        <span className="module-title">{title}</span>
        <span className="module-rule" />
      </div>
      <div className="module-body">{children}</div>
    </div>
  );
}
