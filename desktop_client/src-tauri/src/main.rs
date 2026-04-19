#![cfg_attr(not(debug_assertions), windows_subsystem = "windows")]

use serde_json::{json, Value};
use std::io::{Read, Write};
use std::net::TcpStream;
use std::sync::atomic::{AtomicU64, Ordering};
use std::sync::Mutex;
use std::time::Duration;

const CHAT_PROTOCOL_VERSION: i64 = 1;
const LOGIN_MSG: i64 = 1;
const LOGINOUT_MSG: i64 = 3;
const QUERY_HISTORY_MSG: i64 = 26;
const SEARCH_USER_MSG: i64 = 28;
const SET_USER_STATE_MSG: i64 = 34;
const SET_NICKNAME_MSG: i64 = 36;

struct Session {
    stream: TcpStream,
    host: String,
    port: u16,
    user_id: i64,
}

struct AppState {
    request_counter: AtomicU64,
    session: Mutex<Option<Session>>,
}

fn next_request_id(state: &AppState, prefix: &str) -> String {
    let value = state.request_counter.fetch_add(1, Ordering::SeqCst);
    format!("{prefix}-{value}")
}

fn send_and_receive(stream: &mut TcpStream, payload: &Value) -> Result<Value, String> {
    let mut request = payload.to_string().into_bytes();
    request.push(0);
    stream.write_all(&request).map_err(|err| err.to_string())?;
    stream.flush().map_err(|err| err.to_string())?;

    let mut response = Vec::new();
    let mut buf = [0_u8; 4096];
    loop {
        let read = stream.read(&mut buf).map_err(|err| err.to_string())?;
        if read == 0 {
            break;
        }
        response.extend_from_slice(&buf[..read]);
        if response.contains(&0) {
            break;
        }
    }

    if let Some(pos) = response.iter().position(|byte| *byte == 0) {
        response.truncate(pos);
    }
    let text = String::from_utf8(response).map_err(|err| err.to_string())?;
    serde_json::from_str(&text).map_err(|err| err.to_string())
}

fn connect_stream(host: &str, port: u16) -> Result<TcpStream, String> {
    let stream = TcpStream::connect((host, port)).map_err(|err| err.to_string())?;
    stream
        .set_read_timeout(Some(Duration::from_secs(5)))
        .map_err(|err| err.to_string())?;
    stream
        .set_write_timeout(Some(Duration::from_secs(5)))
        .map_err(|err| err.to_string())?;
    Ok(stream)
}

#[tauri::command]
fn login(state: tauri::State<AppState>, host: String, port: u16, user_id: i64, password: String) -> Result<Value, String> {
    let mut stream = connect_stream(&host, port)?;
    let payload = json!({
        "version": CHAT_PROTOCOL_VERSION,
        "msgid": LOGIN_MSG,
        "request_id": next_request_id(&state, "desktop-login"),
        "id": user_id,
        "password": password,
    });
    let response = send_and_receive(&mut stream, &payload)?;
    if response.get("errno").and_then(Value::as_i64) == Some(0) {
        let mut guard = state.session.lock().map_err(|err| err.to_string())?;
        *guard = Some(Session {
            stream,
            host,
            port,
            user_id,
        });
    }
    Ok(response)
}

#[tauri::command]
fn logout(state: tauri::State<AppState>) -> Result<Value, String> {
    let mut guard = state.session.lock().map_err(|err| err.to_string())?;
    let session = guard.as_mut().ok_or_else(|| "No active session".to_string())?;
    let payload = json!({
        "version": CHAT_PROTOCOL_VERSION,
        "msgid": LOGINOUT_MSG,
        "request_id": next_request_id(&state, "desktop-logout"),
        "id": session.user_id,
    });
    let response = send_and_receive(&mut session.stream, &payload)?;
    *guard = None;
    Ok(response)
}

