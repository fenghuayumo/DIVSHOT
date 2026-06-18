import React, { useState, useRef, useCallback, useEffect, ReactNode } from 'react'
import { SplitDirection } from './DockContext'

interface SplitPaneProps {
  direction: SplitDirection
  initialRatio: number
  first: ReactNode
  second: ReactNode
  minFirst?: number
  minSecond?: number
}

export function SplitPane({ 
  direction, 
  initialRatio, 
  first, 
  second,
  minFirst = 100,
  minSecond = 100 
}: SplitPaneProps) {
  const [ratio, setRatio] = useState(initialRatio)
  const containerRef = useRef<HTMLDivElement>(null)
  const isDragging = useRef(false)

  const handleMouseDown = useCallback((e: React.MouseEvent) => {
    e.preventDefault()
    isDragging.current = true
    document.body.style.cursor = direction === 'horizontal' ? 'ew-resize' : 'ns-resize'
    document.body.style.userSelect = 'none'
  }, [direction])

  useEffect(() => {
    const handleMouseMove = (e: MouseEvent) => {
      if (!isDragging.current || !containerRef.current) return

      const rect = containerRef.current.getBoundingClientRect()
      let newRatio: number

      if (direction === 'horizontal') {
        const x = e.clientX - rect.left
        newRatio = x / rect.width
      } else {
        const y = e.clientY - rect.top
        newRatio = y / rect.height
      }

      // Clamp ratio based on minimum sizes
      const containerSize = direction === 'horizontal' ? rect.width : rect.height
      const minRatio = minFirst / containerSize
      const maxRatio = 1 - (minSecond / containerSize)
      
      newRatio = Math.max(minRatio, Math.min(maxRatio, newRatio))
      setRatio(newRatio)
    }

    const handleMouseUp = () => {
      isDragging.current = false
      document.body.style.cursor = ''
      document.body.style.userSelect = ''
    }

    document.addEventListener('mousemove', handleMouseMove)
    document.addEventListener('mouseup', handleMouseUp)

    return () => {
      document.removeEventListener('mousemove', handleMouseMove)
      document.removeEventListener('mouseup', handleMouseUp)
    }
  }, [direction, minFirst, minSecond])

  const isHorizontal = direction === 'horizontal'
  const firstStyle = isHorizontal 
    ? { width: `${ratio * 100}%` }
    : { height: `${ratio * 100}%` }
  const secondStyle = isHorizontal
    ? { width: `${(1 - ratio) * 100}%` }
    : { height: `${(1 - ratio) * 100}%` }

  return (
    <div 
      ref={containerRef}
      className={`split-pane ${isHorizontal ? 'horizontal' : 'vertical'}`}
    >
      <div className="split-pane-first" style={firstStyle}>
        {first}
      </div>
      <div 
        className={`split-pane-divider ${isHorizontal ? 'horizontal' : 'vertical'}`}
        onMouseDown={handleMouseDown}
      />
      <div className="split-pane-second" style={secondStyle}>
        {second}
      </div>
    </div>
  )
}
