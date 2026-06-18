export function PostProcessPanel() {
  return (
    <div className="panel inspector-panel">
      <div className="panel-header">
        <span className="panel-title">Post Process</span>
      </div>
      <div className="panel-content">
        <div className="inspector-section">
          <div className="inspector-section-header">
            <span className="inspector-section-arrow">▼</span>
            <span className="inspector-section-icon">🎨</span>
            <span className="inspector-section-title">Tone Mapping</span>
          </div>
          <div className="inspector-section-content">
            <div className="inspector-row">
              <span className="inspector-label">Exposure</span>
              <input className="inspector-vector-input" value="1.00" readOnly style={{ flex: 1 }} />
            </div>
            <div className="inspector-row">
              <span className="inspector-label">Gamma</span>
              <input className="inspector-vector-input" value="2.20" readOnly style={{ flex: 1 }} />
            </div>
          </div>
        </div>
        <div className="inspector-section">
          <div className="inspector-section-header">
            <span className="inspector-section-arrow">▼</span>
            <span className="inspector-section-icon">✨</span>
            <span className="inspector-section-title">Bloom</span>
          </div>
          <div className="inspector-section-content">
            <div className="inspector-row">
              <span className="inspector-label">Intensity</span>
              <input type="range" defaultValue={35} style={{ flex: 1 }} readOnly />
            </div>
          </div>
        </div>
      </div>
    </div>
  );
}
