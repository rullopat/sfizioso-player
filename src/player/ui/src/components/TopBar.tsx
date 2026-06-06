import "../styles/topbar.css";

interface Props {
  sampleName: string;
  meta: string;
  onLoad: () => void;
}

export function TopBar({ sampleName, meta, onLoad }: Props) {
  return (
    <header className="topbar">
      <div className="topbar-left">
        <div className="logo-mark">
          <svg width="18" height="18" viewBox="0 0 24 24" fill="none">
            <path
              d="M3 12 L8 6 L8 18 L13 8 L13 16 L18 4 L18 20 L21 12"
              stroke="currentColor"
              strokeWidth="1.5"
              strokeLinecap="round"
              strokeLinejoin="round"
            />
          </svg>
        </div>
        <span className="logo-text">SAMPLE MACHINE PLAYER</span>
      </div>

      <div className="topbar-center">
        <button className="sample-slot" onClick={onLoad}>
          <span className="sample-slot-label">SFZ</span>
          <span className="sample-slot-name">{sampleName}</span>
          <span className="sample-slot-icon">▾</span>
        </button>
      </div>

      <div className="topbar-right">
        <span>METADATA</span>
        <span className="meta-line">{meta}</span>
      </div>
    </header>
  );
}
