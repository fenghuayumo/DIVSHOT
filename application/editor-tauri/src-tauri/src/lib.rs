mod engine;

use engine::{EngineClient, EngineStatus, SceneHierarchy, Transform, ViewportRect};
use std::sync::Mutex;
use tauri::State;

struct AppState {
    engine: Mutex<EngineClient>,
}

#[tauri::command]
fn engine_start(state: State<AppState>, project_root: Option<String>) -> Result<EngineStatus, String> {
    let root = project_root.unwrap_or_else(|| ".".to_string());
    state.engine.lock().map_err(|e| e.to_string())?.start(&root)
}

#[tauri::command]
fn engine_stop(state: State<AppState>) -> Result<(), String> {
    state.engine.lock().map_err(|e| e.to_string())?.stop()
}

#[tauri::command]
fn engine_status(state: State<AppState>) -> Result<EngineStatus, String> {
    Ok(state.engine.lock().map_err(|e| e.to_string())?.status())
}

#[tauri::command]
fn scene_get_hierarchy(state: State<AppState>) -> Result<SceneHierarchy, String> {
    state.engine.lock().map_err(|e| e.to_string())?.get_hierarchy()
}

#[tauri::command]
fn scene_select_entity(state: State<AppState>, entity_id: u64) -> Result<(), String> {
    state.engine.lock().map_err(|e| e.to_string())?.select_entity(entity_id)
}

#[tauri::command]
fn scene_get_transform(state: State<AppState>, entity_id: u64) -> Result<Transform, String> {
    state.engine.lock().map_err(|e| e.to_string())?.get_transform(entity_id)
}

#[tauri::command]
fn scene_set_transform(
    state: State<AppState>,
    entity_id: u64,
    transform: Transform,
) -> Result<(), String> {
    state
        .engine
        .lock()
        .map_err(|e| e.to_string())?
        .set_transform(entity_id, transform)
}

#[tauri::command]
fn viewport_set_rect(state: State<AppState>, rect: ViewportRect) -> Result<(), String> {
    state.engine.lock().map_err(|e| e.to_string())?.set_viewport_rect(rect)
}

#[cfg_attr(mobile, tauri::mobile_entry_point)]
pub fn run() {
    tauri::Builder::default()
        .plugin(tauri_plugin_shell::init())
        .manage(AppState {
            engine: Mutex::new(EngineClient::new()),
        })
        .invoke_handler(tauri::generate_handler![
            engine_start,
            engine_stop,
            engine_status,
            scene_get_hierarchy,
            scene_select_entity,
            scene_get_transform,
            scene_set_transform,
            viewport_set_rect,
        ])
        .run(tauri::generate_context!())
        .expect("error while running DIVSHOT editor");
}
