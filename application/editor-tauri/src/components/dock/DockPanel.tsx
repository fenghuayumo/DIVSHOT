import React, { useState, useRef, ReactNode } from 'react'
import { useDock, PanelId, DropZone } from './DockContext'
import { DockTabBar } from './DockTabBar'

interface DockPanelProps {
  tabs: PanelId[]
  activeTab: PanelId
  children: Record<PanelId, ReactNode>
  windowId?: string
  onClose?: () => void
}

export function DockPanel({ tabs, activeTab, children, windowId, onClose }: DockPanelProps) {
  const { setActiveTab, dragState, dropPanel } = useDock()
  const [dropZone, setDropZone] = useState<DropZone | null>(null)
  const panelRef = useRef<HTMLDivElement>(null)

  const handleTabClick = (tabId: PanelId) => {
    setActiveTab(tabId)
  }

  const handleDragOver = (e: React.DragEvent) => {
    e.preventDefault()
    e.stopPropagation()
    
    if (!panelRef.current || !dragState.panelId) return
    
    const rect = panelRef.current.getBoundingClientRect()
    const x = e.clientX - rect.left
    const y = e.clientY - rect.top
    const w = rect.width
    const h = rect.height

    // Determine drop zone based on position (edges vs center)
    const edgeSize = 60
    if (x < edgeSize) setDropZone('left')
    else if (x > w - edgeSize) setDropZone('right')
    else if (y < edgeSize) setDropZone('top')
    else if (y > h - edgeSize) setDropZone('bottom')
    else setDropZone('center')
  }

  const handleDragLeave = (e: React.DragEvent) => {
    // Only clear if leaving the panel entirely
    const rect = panelRef.current?.getBoundingClientRect()
    if (rect) {
      const x = e.clientX
      const y = e.clientY
      if (x < rect.left || x > rect.right || y < rect.top || y > rect.bottom) {
        setDropZone(null)
      }
    }
  }

  const handleDrop = (e: React.DragEvent) => {
    e.preventDefault()
    e.stopPropagation()
    
    if (dropZone && dragState.panelId) {
      dropPanel(activeTab, dropZone, windowId)
    }
    setDropZone(null)
  }

  // Check if this panel contains scene view for transparent background
  const isScenePanel = tabs.includes('sceneView')
  
  // Build class names
  const classNames = [
    'dock-panel',
    dropZone ? 'has-drop-zone' : '',
    isScenePanel ? 'scene-panel' : '',
  ].filter(Boolean).join(' ')

  return (
    <div 
      ref={panelRef}
      className={classNames}
      onDragOver={handleDragOver}
      onDragLeave={handleDragLeave}
      onDrop={handleDrop}
    >
      <DockTabBar
        tabs={tabs}
        activeTab={activeTab}
        windowId={windowId}
        onTabClick={handleTabClick}
        onClose={onClose}
      />
      
      <div className="dock-panel-content">
        {children[activeTab]}
      </div>
      
      {/* Drop zone overlay */}
      {dropZone && dragState.panelId && (
        <div className={`dock-drop-overlay ${dropZone}`}>
          <div className="dock-drop-highlight" />
        </div>
      )}
    </div>
  )
}
