import "../styles/segmented.css";

interface Option {
  label: string;
  value: number;
}

interface Props {
  label: string;
  options: Option[];
  selected: number;            // index into options
  onSelect: (index: number) => void;
}

export function Segmented({ label, options, selected, onSelect }: Props) {
  return (
    <div className="segmented">
      <div className="segmented-track">
        {options.map((opt, idx) => {
          const isActive = idx === selected;
          return (
            <button
              key={opt.value}
              className={`segmented-option ${isActive ? "is-active" : ""}`}
              onClick={() => onSelect(idx)}
            >
              {opt.label}
            </button>
          );
        })}
      </div>
      <div className="segmented-label">{label}</div>
    </div>
  );
}
