#![cfg_attr(not(debug_assertions), windows_subsystem = "windows")]

use serde_json::{json, Value};
use std::collections::HashMap;
use std::io::{Read, Write};
use std::net::{Shutdown, TcpStream};
use std::sync::atomic::{AtomicU64, Ordering};
use std::sync::{mpsc, Arc, Mutex};
use std::thread;
use std::time::Duration;
use tauri::Emitter;

const CHAT_PROTOCOL_VERSION: i64 = 1;
const LOGIN_MSG: i64 = 1;
const LOGINOUT_MSG: i64 = 3;
const ADD_FRIEND_MSG: i64 = 9;
const CREATE_GROUP_MSG: i64 = 11;
const ADD_GROUP_MSG: i64 = 13;
const QUERY_HISTORY_MSG: i64 = 26;
const SEARCH_USER_MSG: i64 = 28;
const SET_USER_STATE_MSG: i64 = 34;
const SET_NICKNAME_MSG: i64 = 36;
const SET_GROUP_ANNOUNCEMENT_MSG: i64 = 38;
const MUTE_GROUP_MEMBER_MSG: i64 = 40;
const KICK_GROUP_MEMBER_MSG: i64 = 42;
const ONE_CHAT_MSG: i64 = 7;
const GROUP_CHAT_MSG: i64 = 15;
const RECALL_NOTIFY_MSG: i64 = 25;

type PendingMap = Arc<Mutex<HashMap<String, mpsc::Sender<Value>>>>;

struct Session {
    writer: Arc<Mutex<TcpStream>>,
    host: String,
    port: u16,
    user_id: i64,
    user_name: String,
    pending: PendingMap,
}

struct AppState {
    request_counter: AtomicU64,
    session: Mutex<Option<Session>>,
}

fn next_request_id(state: &AppState, prefix: &str) -> String {
    let value = state.request_counter.fetch_add(1, Ordering::SeqCst);
    format!("{prefix}-{value}")
}

