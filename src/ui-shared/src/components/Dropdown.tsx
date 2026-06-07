import "../styles/dropdown.css";

interface Props {
  label: string;
  value: number; // selected index
  options: string[];
  onChange: (index: number) => void;
}

// Themed combobox (styled native <select>) for discrete engine settings.
export function Dropdown({ label, value, options, onChange }: Props) {
  return (
    <div className="dropdown">
      <div className="dropdown-field">
        <select
          className="dropdown-select"
          value={value}
          onChange={(e) => onChange(Number(e.target.value))}
        >
          {options.length === 0 ? (
            <option value={value}>{value}</option>
          ) : (
            options.map((o, i) => (
              <option key={i} value={i}>
                {o}
              </option>
            ))
          )}
        </select>
        <span className="dropdown-caret">▾</span>
      </div>
      <span className="dropdown-label">{label}</span>
    </div>
  );
}
