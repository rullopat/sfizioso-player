import { useState } from "react";
import { callNative } from "@shared/juceBridge";
import { FN_GET_RECENT, FN_LOAD_SFZ_PATH } from "../paramIds";
import "../styles/topbar.css";

interface RecentFile {
  path: string;
  fileName: string;
}

interface Props {
  productName: string;
  sampleName: string;
  meta: string;
  onLoad: () => void;
}

export function TopBar({ productName, sampleName, meta, onLoad }: Props) {
  const [menuOpen, setMenuOpen] = useState(false);
  const [recent, setRecent] = useState<RecentFile[]>([]);

  const toggleMenu = async () => {
    if (menuOpen) {
      setMenuOpen(false);
      return;
    }
    const list = await callNative<RecentFile[]>(FN_GET_RECENT);
    setRecent(list ?? []);
    setMenuOpen(true);
  };

  const pick = (path: string) => {
    setMenuOpen(false);
    callNative(FN_LOAD_SFZ_PATH, path); // status re-hydrates via the sfzLoaded event
  };

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
        <span className="logo-text">{productName.toUpperCase()}</span>
      </div>

      <div className="topbar-center">
        <div className="sample-slot-wrap">
          <button className="sample-slot" onClick={onLoad}>
            <span className="sample-slot-label">SFZ</span>
            <span className="sample-slot-name">{sampleName}</span>
          </button>
          <button
            className="sample-slot-caret"
            title="Recent files"
            onClick={toggleMenu}
          >
            ▾
          </button>

          {menuOpen && (
            <div className="recent-menu" onMouseLeave={() => setMenuOpen(false)}>
              {recent.length === 0 ? (
                <div className="recent-empty">No recent files</div>
              ) : (
                recent.map((r) => (
                  <button key={r.path} className="recent-row" onClick={() => pick(r.path)}>
                    <span className="recent-name">{r.fileName}</span>
                    <span className="recent-dir">{r.path}</span>
                  </button>
                ))
              )}
            </div>
          )}
        </div>
      </div>

      <div className="topbar-right">
        <span>METADATA</span>
        <span className="meta-line">{meta}</span>
      </div>
    </header>
  );
}
