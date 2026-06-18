const MOCK_ENTITIES = [
  { id: 1, name: "Scene Root", type: "empty", depth: 0, expanded: true },
  { id: 2, name: "Gaussian Splat", type: "gaussian_splat", depth: 1, expanded: false },
  { id: 3, name: "Main Camera", type: "camera", depth: 1, expanded: false },
  { id: 4, name: "Directional Light", type: "light", depth: 1, expanded: false },
];

const iconClass: Record<string, string> = {
  empty: "empty",
  gaussian_splat: "gaussian_splat",
  camera: "camera",
  light: "light",
};

const iconEmoji: Record<string, string> = {
  empty: "⬜",
  gaussian_splat: "✨",
  camera: "📷",
  light: "☀️",
};

export function Hierarchy() {
  return (
    <div className="panel hierarchy-panel">
      <div className="panel-header">
        <span className="panel-title">Hierarchy</span>
      </div>

      <div className="search-container">
        <div className="search-wrapper">
          <span className="search-icon">🔍</span>
          <input className="search-input" placeholder="Search entities..." readOnly />
        </div>
      </div>

      <div className="panel-content">
        <div className="hierarchy-list">
          {MOCK_ENTITIES.map((entity, index) => (
            <div key={entity.id} className="hierarchy-item-wrapper">
              <div
                className={`hierarchy-item${index === 1 ? " selected" : ""}`}
                style={{ paddingLeft: 8 + entity.depth * 16 }}
              >
                <span className="hierarchy-expand">{entity.depth === 0 ? "▼" : " "}</span>
                <span className={`hierarchy-icon ${iconClass[entity.type]}`}>
                  {iconEmoji[entity.type]}
                </span>
                <span className="hierarchy-name">{entity.name}</span>
                <span className="hierarchy-visibility">👁</span>
              </div>
            </div>
          ))}
        </div>
      </div>
    </div>
  );
}
