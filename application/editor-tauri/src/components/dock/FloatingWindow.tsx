import React, { useRef, useState, useCallback, useEffect, ReactNode } from 'react'
import { useDock, FloatingWindow as FloatingWindowType, PanelId } from './DockContext'
import { DockPanel } from './DockPanel'

interface FloatingWindowProps {
  window: FloatingWindowType
  children: Record<PanelId, ReactNode>
}

export function FloatingWindow({ window, children }: FloatingWindowProps) {
  const { moveFloatingWindow, resizeFloatingWindow, bringToFront, closeFloatingWindow } = useDock()
  const windowRef = useRef<HTMLDivElement>(null)
  const [isDragging, setIsDragging] = useState(false)
  const [isResizing, setIsResizing] = useState(false)
  const [resizeEdge, setResizeEdge] = useState<string | null>(null)
  const dragOffset = useRef({ x: 0, y: 0 })
  const resizeStart = useRef({ x: 0, y: 0, width: 0, height: 0 })

  const handleMouseDown = useCallback((e: React.MouseEvent) => {
    if ((e.target as HTMLElement).closest('.dock-tab-bar')) {
      setIsDragging(true)
      dragOffset.current = {
        x: e.clientX - window.x,
        y: e.clientY - window.y,
      }
      bringToFront(window.id)
    }
  }, [window.id, window.x, window.y, bringToFront])

  const handleResizeMouseDown = useCallback((e: React.MouseEvent, edge: string) => {
    e.stopPropagation()
    setIsResizing(true)
    setResizeEdge(edge)
    resizeStart.current = {
      x: e.clientX,
      y: e.clientY,
      width: window.width,
      height: window.height,
    }
    bringToFront(window.id)
  }, [window.id, window.width, window.height, bringToFront])

  useEffect(() => {
    const handleMouseMove = (e: MouseEvent) => {
      if (isDragging) {
        const newX = Math.max(0, Math.min(globalThis.innerWidth - 100, e.clientX - dragOffset.current.x))
        const newY = Math.max(0, Math.min(globalThis.innerHeight - 100, e.clientY - dragOffset.current.y))
        moveFloatingWindow(window.id, newX, newY)
      }
      
      if (isResizing && resizeEdge) {
        const dx = e.clientX - resizeStart.current.x
        const dy = e.clientY - resizeStart.current.y
        
        let newWidth = resizeStart.current.width
        let newHeight = resizeStart.current.height
        
        if (resizeEdge.includes('e')) newWidth = resizeStart.current.width + dx
        if (resizeEdge.includes('w')) newWidth = resizeStart.current.width - dx
        if (resizeEdge.includes('s')) newHeight = resizeStart.current.height + dy
        if (resizeEdge.includes('n')) newHeight = resizeStart.current.height - dy
        
        resizeFloatingWindow(window.id, newWidth, newHeight)
      }
    }

    const handleMouseUp = () => {
      setIsDragging(false)
      setIsResizing(false)
      setResizeEdge(null)
    }

    if (isDragging || isResizing) {
      document.addEventListener('mousemove', handleMouseMove)
      document.addEventListener('mouseup', handleMouseUp)
      document.body.style.cursor = isDragging ? 'move' : 
        resizeEdge?.includes('n') && resizeEdge?.includes('e') ? 'ne-resize' :
        resizeEdge?.includes('n') && resizeEdge?.includes('w') ? 'nw-resize' :
        resizeEdge?.includes('s') && resizeEdge?.includes('e') ? 'se-resize' :
        resizeEdge?.includes('s') && resizeEdge?.includes('w') ? 'sw-resize' :
        resizeEdge?.includes('n') || resizeEdge?.includes('s') ? 'ns-resize' :
        'ew-resize'
      document.body.style.userSelect = 'none'
    }

    return () => {
      document.removeEventListener('mousemove', handleMouseMove)
      document.removeEventListener('mouseup', handleMouseUp)
      document.body.style.cursor = ''
      document.body.style.userSelect = ''
    }
  }, [isDragging, isResizing, resizeEdge, window.id, moveFloatingWindow, resizeFloatingWindow])

  const handleClose = () => {
    closeFloatingWindow(window.id)
  }

  return (
    <div
      ref={windowRef}
      className={`floating-window ${isDragging ? 'dragging' : ''}`}
      style={{
        left: window.x,
        top: window.y,
        width: window.width,
        height: window.height,
        zIndex: window.zIndex,
      }}
      onMouseDown={handleMouseDown}
    >
      {/* Resize handles */}
      <div className="resize-handle-n" onMouseDown={(e) => handleResizeMouseDown(e, 'n')} />
      <div className="resize-handle-s" onMouseDown={(e) => handleResizeMouseDown(e, 's')} />
      <div className="resize-handle-e" onMouseDown={(e) => handleResizeMouseDown(e, 'e')} />
      <div className="resize-handle-w" onMouseDown={(e) => handleResizeMouseDown(e, 'w')} />
      <div className="resize-handle-ne" onMouseDown={(e) => handleResizeMouseDown(e, 'ne')} />
      <div className="resize-handle-nw" onMouseDown={(e) => handleResizeMouseDown(e, 'nw')} />
      <div className="resize-handle-se" onMouseDown={(e) => handleResizeMouseDown(e, 'se')} />
      <div className="resize-handle-sw" onMouseDown={(e) => handleResizeMouseDown(e, 'sw')} />
      
      <DockPanel
        tabs={window.panels}
        activeTab={window.activePanel}
        children={children}
        windowId={window.id}
        onClose={handleClose}
      />
    </div>
  )
}