fn recv_message(stream: &mut TcpStream) -> Result<Value, String> {
    let mut response = Vec::new();
    let mut buf = [0_u8; 4096];
    loop {
        let read = stream.read(&mut buf).map_err(|err| err.to_string())?;
        if read == 0 {
            return Err("socket closed".to_string());
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

fn write_message(writer: &Arc<Mutex<TcpStream>>, payload: &Value) -> Result<(), String> {
    let mut stream = writer.lock().map_err(|err| err.to_string())?;
    let mut request = payload.to_string().into_bytes();
    request.push(0);
    stream.write_all(&request).map_err(|err| err.to_string())?;
    stream.flush().map_err(|err| err.to_string())
}

fn connect_stream(host: &str, port: u16) -> Result<TcpStream, String> {
    let stream = TcpStream::connect((host, port)).map_err(|err| err.to_string())?;
    stream
        .set_read_timeout(Some(Duration::from_secs(30)))
        .map_err(|err| err.to_string())?;
    stream
        .set_write_timeout(Some(Duration::from_secs(5)))
        .map_err(|err| err.to_string())?;
    Ok(stream)
}

fn spawn_listener(app: tauri::AppHandle, mut reader: TcpStream, pending: PendingMap) {
    thread::spawn(move || loop {
        let payload = match recv_message(&mut reader) {
            Ok(payload) => payload,
            Err(_) => break,
        };

        let request_id = payload
            .get("request_id")
            .and_then(Value::as_str)
            .map(ToOwned::to_owned)
            .unwrap_or_default();

        let delivered = if !request_id.is_empty() {
            let sender = pending
                .lock()
                .ok()
                .and_then(|mut entries| entries.remove(&request_id));
            if let Some(sender) = sender {
                sender.send(payload.clone()).is_ok()
            } else {
                false
            }
        } else {
            false
        };

        if delivered {
            continue;
        }

        let msgid = payload.get("msgid").and_then(Value::as_i64).unwrap_or_default();
        if msgid == ONE_CHAT_MSG || msgid == GROUP_CHAT_MSG || msgid == RECALL_NOTIFY_MSG {
            let _ = app.emit("protocol-event", payload);
        }
    });
}

fn send_request(session: &Session, payload: Value) -> Result<Value, String> {
    let request_id = payload
        .get("request_id")
        .and_then(Value::as_str)
        .ok_or_else(|| "request_id missing".to_string())?
        .to_string();
    let (sender, receiver) = mpsc::channel();
    session
        .pending
        .lock()
        .map_err(|err| err.to_string())?
        .insert(request_id.clone(), sender);

    if let Err(error) = write_message(&session.writer, &payload) {
        session
            .pending
            .lock()
            .map_err(|err| err.to_string())?
            .remove(&request_id);
        return Err(error);
    }

    receiver
        .recv_timeout(Duration::from_secs(5))
        .map_err(|err| err.to_string())
}

fn current_time_string() -> String {
    chrono::Local::now().format("%Y-%m-%d %H:%M:%S").to_string()
}

#[tauri::command]
fn login(
    app: tauri::AppHandle,
    state: tauri::State<AppState>,
    host: String,
    port: u16,
    user_id: i64,
    password: String,
) -> Result<Value, String> {
    let writer = connect_stream(&host, port)?;
    let reader = writer.try_clone().map_err(|err| err.to_string())?;
    let pending: PendingMap = Arc::new(Mutex::new(HashMap::new()));
    spawn_listener(app, reader, pending.clone());

    let writer = Arc::new(Mutex::new(writer));
    let request_id = next_request_id(&state, "desktop-login");
    let payload = json!({
        "version": CHAT_PROTOCOL_VERSION,
        "msgid": LOGIN_MSG,
        "request_id": request_id,
        "id": user_id,
        "password": password,
    });

    let temp_session = Session {
        writer: writer.clone(),
        host: host.clone(),
        port,
        user_id,
        user_name: String::new(),
        pending: pending.clone(),
    };
    let response = send_request(&temp_session, payload)?;
    if response.get("errno").and_then(Value::as_i64) == Some(0) {
        let mut connected_session = temp_session;
        connected_session.user_name = response
            .get("name")
            .and_then(Value::as_str)
            .unwrap_or_default()
            .to_string();
        let mut guard = state.session.lock().map_err(|err| err.to_string())?;
        *guard = Some(connected_session);
    } else {
        let stream = writer.lock().map_err(|err| err.to_string())?;
        let _ = stream.shutdown(Shutdown::Both);
    }
    Ok(response)
}

#[tauri::command]
fn logout(state: tauri::State<AppState>) -> Result<Value, String> {
    let mut guard = state.session.lock().map_err(|err| err.to_string())?;
    let session = guard.as_ref().ok_or_else(|| "No active session".to_string())?;
    let payload = json!({
        "version": CHAT_PROTOCOL_VERSION,
        "msgid": LOGINOUT_MSG,
        "request_id": next_request_id(&state, "desktop-logout"),
        "id": session.user_id,
    });
    let response = send_request(session, payload)?;
    if let Ok(stream) = session.writer.lock() {
        let _ = stream.shutdown(Shutdown::Both);
    }
    *guard = None;
    Ok(response)
}

#[tauri::command]
fn set_presence(state: tauri::State<AppState>, state_name: String) -> Result<Value, String> {
    let guard = state.session.lock().map_err(|err| err.to_string())?;
    let session = guard.as_ref().ok_or_else(|| "No active session".to_string())?;
    let payload = json!({
        "version": CHAT_PROTOCOL_VERSION,
        "msgid": SET_USER_STATE_MSG,
        "request_id": next_request_id(&state, "desktop-state"),
        "id": session.user_id,
        "state": state_name,
    });
    send_request(session, payload)
}

#[tauri::command]
fn set_nickname(state: tauri::State<AppState>, name: String) -> Result<Value, String> {
    let mut guard = state.session.lock().map_err(|err| err.to_string())?;
    let session = guard.as_ref().ok_or_else(|| "No active session".to_string())?;
    let payload = json!({
        "version": CHAT_PROTOCOL_VERSION,
        "msgid": SET_NICKNAME_MSG,
        "request_id": next_request_id(&state, "desktop-name"),
        "id": session.user_id,
        "name": name,
    });
    let response = send_request(session, payload)?;
    if response.get("errno").and_then(Value::as_i64) == Some(0) {
        if let Some(session) = guard.as_mut() {
            session.user_name = response
                .get("name")
                .and_then(Value::as_str)
                .unwrap_or_default()
                .to_string();
        }
    }
    Ok(response)
}

#[tauri::command]
fn add_friend(state: tauri::State<AppState>, friend_id: i64) -> Result<Value, String> {
    let guard = state.session.lock().map_err(|err| err.to_string())?;
    let session = guard.as_ref().ok_or_else(|| "No active session".to_string())?;
    let payload = json!({
        "version": CHAT_PROTOCOL_VERSION,
        "msgid": ADD_FRIEND_MSG,
        "request_id": next_request_id(&state, "desktop-add-friend"),
        "id": session.user_id,
        "friendid": friend_id,
    });
    send_request(session, payload)
}

#[tauri::command]
fn create_group(state: tauri::State<AppState>, group_name: String, group_desc: String) -> Result<Value, String> {
    let guard = state.session.lock().map_err(|err| err.to_string())?;
    let session = guard.as_ref().ok_or_else(|| "No active session".to_string())?;
    let payload = json!({
        "version": CHAT_PROTOCOL_VERSION,
        "msgid": CREATE_GROUP_MSG,
        "request_id": next_request_id(&state, "desktop-create-group"),
        "id": session.user_id,
        "groupname": group_name,
        "groupdesc": group_desc,
    });
    send_request(session, payload)
}

#[tauri::command]
fn join_group(state: tauri::State<AppState>, group_id: i64) -> Result<Value, String> {
    let guard = state.session.lock().map_err(|err| err.to_string())?;
    let session = guard.as_ref().ok_or_else(|| "No active session".to_string())?;
    let payload = json!({
        "version": CHAT_PROTOCOL_VERSION,
        "msgid": ADD_GROUP_MSG,
        "request_id": next_request_id(&state, "desktop-join-group"),
        "id": session.user_id,
        "groupid": group_id,
    });
    send_request(session, payload)
}

#[tauri::command]
fn set_group_announcement(state: tauri::State<AppState>, group_id: i64, announcement: String) -> Result<Value, String> {
    let guard = state.session.lock().map_err(|err| err.to_string())?;
    let session = guard.as_ref().ok_or_else(|| "No active session".to_string())?;
    let payload = json!({
        "version": CHAT_PROTOCOL_VERSION,
        "msgid": SET_GROUP_ANNOUNCEMENT_MSG,
        "request_id": next_request_id(&state, "desktop-group-announcement"),
        "id": session.user_id,
        "groupid": group_id,
        "announcement": announcement,
    });
    send_request(session, payload)
}

#[tauri::command]
fn mute_group_member(state: tauri::State<AppState>, group_id: i64, target_id: i64, minutes: i64) -> Result<Value, String> {
    let guard = state.session.lock().map_err(|err| err.to_string())?;
    let session = guard.as_ref().ok_or_else(|| "No active session".to_string())?;
    let payload = json!({
        "version": CHAT_PROTOCOL_VERSION,
        "msgid": MUTE_GROUP_MEMBER_MSG,
        "request_id": next_request_id(&state, "desktop-group-mute"),
        "id": session.user_id,
        "groupid": group_id,
        "targetid": target_id,
        "minutes": minutes,
    });
    send_request(session, payload)
}

#[tauri::command]
fn kick_group_member(state: tauri::State<AppState>, group_id: i64, target_id: i64) -> Result<Value, String> {
    let guard = state.session.lock().map_err(|err| err.to_string())?;
    let session = guard.as_ref().ok_or_else(|| "No active session".to_string())?;
    let payload = json!({
        "version": CHAT_PROTOCOL_VERSION,
        "msgid": KICK_GROUP_MEMBER_MSG,
        "request_id": next_request_id(&state, "desktop-group-kick"),
        "id": session.user_id,
        "groupid": group_id,
        "targetid": target_id,
    });
    send_request(session, payload)
}

#[tauri::command]
fn send_direct_message(state: tauri::State<AppState>, target_id: i64, message: String) -> Result<Value, String> {
    let guard = state.session.lock().map_err(|err| err.to_string())?;
    let session = guard.as_ref().ok_or_else(|| "No active session".to_string())?;
    let payload = json!({
        "version": CHAT_PROTOCOL_VERSION,
        "msgid": ONE_CHAT_MSG,
        "request_id": next_request_id(&state, "desktop-direct-msg"),
        "id": session.user_id,
        "name": session.user_name,
        "toid": target_id,
        "msg": message,
        "time": current_time_string(),
    });
    send_request(session, payload)
}

#[tauri::command]
fn send_group_message(state: tauri::State<AppState>, group_id: i64, message: String) -> Result<Value, String> {
    let guard = state.session.lock().map_err(|err| err.to_string())?;
    let session = guard.as_ref().ok_or_else(|| "No active session".to_string())?;
    let payload = json!({
        "version": CHAT_PROTOCOL_VERSION,
        "msgid": GROUP_CHAT_MSG,
        "request_id": next_request_id(&state, "desktop-group-msg"),
        "id": session.user_id,
        "name": session.user_name,
        "groupid": group_id,
        "msg": message,
        "time": current_time_string(),
    });
    send_request(session, payload)
}

#[tauri::command]
fn search_users(state: tauri::State<AppState>, keyword: String, limit: i64, offset: i64) -> Result<Value, String> {
    let guard = state.session.lock().map_err(|err| err.to_string())?;
    let session = guard.as_ref().ok_or_else(|| "No active session".to_string())?;
    let payload = json!({
        "version": CHAT_PROTOCOL_VERSION,
        "msgid": SEARCH_USER_MSG,
        "request_id": next_request_id(&state, "desktop-search"),
        "id": session.user_id,
        "keyword": keyword,
        "limit": limit,
        "offset": offset,
    });
    send_request(session, payload)
}

#[tauri::command]
fn query_direct_history(
    state: tauri::State<AppState>,
    target_id: i64,
    limit: i64,
    offset: i64,
    order: String,
) -> Result<Value, String> {
    let guard = state.session.lock().map_err(|err| err.to_string())?;
    let session = guard.as_ref().ok_or_else(|| "No active session".to_string())?;
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
    send_request(session, payload)
}

#[tauri::command]
fn query_group_history(
    state: tauri::State<AppState>,
    group_id: i64,
    limit: i64,
    offset: i64,
    order: String,
) -> Result<Value, String> {
    let guard = state.session.lock().map_err(|err| err.to_string())?;
    let session = guard.as_ref().ok_or_else(|| "No active session".to_string())?;
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
    send_request(session, payload)
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
            "user_name": session.user_name,
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
            add_friend,
            create_group,
            join_group,
            set_group_announcement,
            mute_group_member,
            kick_group_member,
            send_direct_message,
            send_group_message,
            search_users,
            query_direct_history,
            query_group_history,
            session_info,
        ])
        .run(tauri::generate_context!())
        .expect("failed to run tauri application");
}
