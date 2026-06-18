export interface HierarchyEntity {
  id: number;
  name: string;
  children: HierarchyEntity[];
}

export interface SceneHierarchy {
  entities: HierarchyEntity[];
  selected: number;
}

export interface EditorTransform {
  position: [number, number, number];
  rotation: [number, number, number];
  scale: [number, number, number];
}

export interface ViewportRect {
  x: number;
  y: number;
  width: number;
  height: number;
}

export interface EngineStatus {
  running: boolean;
  version: string | null;
  ipcPort: number;
  error: string | null;
}
