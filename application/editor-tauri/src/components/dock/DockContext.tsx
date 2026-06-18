import { createContext, useContext, useState, useCallback, ReactNode } from 'react'

// Panel types
export type PanelId = 'hierarchy' | 'inspector' | 'console' | 'keyframe' | 'sceneView' | 'postprocess' | 'imageViewer'

export interface PanelConfig {
  id: PanelId
  title: string
  icon: string
  closable: boolean
  minWidth?: number
  minHeight?: number
}

// Layout node types
export type SplitDirection = 'horizontal' | 'vertical'

export interface TabGroup {
  type: 'tabs'
  tabs: PanelId[]
  activeTab: PanelId
}

export interface SplitNode {
  type: 'split'
  direction: SplitDirection
  ratio: number // 0-1, position of the split
  first: LayoutNode
  second: LayoutNode
}

export type LayoutNode = TabGroup | SplitNode

export interface FloatingWindow {
  id: string
  panels: PanelId[]
  activePanel: PanelId
  x: number
  y: number
  width: number
  height: number
  zIndex: number
}

export interface DockState {
  layout: LayoutNode
  floatingWindows: FloatingWindow[]
  nextZIndex: number
}

// Drop zones for drag and drop
export type DropZone = 'center' | 'top' | 'bottom' | 'left' | 'right' | 'float'

interface DragState {
  panelId: PanelId | null
  sourceType: 'docked' | 'floating' | null
  sourceWindowId: string | null
}

interface DockContextValue {
  state: DockState
  dragState: DragState
  panelConfigs: Record<PanelId, PanelConfig>
  // Actions
  startDrag: (panelId: PanelId, sourceType: 'docked' | 'floating', sourceWindowId?: string) => void
  endDrag: () => void
  dropPanel: (targetPanelId: PanelId | null, zone: DropZone, targetWindowId?: string) => void
  closePanel: (panelId: PanelId) => void
  setActiveTab: (panelId: PanelId) => void
  moveFloatingWindow: (windowId: string, x: number, y: number) => void
  resizeFloatingWindow: (windowId: string, width: number, height: number) => void
  bringToFront: (windowId: string) => void
  closeFloatingWindow: (windowId: string) => void
  resetLayout: () => void
  // Panel visibility
  isPanelVisible: (panelId: PanelId) => boolean
  showPanel: (panelId: PanelId) => void
  // Layout management
  getLayout: () => LayoutNode
  setLayout: (layout: LayoutNode) => void
}

const DockContext = createContext<DockContextValue | null>(null)

// Panel configurations
const defaultPanelConfigs: Record<PanelId, PanelConfig> = {
  hierarchy: { id: 'hierarchy', title: 'Hierarchy', icon: '📋', closable: true, minWidth: 200 },
  inspector: { id: 'inspector', title: 'Inspector', icon: '🔍', closable: true, minWidth: 200 },
  console: { id: 'console', title: 'Console', icon: '💻', closable: true, minHeight: 100 },
  keyframe: { id: 'keyframe', title: 'KeyFrame', icon: '🎬', closable: true, minHeight: 100 },
  sceneView: { id: 'sceneView', title: 'Scene', icon: '🎮', closable: false },
  postprocess: { id: 'postprocess', title: 'Post Process', icon: '🎨', closable: true, minWidth: 200 },
  imageViewer: { id: 'imageViewer', title: 'Image Viewer', icon: '🖼️', closable: true, minWidth: 400, minHeight: 300 },
}

// Default layout similar to rfs_ui
// Console and KeyFrame panels are hidden by default (can be shown via View menu)
const createDefaultLayout = (): LayoutNode => ({
  type: 'split',
  direction: 'horizontal',
  ratio: 0.75,
  first: {
    type: 'tabs',
    tabs: ['sceneView'],
    activeTab: 'sceneView',
  },
  second: {
    type: 'split',
    direction: 'vertical',
    ratio: 0.5,
    first: { type: 'tabs', tabs: ['hierarchy'], activeTab: 'hierarchy' },
    second: { type: 'tabs', tabs: ['inspector', 'postprocess'], activeTab: 'inspector' },
  },
})

