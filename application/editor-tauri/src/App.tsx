import type { ReactNode } from "react";
import { Toolbar } from "./components/Toolbar";
import { Hierarchy } from "./components/Hierarchy";
import { Inspector } from "./components/Inspector";
import { Console } from "./components/Console";
import { KeyframePanel } from "./components/KeyframePanel";
import { SceneView } from "./components/SceneView";
import { PostProcessPanel } from "./components/PostProcessPanel";
import { DockProvider, DockLayout, PanelId } from "./components/dock";

function AppContent() {
  const panelComponents: Record<PanelId, ReactNode> = {
    sceneView: <SceneView />,
    hierarchy: <Hierarchy />,
    inspector: <Inspector />,
    console: <Console />,
    keyframe: <KeyframePanel />,
    postprocess: <PostProcessPanel />,
    imageViewer: (
      <div className="panel">
        <div className="panel-header">
          <span className="panel-title">Image Viewer</span>
        </div>
        <div className="panel-content">
          <div className="empty-state">
            <div className="empty-state-icon">🖼️</div>
            <div className="empty-state-text">No image selected</div>
          </div>
        </div>
      </div>
    ),
  };

  return (
    <div className="app">
      <Toolbar />
      <DockLayout children={panelComponents} />

      <div className="status-bar">
        <div className="status-bar-group">
          <div className="status-bar-section">
            <span className="status-bar-label">Status</span>
            <span className="status-bar-value">Ready</span>
          </div>
        </div>

        <div className="status-bar-group">
          <div className="status-bar-section status-bar-progress">
            <div className="status-bar-progress-bar-container">
              <div className="status-bar-progress-bar" style={{ width: "45%" }} />
            </div>
            <span className="status-bar-progress-text">45%</span>
          </div>
        </div>

        <div className="status-bar-group">
          <div className="status-bar-section">
            <span className="status-bar-value">Step 1200 / 30000</span>
          </div>
        </div>

        <div className="status-bar-group">
          <div className="status-bar-section">
            <span className="status-bar-label">Loss</span>
            <span className="status-bar-value">0.124</span>
          </div>
        </div>

        <div className="status-bar-group">
          <div className="status-bar-section">
            <span className="status-bar-value" style={{ color: "#5cb85c" }}>
              NVIDIA GeForce RTX 4090 · 8.2/24.0GB
            </span>
          </div>
        </div>
      </div>
    </div>
  );
}

export default function App() {
  return (
    <DockProvider>
      <AppContent />
    </DockProvider>
  );
}
