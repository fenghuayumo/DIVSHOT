/** Transparent viewport shell with overlay controls (visual only). */

export function SceneView() {
  return (
    <div className="scene-view" id="viewport-slot">
      <div className="scene-view-overlay">
        <div className="viewport-toolbar-container">
          <div className="viewport-toolbar viewport-toolbar-left">
            <div className="viewport-dropdown">
              <button type="button" className="viewport-dropdown-button">
                <span className="dropdown-icon">💡</span>
                <span className="dropdown-text">Lit</span>
                <span className="dropdown-arrow">▼</span>
              </button>
            </div>
            <div className="viewport-dropdown">
              <button type="button" className="viewport-dropdown-button">
                <span className="dropdown-icon">👁</span>
                <span className="dropdown-text">Show</span>
                <span className="dropdown-arrow">▼</span>
              </button>
            </div>
          </div>
        </div>

        <div className="viewport-splat-info">
          <span className="viewport-splat-info-label">Splats</span>
          <span className="viewport-splat-info-value">1.23M</span>
        </div>

        <div className="empty-scene-dropzone">
          <div className="empty-scene-dropzone-content">
            <div className="empty-scene-icon">📦</div>
            <div className="empty-scene-title">Drop files to import</div>
            <div className="empty-scene-hint">.ply · .splat · .dvs · images</div>
          </div>
        </div>

        <div
          style={{
            position: "absolute",
            right: 12,
            bottom: 12,
            width: 96,
            height: 96,
            borderRadius: 8,
            border: "1px solid var(--border-color)",
            background: "rgba(30, 30, 30, 0.75)",
            display: "flex",
            alignItems: "center",
            justifyContent: "center",
            color: "var(--text-muted)",
            fontSize: 11,
            pointerEvents: "auto",
          }}
        >
          Gizmo
        </div>
      </div>
    </div>
  );
}
