function VectorRow({ label }: { label: string }) {
  const axes = ["x", "y", "z"] as const;
  const values = label === "Scale" ? ["1.000", "1.000", "1.000"] : ["0.000", "0.000", "0.000"];

  return (
    <div className="inspector-row">
      <span className="inspector-label">{label}</span>
      <div className="inspector-vector">
        {axes.map((axis, index) => (
          <input
            key={`${label}-${axis}`}
            className={`inspector-vector-input ${axis}`}
            value={values[index]}
            readOnly
          />
        ))}
      </div>
    </div>
  );
}

export function Inspector() {
  return (
    <div className="panel inspector-panel">
      <div className="panel-header">
        <span className="panel-title">Inspector</span>
      </div>

      <div className="panel-content">
        <div className="inspector-section">
          <div className="inspector-section-header">
            <span className="inspector-section-arrow">▼</span>
            <span className="inspector-section-icon">🧊</span>
            <span className="inspector-section-title">Transform</span>
          </div>
          <div className="inspector-section-content">
            <VectorRow label="Position" />
            <VectorRow label="Rotation" />
            <VectorRow label="Scale" />
          </div>
        </div>

        <div className="inspector-section">
          <div className="inspector-section-header">
            <span className="inspector-section-arrow">▼</span>
            <span className="inspector-section-icon">✨</span>
            <span className="inspector-section-title">Gaussian Splat</span>
          </div>
          <div className="inspector-section-content">
            <div className="inspector-row">
              <span className="inspector-label">Count</span>
              <span style={{ color: "var(--text-primary)", fontSize: "var(--font-size-sm)" }}>
                1,234,567
              </span>
            </div>
            <div className="inspector-row">
              <span className="inspector-label">Opacity</span>
              <input type="range" defaultValue={100} style={{ flex: 1 }} readOnly />
            </div>
          </div>
        </div>
      </div>
    </div>
  );
}
