#!/usr/bin/env python3
from __future__ import annotations

import json
import socket
import time


def recv_json(sock: socket.socket) -> dict:
    data = sock.recv(65535)
    if not data:
        raise RuntimeError("empty response from server")
    text = data.decode("utf-8").rstrip("\x00")
    return json.loads(text)


def main() -> int:
    now = int(time.time())
    username = f"codex_user_{now}"
    password = "codex_pass_123"

    # keep numeric ids aligned with include/public.hpp
    LOGIN_MSG = 1
    LOGINOUT_MSG = 3
    REG_MSG = 5

    sock = socket.create_connection(("127.0.0.1", 9120), timeout=5)
    try:
        reg_req = {
            "msgid": REG_MSG,
            "request_id": f"reg-{now}",
            "name": username,
            "password": password,
        }
        sock.sendall(json.dumps(reg_req).encode("utf-8"))
        reg_resp = recv_json(sock)
        print("REG", reg_resp)
        if reg_resp.get("errno") != 0:
            return 1

        user_id = reg_resp["id"]
        login_req = {
            "msgid": LOGIN_MSG,
            "request_id": f"login-{now}",
            "id": user_id,
            "password": password,
        }
        sock.sendall(json.dumps(login_req).encode("utf-8"))
        login_resp = recv_json(sock)
        print("LOGIN", login_resp)
        if login_resp.get("errno") != 0:
            return 2

        logout_req = {
            "msgid": LOGINOUT_MSG,
            "request_id": f"logout-{now}",
            "id": user_id,
        }
        sock.sendall(json.dumps(logout_req).encode("utf-8"))
        logout_resp = recv_json(sock)
        print("LOGOUT", logout_resp)
        if logout_resp.get("errno") != 0:
            return 3
    finally:
        sock.close()

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
