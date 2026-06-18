import React, { useRef, useState, useEffect } from 'react'
import { useDock, PanelId, DropZone } from './DockContext'

interface DockTabBarProps {
  tabs: PanelId[]
  activeTab: PanelId
  windowId?: string
  onTabClick: (tabId: PanelId) => void
  onClose?: () => void
}

export function DockTabBar({ tabs, activeTab, windowId, onTabClick, onClose }: DockTabBarProps) {
  const { panelConfigs, startDrag, endDrag, dropPanel, closePanel, dragState } = useDock()
  const [dropZone, setDropZone] = useState<DropZone | null>(null)
  const tabBarRef = useRef<HTMLDivElement>(null)
  
  // 拖拽状态
  const dragInfoRef = useRef<{
    tabId: PanelId | null
    startX: number
    startY: number
    isDragging: boolean
  }>({ tabId: null, startX: 0, startY: 0, isDragging: false })

  const handleMouseDown = (e: React.MouseEvent, tabId: PanelId) => {
    // 忽略关闭按钮
    if ((e.target as HTMLElement).closest('.dock-tab-close')) {
      return
    }
    
    e.preventDefault()
    dragInfoRef.current = {
      tabId,
      startX: e.clientX,
      startY: e.clientY,
      isDragging: false,
    }
  }

  useEffect(() => {
    const handleMouseMove = (e: MouseEvent) => {
      const info = dragInfoRef.current
      if (!info.tabId) return

      const dx = Math.abs(e.clientX - info.startX)
      const dy = Math.abs(e.clientY - info.startY)

      // 如果移动超过阈值，开始拖拽
      if (!info.isDragging && (dx > 5 || dy > 5)) {
        info.isDragging = true
        startDrag(info.tabId, windowId ? 'floating' : 'docked', windowId)
      }
    }

    const handleMouseUp = (_e: MouseEvent) => {
      const info = dragInfoRef.current
      if (!info.tabId) return

      if (!info.isDragging) {
        onTabClick(info.tabId);
      } else if (dragState.panelId) {
        endDrag();
      }

      dragInfoRef.current = { tabId: null, startX: 0, startY: 0, isDragging: false };
    };

    document.addEventListener('mousemove', handleMouseMove)
    document.addEventListener('mouseup', handleMouseUp)

    return () => {
      document.removeEventListener('mousemove', handleMouseMove)
      document.removeEventListener('mouseup', handleMouseUp)
    }
  }, [windowId, startDrag, endDrag, onTabClick, dragState.panelId])

  // HTML5 拖拽事件（用于接收拖拽）
  const handleDragOver = (e: React.DragEvent) => {
    e.preventDefault()
    e.dataTransfer.dropEffect = 'move'
    
    if (!tabBarRef.current || !dragState.panelId) return
    
    const rect = tabBarRef.current.getBoundingClientRect()
    const x = e.clientX - rect.left
    const y = e.clientY - rect.top
    const w = rect.width
    const h = rect.height

    const margin = 30
    if (x < margin) setDropZone('left')
    else if (x > w - margin) setDropZone('right')
    else if (y < margin) setDropZone('top')
    else if (y > h - margin) setDropZone('bottom')
    else setDropZone('center')
  }

  const handleDragLeave = () => {
    setDropZone(null)
  }

  const handleDrop = (e: React.DragEvent) => {
    e.preventDefault()
    if (dropZone && dragState.panelId) {
      dropPanel(activeTab, dropZone, windowId)
    }
    setDropZone(null)
  }

  const handleTabClose = (e: React.MouseEvent, tabId: PanelId) => {
    e.stopPropagation()
    e.preventDefault()
    closePanel(tabId)
  }

  return (
    <div 
      ref={tabBarRef}
      className={`dock-tab-bar ${dropZone ? `drop-zone-${dropZone}` : ''}`}
      onDragOver={handleDragOver}
      onDragLeave={handleDragLeave}
      onDrop={handleDrop}
    >
      <div className="dock-tabs">
        {tabs.map(tabId => {
          const config = panelConfigs[tabId]
          const isActive = activeTab === tabId
          return (
            <div
              key={tabId}
              className={`dock-tab ${isActive ? 'active' : ''} ${dragState.panelId === tabId ? 'dragging' : ''}`}
              onMouseDown={(e) => handleMouseDown(e, tabId)}
            >
              <span className="dock-tab-icon">{config.icon}</span>
              <span className="dock-tab-title">{config.title}</span>
              {config.closable && (
                <button 
                  className="dock-tab-close"
                  onClick={(e) => handleTabClose(e, tabId)}
                  title={`Close ${config.title}`}
                >
                  ×
                </button>
              )}
            </div>
          )
        })}
      </div>
      
      {onClose && (
        <div className="dock-tab-bar-actions">
          <button className="dock-tab-bar-close" onClick={onClose} title="Close Window">
            ×
          </button>
        </div>
      )}
      
      {dropZone && dragState.panelId && (
        <div className={`dock-drop-indicator ${dropZone}`} />
      )}
    </div>
  )
}