const createDefaultState = (): DockState => ({
  layout: createDefaultLayout(),
  floatingWindows: [],
  nextZIndex: 1000,
})

// Helper to find and remove a panel from layout
const removePanelFromLayout = (layout: LayoutNode, panelId: PanelId): LayoutNode | null => {
  if (layout.type === 'tabs') {
    const newTabs = layout.tabs.filter(t => t !== panelId)
    if (newTabs.length === 0) return null
    return {
      ...layout,
      tabs: newTabs,
      activeTab: newTabs.includes(layout.activeTab) ? layout.activeTab : newTabs[0],
    }
  }

  const newFirst = removePanelFromLayout(layout.first, panelId)
  const newSecond = removePanelFromLayout(layout.second, panelId)

  if (!newFirst && !newSecond) return null
  if (!newFirst) return newSecond
  if (!newSecond) return newFirst

  return { ...layout, first: newFirst, second: newSecond }
}

// Helper to add a panel to a tab group
const addPanelToTabGroup = (
  layout: LayoutNode,
  targetPanelId: PanelId,
  newPanelId: PanelId,
  zone: DropZone
): LayoutNode => {
  if (layout.type === 'tabs') {
    if (layout.tabs.includes(targetPanelId)) {
      if (zone === 'center') {
        // Add as new tab
        if (!layout.tabs.includes(newPanelId)) {
          return {
            ...layout,
            tabs: [...layout.tabs, newPanelId],
            activeTab: newPanelId,
          }
        }
        return { ...layout, activeTab: newPanelId }
      } else {
        // Split the tab group
        const direction: SplitDirection = zone === 'left' || zone === 'right' ? 'horizontal' : 'vertical'
        const newTabGroup: TabGroup = { type: 'tabs', tabs: [newPanelId], activeTab: newPanelId }
        
        if (zone === 'left' || zone === 'top') {
          return { type: 'split', direction, ratio: 0.5, first: newTabGroup, second: layout }
        } else {
          return { type: 'split', direction, ratio: 0.5, first: layout, second: newTabGroup }
        }
      }
    }
    return layout
  }

  return {
    ...layout,
    first: addPanelToTabGroup(layout.first, targetPanelId, newPanelId, zone),
    second: addPanelToTabGroup(layout.second, targetPanelId, newPanelId, zone),
  }
}

// Check if panel exists in layout
const panelExistsInLayout = (layout: LayoutNode, panelId: PanelId): boolean => {
  if (layout.type === 'tabs') {
    return layout.tabs.includes(panelId)
  }
  return panelExistsInLayout(layout.first, panelId) || panelExistsInLayout(layout.second, panelId)
}

// Set active tab in layout
const setActiveTabInLayout = (layout: LayoutNode, panelId: PanelId): LayoutNode => {
  if (layout.type === 'tabs') {
    if (layout.tabs.includes(panelId)) {
      return { ...layout, activeTab: panelId }
    }
    return layout
  }
  return {
    ...layout,
    first: setActiveTabInLayout(layout.first, panelId),
    second: setActiveTabInLayout(layout.second, panelId),
  }
}

interface DockProviderProps {
  children: ReactNode
}

