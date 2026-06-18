export function KeyframePanel() {
  return (
    <div className="panel inspector-panel">
      <div className="panel-header">
        <span className="panel-title">KeyFrame</span>
      </div>
      <div className="panel-content">
        <div className="empty-state">
          <div className="empty-state-icon">🎬</div>
          <div className="empty-state-text">No keyframes in timeline</div>
          <div className="empty-state-hint">Add camera keyframes to animate the scene</div>
        </div>
      </div>
    </div>
  );
}