#[tauri::command]
fn set_presence(state: tauri::State<AppState>, state_name: String) -> Result<Value, String> {
    let mut guard = state.session.lock().map_err(|err| err.to_string())?;
    let session = guard.as_mut().ok_or_else(|| "No active session".to_string())?;
    let payload = json!({
        "version": CHAT_PROTOCOL_VERSION,
        "msgid": SET_USER_STATE_MSG,
        "request_id": next_request_id(&state, "desktop-state"),
        "id": session.user_id,
        "state": state_name,
    });
    send_and_receive(&mut session.stream, &payload)
}

#[tauri::command]
fn set_nickname(state: tauri::State<AppState>, name: String) -> Result<Value, String> {
    let mut guard = state.session.lock().map_err(|err| err.to_string())?;
    let session = guard.as_mut().ok_or_else(|| "No active session".to_string())?;
    let payload = json!({
        "version": CHAT_PROTOCOL_VERSION,
        "msgid": SET_NICKNAME_MSG,
        "request_id": next_request_id(&state, "desktop-name"),
        "id": session.user_id,
        "name": name,
    });
    send_and_receive(&mut session.stream, &payload)
}

#[tauri::command]
fn search_users(state: tauri::State<AppState>, keyword: String, limit: i64, offset: i64) -> Result<Value, String> {
    let mut guard = state.session.lock().map_err(|err| err.to_string())?;
    let session = guard.as_mut().ok_or_else(|| "No active session".to_string())?;
    let payload = json!({
        "version": CHAT_PROTOCOL_VERSION,
        "msgid": SEARCH_USER_MSG,
        "request_id": next_request_id(&state, "desktop-search"),
        "id": session.user_id,
        "keyword": keyword,
        "limit": limit,
        "offset": offset,
    });
    send_and_receive(&mut session.stream, &payload)
}

#[tauri::command]
fn query_direct_history(
    state: tauri::State<AppState>,
    target_id: i64,
    limit: i64,
    offset: i64,
    order: String,
) -> Result<Value, String> {
    let mut guard = state.session.lock().map_err(|err| err.to_string())?;
    let session = guard.as_mut().ok_or_else(|| "No active session".to_string())?;
    let payload = json!({
        "version": CHAT_PROTOCOL_VERSION,
        "msgid": QUERY_HISTORY_MSG,
        "request_id": next_request_id(&state, "desktop-history-direct"),
        "id": session.user_id,
        "targetid": target_id,
        "limit": limit,
        "offset": offset,
        "order": order,
    });
    send_and_receive(&mut session.stream, &payload)
}

#[tauri::command]
fn query_group_history(
    state: tauri::State<AppState>,
    group_id: i64,
    limit: i64,
    offset: i64,
    order: String,
) -> Result<Value, String> {
    let mut guard = state.session.lock().map_err(|err| err.to_string())?;
    let session = guard.as_mut().ok_or_else(|| "No active session".to_string())?;
    let payload = json!({
        "version": CHAT_PROTOCOL_VERSION,
        "msgid": QUERY_HISTORY_MSG,
        "request_id": next_request_id(&state, "desktop-history-group"),
        "id": session.user_id,
        "groupid": group_id,
        "limit": limit,
        "offset": offset,
        "order": order,
    });
    send_and_receive(&mut session.stream, &payload)
}

#[tauri::command]
fn session_info(state: tauri::State<AppState>) -> Result<Value, String> {
    let guard = state.session.lock().map_err(|err| err.to_string())?;
    if let Some(session) = guard.as_ref() {
        Ok(json!({
            "connected": true,
            "host": session.host,
            "port": session.port,
            "user_id": session.user_id,
        }))
    } else {
        Ok(json!({ "connected": false }))
    }
}

fn main() {
    tauri::Builder::default()
        .manage(AppState {
            request_counter: AtomicU64::new(1),
            session: Mutex::new(None),
        })
        .invoke_handler(tauri::generate_handler![
            login,
            logout,
            set_presence,
            set_nickname,
            search_users,
            query_direct_history,
            query_group_history,
            session_info,
        ])
        .run(tauri::generate_context!())
        .expect("failed to run tauri application");
}