export function DockProvider({ children }: DockProviderProps) {
  const [state, setState] = useState<DockState>(createDefaultState)
  const [dragState, setDragState] = useState<DragState>({
    panelId: null,
    sourceType: null,
    sourceWindowId: null,
  })

  const startDrag = useCallback((panelId: PanelId, sourceType: 'docked' | 'floating', sourceWindowId?: string) => {
    setDragState({ panelId, sourceType, sourceWindowId: sourceWindowId || null })
  }, [])

  const endDrag = useCallback(() => {
    setDragState({ panelId: null, sourceType: null, sourceWindowId: null })
  }, [])

  const dropPanel = useCallback((targetPanelId: PanelId | null, zone: DropZone, targetWindowId?: string) => {
    const { panelId, sourceType, sourceWindowId } = dragState
    if (!panelId) return

    setState(prev => {
      let newLayout = prev.layout
      let newFloatingWindows = [...prev.floatingWindows]

      // Remove from source
      if (sourceType === 'docked') {
        const result = removePanelFromLayout(newLayout, panelId)
        if (result) newLayout = result
      } else if (sourceType === 'floating' && sourceWindowId) {
        newFloatingWindows = newFloatingWindows.map(w => {
          if (w.id === sourceWindowId) {
            const newPanels = w.panels.filter(p => p !== panelId)
            if (newPanels.length === 0) return null as any
            return {
              ...w,
              panels: newPanels,
              activePanel: newPanels.includes(w.activePanel) ? w.activePanel : newPanels[0],
            }
          }
          return w
        }).filter(Boolean)
      }

      // Add to target
      if (zone === 'float') {
        // Create new floating window
        const newWindow: FloatingWindow = {
          id: `window-${Date.now()}`,
          panels: [panelId],
          activePanel: panelId,
          x: 100 + newFloatingWindows.length * 30,
          y: 100 + newFloatingWindows.length * 30,
          width: 300,
          height: 400,
          zIndex: prev.nextZIndex,
        }
        newFloatingWindows.push(newWindow)
        return { ...prev, layout: newLayout, floatingWindows: newFloatingWindows, nextZIndex: prev.nextZIndex + 1 }
      } else if (targetWindowId) {
        // Add to existing floating window
        newFloatingWindows = newFloatingWindows.map(w => {
          if (w.id === targetWindowId) {
            if (zone === 'center') {
              return { ...w, panels: [...w.panels, panelId], activePanel: panelId }
            }
            // For other zones, create adjacent floating window
          }
          return w
        })
      } else if (targetPanelId) {
        // Add to docked layout
        newLayout = addPanelToTabGroup(newLayout, targetPanelId, panelId, zone)
      }

      return { ...prev, layout: newLayout, floatingWindows: newFloatingWindows }
    })

    endDrag()
  }, [dragState, endDrag])

  const closePanel = useCallback((panelId: PanelId) => {
    if (!defaultPanelConfigs[panelId].closable) return

    setState(prev => {
      const newLayout = removePanelFromLayout(prev.layout, panelId)
      const newFloatingWindows = prev.floatingWindows.map(w => {
        const newPanels = w.panels.filter(p => p !== panelId)
        if (newPanels.length === 0) return null as any
        return {
          ...w,
          panels: newPanels,
          activePanel: newPanels.includes(w.activePanel) ? w.activePanel : newPanels[0],
        }
      }).filter(Boolean)

      return {
        ...prev,
        layout: newLayout || createDefaultLayout(),
        floatingWindows: newFloatingWindows,
      }
    })
  }, [])

  const setActiveTab = useCallback((panelId: PanelId) => {
    console.log('setActiveTab called with:', panelId) // 调试
    setState(prev => {
      const newLayout = setActiveTabInLayout(prev.layout, panelId)
      console.log('New layout after setActiveTab:', JSON.stringify(newLayout, null, 2)) // 调试
      return {
        ...prev,
        layout: newLayout,
        floatingWindows: prev.floatingWindows.map(w => 
          w.panels.includes(panelId) ? { ...w, activePanel: panelId } : w
        ),
      }
    })
  }, [])

  const moveFloatingWindow = useCallback((windowId: string, x: number, y: number) => {
    setState(prev => ({
      ...prev,
      floatingWindows: prev.floatingWindows.map(w =>
        w.id === windowId ? { ...w, x, y } : w
      ),
    }))
  }, [])

  const resizeFloatingWindow = useCallback((windowId: string, width: number, height: number) => {
    setState(prev => ({
      ...prev,
      floatingWindows: prev.floatingWindows.map(w =>
        w.id === windowId ? { ...w, width: Math.max(200, width), height: Math.max(150, height) } : w
      ),
    }))
  }, [])

  const bringToFront = useCallback((windowId: string) => {
    setState(prev => ({
      ...prev,
      floatingWindows: prev.floatingWindows.map(w =>
        w.id === windowId ? { ...w, zIndex: prev.nextZIndex } : w
      ),
      nextZIndex: prev.nextZIndex + 1,
    }))
  }, [])

  const closeFloatingWindow = useCallback((windowId: string) => {
    setState(prev => ({
      ...prev,
      floatingWindows: prev.floatingWindows.filter(w => w.id !== windowId),
    }))
  }, [])

  const resetLayout = useCallback(() => {
    setState(createDefaultState())
  }, [])

  const getLayout = useCallback(() => {
    return state.layout
  }, [state.layout])

  const setLayout = useCallback((layout: LayoutNode) => {
    setState(prev => ({
      ...prev,
      layout,
    }))
  }, [])

  const isPanelVisible = useCallback((panelId: PanelId) => {
    const inLayout = panelExistsInLayout(state.layout, panelId)
    const inFloating = state.floatingWindows.some(w => w.panels.includes(panelId))
    return inLayout || inFloating
  }, [state])

  const showPanel = useCallback((panelId: PanelId) => {
    if (isPanelVisible(panelId)) {
      setActiveTab(panelId)
      return
    }

    // ImageViewer should always open as a floating window
    if (panelId === 'imageViewer') {
      setState(prev => {
        const newWindow: FloatingWindow = {
          id: `floating-${Date.now()}`,
          panels: [panelId],
          activePanel: panelId,
          x: window.innerWidth * 0.25,
          y: window.innerHeight * 0.15,
          width: 800,
          height: 600,
          zIndex: prev.nextZIndex,
        }
        return {
          ...prev,
          floatingWindows: [...prev.floatingWindows, newWindow],
          nextZIndex: prev.nextZIndex + 1,
        }
      })
      return
    }

    setState(prev => {
      let newLayout = prev.layout
      
      // Console and KeyFrame should be added to bottom area below sceneView
      if (panelId === 'console' || panelId === 'keyframe') {
        // Find the sceneView node and add bottom panel below it
        const addBottomPanel = (node: LayoutNode): LayoutNode => {
          if (node.type === 'tabs' && node.tabs.includes('sceneView')) {
            // Create a vertical split with sceneView on top and new panel on bottom
            return {
              type: 'split',
              direction: 'vertical',
              ratio: 0.75,
              first: node,
              second: { type: 'tabs', tabs: [panelId], activeTab: panelId },
            }
          }
          if (node.type === 'split') {
            // Check if this is already a vertical split with sceneView on top
            if (node.direction === 'vertical' && 
                node.first.type === 'tabs' && 
                node.first.tabs.includes('sceneView')) {
              // Add to existing bottom panel
              if (node.second.type === 'tabs') {
                return {
                  ...node,
                  second: {
                    ...node.second,
                    tabs: [...node.second.tabs, panelId],
                    activeTab: panelId,
                  }
                }
              }
            }
            // Recursively search in left side (where sceneView usually is)
            const newFirst = addBottomPanel(node.first)
            if (newFirst !== node.first) {
              return { ...node, first: newFirst }
            }
          }
          return node
        }
        
        newLayout = addBottomPanel(newLayout)
      } else {
        // Other panels: add to first available tab group
        const addToFirstTabGroup = (node: LayoutNode): LayoutNode => {
          if (node.type === 'tabs') {
            return { ...node, tabs: [...node.tabs, panelId], activeTab: panelId }
          }
          return { ...node, first: addToFirstTabGroup(node.first) }
        }
        newLayout = addToFirstTabGroup(newLayout)
      }

      return { ...prev, layout: newLayout }
    })
  }, [isPanelVisible, setActiveTab])

  const value: DockContextValue = {
    state,
    dragState,
    panelConfigs: defaultPanelConfigs,
    startDrag,
    endDrag,
    dropPanel,
    closePanel,
    setActiveTab,
    moveFloatingWindow,
    resizeFloatingWindow,
    bringToFront,
    closeFloatingWindow,
    resetLayout,
    isPanelVisible,
    showPanel,
    getLayout,
    setLayout,
  }

  return <DockContext.Provider value={value}>{children}</DockContext.Provider>
}

export function useDock() {
  const context = useContext(DockContext)
  if (!context) {
    throw new Error('useDock must be used within a DockProvider')
  }
  return context
}
