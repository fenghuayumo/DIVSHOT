import React, { ReactNode } from 'react'
import { useDock, LayoutNode, PanelId } from './DockContext'
import { DockPanel } from './DockPanel'
import { FloatingWindow } from './FloatingWindow'
import { SplitPane } from './SplitPane'

interface DockLayoutProps {
  children: Record<PanelId, ReactNode>
}

export function DockLayout({ children }: DockLayoutProps) {
  const { state, dragState, endDrag, dropPanel } = useDock()

  const renderLayoutNode = (node: LayoutNode): ReactNode => {
    if (node.type === 'tabs') {
      return (
        <DockPanel
          tabs={node.tabs}
          activeTab={node.activeTab}
          children={children}
        />
      )
    }

    return (
      <SplitPane
        direction={node.direction}
        initialRatio={node.ratio}
        first={renderLayoutNode(node.first)}
        second={renderLayoutNode(node.second)}
      />
    )
  }

  const handleBackgroundDrop = (e: React.DragEvent) => {
    e.preventDefault()
    if (dragState.panelId) {
      // Drop as floating window
      dropPanel(null, 'float')
    }
  }

  const handleBackgroundDragOver = (e: React.DragEvent) => {
    e.preventDefault()
    e.dataTransfer.dropEffect = 'move'
  }

  return (
    <div 
      className="dock-layout"
      onDrop={handleBackgroundDrop}
      onDragOver={handleBackgroundDragOver}
    >
      {/* Main docked layout */}
      <div className="dock-main">
        {renderLayoutNode(state.layout)}
      </div>

      {/* Floating windows */}
      {state.floatingWindows.map(window => (
        <FloatingWindow
          key={window.id}
          window={window}
          children={children}
        />
      ))}

      {/* Drag overlay */}
      {dragState.panelId && (
        <div className="dock-drag-overlay" onDragEnd={endDrag} />
      )}
    </div>
  )
}
