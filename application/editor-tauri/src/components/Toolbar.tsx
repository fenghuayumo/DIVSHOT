/** Static menubar + toolbar shell (visual only, aligned with rfs web-ui). */

const MENU_ITEMS = ["File", "Edit", "View", "Splat", "Help"];

export function Toolbar() {
  return (
    <>
      <div className="menubar">
        {MENU_ITEMS.map((label) => (
          <div key={label} className="menu-item">
            {label}
          </div>
        ))}
        <div style={{ flex: 1 }} />
        <div
          style={{
            fontSize: "var(--font-size-sm)",
            color: "var(--text-secondary)",
            padding: "0 8px",
          }}
        >
          Untitled.dvs
        </div>
      </div>

      <div className="toolbar">
        <div className="toolbar-section">
          <button type="button" className="toolbar-button" title="New">
            📄
          </button>
          <button type="button" className="toolbar-button" title="Open">
            📂
          </button>
          <button type="button" className="toolbar-button" title="Save">
            💾
          </button>
        </div>

        <div className="toolbar-separator" />

        <div className="toolbar-section">
          <button type="button" className="toolbar-button" title="Undo">
            ↶
          </button>
          <button type="button" className="toolbar-button" title="Redo">
            ↷
          </button>
        </div>

        <div className="toolbar-separator" />

        <div className="toolbar-section">
          <button type="button" className="toolbar-button" title="Focus">
            🎯
          </button>
        </div>

        <div className="toolbar-separator" />

        <div className="toolbar-section">
          <button type="button" className="toolbar-button" title="Render2Splats">
            📷
          </button>
          <button type="button" className="toolbar-button" title="Splat Edit">
            ✏️
          </button>
        </div>

        <div style={{ flex: 0.5 }} />

        <div
          className="toolbar-section training-controls"
          style={{
            display: "flex",
            alignItems: "center",
            gap: "8px",
            padding: "4px 12px",
            background: "rgba(59, 130, 246, 0.15)",
            borderRadius: "6px",
            border: "1px solid rgba(59, 130, 246, 0.3)",
          }}
        >
          <button
            type="button"
            className="toolbar-button"
            style={{
              background: "#22c55e",
              color: "#fff",
              padding: "4px 8px",
              borderRadius: "4px",
              fontSize: "14px",
              minWidth: "28px",
            }}
          >
            ▶
          </button>
          <button
            type="button"
            className="toolbar-button"
            style={{
              background: "#8b5cf6",
              color: "#fff",
              padding: "4px 8px",
              borderRadius: "4px",
              fontSize: "12px",
              minWidth: "28px",
            }}
          >
            🔄
          </button>
          <button
            type="button"
            className="toolbar-button"
            style={{
              background: "#ef4444",
              color: "#fff",
              padding: "4px 8px",
              borderRadius: "4px",
              fontSize: "12px",
              minWidth: "28px",
            }}
          >
            🗑️
          </button>
        </div>

        <div style={{ flex: 1 }} />

        <div
          className="toolbar-section"
          style={{
            marginRight: "8px",
            padding: "0 12px",
            display: "flex",
            alignItems: "center",
            gap: "6px",
            background: "rgba(0, 0, 0, 0.2)",
            borderRadius: "4px",
            fontSize: "var(--font-size-sm)",
            fontFamily: "monospace",
            userSelect: "none",
          }}
        >
          <span style={{ color: "var(--text-secondary)", fontSize: "11px", fontWeight: 500 }}>
            FPS
          </span>
          <span
            style={{
              color: "#4ade80",
              fontSize: "13px",
              fontWeight: "bold",
              minWidth: "32px",
              textAlign: "right",
            }}
          >
            60
          </span>
        </div>

        <div className="toolbar-section">
          <button type="button" className="toolbar-button" title="DevTools" style={{ fontSize: "10px" }}>
            🛠
          </button>
        </div>
      </div>
    </>
  );
}
