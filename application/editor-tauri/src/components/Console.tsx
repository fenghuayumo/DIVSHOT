const MOCK_LOGS = [
  { type: "success", time: "10:24:01", message: "DIVSHOT editor shell initialized" },
  { type: "info", time: "10:24:02", message: "Waiting for divshot-engine sidecar..." },
  { type: "info", time: "10:24:03", message: "Scene viewport ready (native attach pending)" },
];

export function Console() {
  return (
    <div className="console-panel">
      <div className="console-toolbar">
        <button type="button" className="console-filter active">
          All (3)
        </button>
        <button type="button" className="console-filter">
          Info (2)
        </button>
        <button type="button" className="console-filter">
          Warnings (0)
        </button>
        <button type="button" className="console-filter">
          Errors (0)
        </button>
        <div style={{ flex: 1 }} />
        <button type="button" className="console-filter">
          🗑 Clear
        </button>
      </div>

      <div className="console-content">
        {MOCK_LOGS.map((entry) => (
          <div key={entry.time} className={`console-entry ${entry.type}`}>
            <span className="console-entry-icon">
              {entry.type === "success" ? "✓" : "ℹ"}
            </span>
            <span className="console-entry-time">{entry.time}</span>
            <span className="console-entry-message">{entry.message}</span>
          </div>
        ))}
      </div>
    </div>
  );
}
