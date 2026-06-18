use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct EngineStatus {
    pub running: bool,
    pub version: Option<String>,
    pub ipc_port: u16,
    pub error: Option<String>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct HierarchyEntity {
    pub id: u64,
    pub name: String,
    pub children: Vec<HierarchyEntity>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct SceneHierarchy {
    pub entities: Vec<HierarchyEntity>,
    pub selected: u64,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Transform {
    pub position: [f32; 3],
    pub rotation: [f32; 3],
    pub scale: [f32; 3],
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ViewportRect {
    pub x: i32,
    pub y: i32,
    pub width: i32,
    pub height: i32,
}

const DEFAULT_IPC_PORT: u16 = 17350;

/// Phase 0 client: in-process stub matching editor-api JSON until IPC is wired.
pub struct EngineClient {
    running: bool,
    ipc_port: u16,
    version: Option<String>,
    last_error: Option<String>,
    selected_entity: u64,
    viewport_rect: ViewportRect,
}

impl EngineClient {
    pub fn new() -> Self {
        Self {
            running: false,
            ipc_port: DEFAULT_IPC_PORT,
            version: None,
            last_error: None,
            selected_entity: 0,
            viewport_rect: ViewportRect {
                x: 0,
                y: 0,
                width: 1280,
                height: 720,
            },
        }
    }

    pub fn start(&mut self, _project_root: &str) -> Result<EngineStatus, String> {
        // TODO: spawn divshot-engine sidecar via tauri_plugin_shell::ShellExt
        self.running = true;
        self.version = Some("0.1.0-stub".to_string());
        self.last_error = None;
        Ok(self.status())
    }

    pub fn stop(&mut self) -> Result<(), String> {
        self.running = false;
        self.version = None;
        Ok(())
    }

    pub fn status(&self) -> EngineStatus {
        EngineStatus {
            running: self.running,
            version: self.version.clone(),
            ipc_port: self.ipc_port,
            error: self.last_error.clone(),
        }
    }

    pub fn get_hierarchy(&self) -> Result<SceneHierarchy, String> {
        if !self.running {
            return Err("engine is not running".to_string());
        }

        Ok(SceneHierarchy {
            entities: vec![HierarchyEntity {
                id: 1,
                name: "Root".to_string(),
                children: vec![],
            }],
            selected: self.selected_entity,
        })
    }

    pub fn select_entity(&mut self, entity_id: u64) -> Result<(), String> {
        if !self.running {
            return Err("engine is not running".to_string());
        }
        self.selected_entity = entity_id;
        Ok(())
    }

    pub fn get_transform(&self, entity_id: u64) -> Result<Transform, String> {
        if !self.running {
            return Err("engine is not running".to_string());
        }
        if entity_id == 0 {
            return Err("invalid entity id".to_string());
        }

        Ok(Transform {
            position: [0.0, 0.0, 0.0],
            rotation: [0.0, 0.0, 0.0],
            scale: [1.0, 1.0, 1.0],
        })
    }

    pub fn set_transform(&mut self, entity_id: u64, _transform: Transform) -> Result<(), String> {
        if !self.running {
            return Err("engine is not running".to_string());
        }
        if entity_id == 0 {
            return Err("invalid entity id".to_string());
        }
        Ok(())
    }

    pub fn set_viewport_rect(&mut self, rect: ViewportRect) -> Result<(), String> {
        if rect.width <= 0 || rect.height <= 0 {
            return Err("invalid viewport rect".to_string());
        }
        self.viewport_rect = rect;
        Ok(())
    }
}
