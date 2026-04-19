#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import os
import socket
import subprocess
import time

ERR_OK = 0
ERR_AUTH_ALREADY_ONLINE = 4102
ERR_AUTH_BANNED = 4103
ERR_GROUP_USER_NOT_FOUND = 4401
ERR_GROUP_CANNOT_CHANGE_CREATOR_ROLE = 4413
ERR_GROUP_ADMIN_CANNOT_GRANT_ADMIN = 4418
ERR_GROUP_MEMBER_MUTED = 4422
ERR_USER_BLOCKED_RELATION = 4608


def recv_json(sock: socket.socket) -> dict:
    data = sock.recv(65535)
    if not data:
        raise RuntimeError("empty response from server")
    text = data.decode("utf-8").rstrip("\x00")
    return json.loads(text)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Validate chat server roundtrip flows")
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=9120)
    parser.add_argument("--timeout", type=float, default=5.0)
    parser.add_argument("--ban-before-login", action="store_true")
    parser.add_argument("--duplicate-one-chat", action="store_true")
    parser.add_argument("--group-permission-checks", action="store_true")
    parser.add_argument("--history-pagination-checks", action="store_true")
    parser.add_argument("--search-blacklist-checks", action="store_true")
    parser.add_argument("--busy-state-checks", action="store_true")
    parser.add_argument("--nickname-checks", action="store_true")
    parser.add_argument("--mysql-host", default="127.0.0.1")
    parser.add_argument("--mysql-port", type=int, default=3306)
    parser.add_argument("--mysql-user", default="root")
    parser.add_argument("--mysql-password", default="123456")
    parser.add_argument("--mysql-database", default="chat")
    return parser.parse_args()


def update_user_state(user_id: int, state: str, args: argparse.Namespace) -> None:
    env = os.environ.copy()
    if args.mysql_password:
        env["MYSQL_PWD"] = args.mysql_password

    sql = f"UPDATE user SET state='{state}' WHERE id={user_id};"
    command = [
        "mysql",
        f"--host={args.mysql_host}",
        f"--port={args.mysql_port}",
        f"--user={args.mysql_user}",
        args.mysql_database,
        "-e",
        sql,
    ]
    try:
        subprocess.run(command, check=True, capture_output=True, text=True, env=env)
    except FileNotFoundError as exc:
        raise RuntimeError("mysql CLI not found") from exc
    except subprocess.CalledProcessError as exc:
        detail = exc.stderr.strip() or exc.stdout.strip() or "unknown mysql error"
        raise RuntimeError(f"failed to update user state: {detail}") from exc


def mysql_query_lines(sql: str, args: argparse.Namespace) -> list[str]:
    env = os.environ.copy()
    if args.mysql_password:
        env["MYSQL_PWD"] = args.mysql_password

    command = [
        "mysql",
        "--batch",
        "--skip-column-names",
        f"--host={args.mysql_host}",
        f"--port={args.mysql_port}",
        f"--user={args.mysql_user}",
        args.mysql_database,
        "-e",
        sql,
    ]
    try:
        result = subprocess.run(command, check=True, capture_output=True, text=True, env=env)
    except FileNotFoundError as exc:
        raise RuntimeError("mysql CLI not found") from exc
    except subprocess.CalledProcessError as exc:
        detail = exc.stderr.strip() or exc.stdout.strip() or "unknown mysql error"
        raise RuntimeError(f"mysql query failed: {detail}") from exc
    return [line.strip() for line in result.stdout.splitlines() if line.strip()]


def verify_duplicate_one_chat(sock: socket.socket, now: int, args: argparse.Namespace) -> int:
    password = "codex_pass_123"
    sender_name = f"dup_sender_{now}"
    receiver_name = f"dup_receiver_{now}"
    protocol_version = 1
    login_msg = 1
    reg_msg = 5
    one_chat_msg = 7
    sender_id = -1
    receiver_id = -1

    sender_reg = {
        "version": protocol_version,
        "msgid": reg_msg,
        "request_id": f"dup-reg-s-{now}",
        "name": sender_name,
        "password": password,
    }
    sock.sendall(json.dumps(sender_reg).encode("utf-8"))
    sender_resp = recv_json(sock)
    print("DUP_REG_SENDER", sender_resp)
    if sender_resp.get("errno") != ERR_OK:
        return 11

    receiver_reg = {
        "version": protocol_version,
        "msgid": reg_msg,
        "request_id": f"dup-reg-r-{now}",
        "name": receiver_name,
        "password": password,
    }
    sock.sendall(json.dumps(receiver_reg).encode("utf-8"))
    receiver_resp = recv_json(sock)
    print("DUP_REG_RECEIVER", receiver_resp)
    if receiver_resp.get("errno") != ERR_OK:
        return 12

    sender_id = sender_resp["id"]
    receiver_id = receiver_resp["id"]

    try:
        login_req = {
            "version": protocol_version,
            "msgid": login_msg,
            "request_id": f"dup-login-{now}",
            "id": sender_id,
            "password": password,
        }
        sock.sendall(json.dumps(login_req).encode("utf-8"))
        login_resp = recv_json(sock)
        print("DUP_LOGIN", login_resp)
        if login_resp.get("errno") != ERR_OK:
            return 13

        duplicate_request_id = f"dup-chat-{now}"
        chat_req = {
            "version": protocol_version,
            "msgid": one_chat_msg,
            "request_id": duplicate_request_id,
            "id": sender_id,
            "name": sender_name,
            "toid": receiver_id,
            "msg": "dedupe message",
            "time": "2026-04-19 00:00:00",
        }
        sock.sendall(json.dumps(chat_req).encode("utf-8"))
        first_resp = recv_json(sock)
        print("DUP_FIRST", first_resp)
        if first_resp.get("errno") != ERR_OK:
            return 14

        sock.sendall(json.dumps(chat_req).encode("utf-8"))
        second_resp = recv_json(sock)
        print("DUP_SECOND", second_resp)
        if second_resp.get("errno") != ERR_OK:
            return 15
        if second_resp.get("message_id") != first_resp.get("message_id"):
            return 16
        if not second_resp.get("duplicate"):
            return 17

        history_rows = mysql_query_lines(
            f"SELECT COUNT(*) FROM message_history WHERE request_id = '{duplicate_request_id}';",
            args,
        )
        offline_rows = mysql_query_lines(
            f"SELECT COUNT(*) FROM offlinemessage WHERE userid = {receiver_id} AND request_id = '{duplicate_request_id}';",
            args,
        )
        if not history_rows or history_rows[0] != "1":
            return 18
        if not offline_rows or offline_rows[0] != "1":
            return 19
        return 0
    finally:
        if sender_id > 0 and receiver_id > 0:
            mysql_query_lines(f"DELETE FROM user WHERE id IN ({sender_id}, {receiver_id});", args)
        elif sender_id > 0:
            mysql_query_lines(f"DELETE FROM user WHERE id = {sender_id};", args)
        elif receiver_id > 0:
            mysql_query_lines(f"DELETE FROM user WHERE id = {receiver_id};", args)


def verify_group_permission_checks(sock: socket.socket, now: int, args: argparse.Namespace) -> int:
    password = "codex_pass_123"
    creator_name = f"group_creator_{now}"
    member_name = f"group_member_{now}"
    extra_name = f"group_extra_{now}"
    protocol_version = 1
    login_msg = 1
    reg_msg = 5
    create_group_msg = 11
    add_group_msg = 13
    set_role_msg = 19
    creator_id = -1
    member_id = -1
    extra_id = -1

    creator_reg = {
        "version": protocol_version,
        "msgid": reg_msg,
        "request_id": f"group-reg-c-{now}",
        "name": creator_name,
        "password": password,
    }
    sock.sendall(json.dumps(creator_reg).encode("utf-8"))
    creator_resp = recv_json(sock)
    print("GROUP_REG_CREATOR", creator_resp)
    if creator_resp.get("errno") != ERR_OK:
        return 20
    creator_id = creator_resp["id"]

    member_reg = {
        "version": protocol_version,
        "msgid": reg_msg,
        "request_id": f"group-reg-m-{now}",
        "name": member_name,
        "password": password,
    }
    sock.sendall(json.dumps(member_reg).encode("utf-8"))
    member_resp = recv_json(sock)
    print("GROUP_REG_MEMBER", member_resp)
    if member_resp.get("errno") != ERR_OK:
        return 21
    member_id = member_resp["id"]

    extra_reg = {
        "version": protocol_version,
        "msgid": reg_msg,
        "request_id": f"group-reg-e-{now}",
        "name": extra_name,
        "password": password,
    }
    sock.sendall(json.dumps(extra_reg).encode("utf-8"))
    extra_resp = recv_json(sock)
    print("GROUP_REG_EXTRA", extra_resp)
    if extra_resp.get("errno") != ERR_OK:
        return 22
    extra_id = extra_resp["id"]

    try:
        invalid_create_req = {
            "version": protocol_version,
            "msgid": create_group_msg,
            "request_id": f"group-invalid-create-{now}",
            "id": 999999,
            "groupname": "bad-group",
            "groupdesc": "should fail",
        }
        sock.sendall(json.dumps(invalid_create_req).encode("utf-8"))
        invalid_create_resp = recv_json(sock)
        print("GROUP_INVALID_CREATE", invalid_create_resp)
        if invalid_create_resp.get("errno") != ERR_GROUP_USER_NOT_FOUND:
            return 23

        creator_login_req = {
            "version": protocol_version,
            "msgid": login_msg,
            "request_id": f"group-login-c-{now}",
            "id": creator_id,
            "password": password,
        }
        sock.sendall(json.dumps(creator_login_req).encode("utf-8"))
        creator_login_resp = recv_json(sock)
        print("GROUP_LOGIN_CREATOR", creator_login_resp)
        if creator_login_resp.get("errno") != ERR_OK:
            return 24

        create_req = {
            "version": protocol_version,
            "msgid": create_group_msg,
            "request_id": f"group-create-{now}",
            "id": creator_id,
            "groupname": f"group_{now}",
            "groupdesc": "permission test",
        }
        sock.sendall(json.dumps(create_req).encode("utf-8"))
        create_resp = recv_json(sock)
        print("GROUP_CREATE", create_resp)
        if create_resp.get("errno") != ERR_OK:
            return 25
        group_id = create_resp.get("groupid")
        if not group_id:
            return 26

        member_login_req = {
            "version": protocol_version,
            "msgid": login_msg,
            "request_id": f"group-login-m-{now}",
            "id": member_id,
            "password": password,
        }
        sock.sendall(json.dumps(member_login_req).encode("utf-8"))
        member_login_resp = recv_json(sock)
        print("GROUP_LOGIN_MEMBER", member_login_resp)
        if member_login_resp.get("errno") != ERR_OK:
            return 27

        extra_login_req = {
            "version": protocol_version,
            "msgid": login_msg,
            "request_id": f"group-login-e-{now}",
            "id": extra_id,
            "password": password,
        }
        sock.sendall(json.dumps(extra_login_req).encode("utf-8"))
        extra_login_resp = recv_json(sock)
        print("GROUP_LOGIN_EXTRA", extra_login_resp)
        if extra_login_resp.get("errno") != ERR_OK:
            return 28

        add_group_req = {
            "version": protocol_version,
            "msgid": add_group_msg,
            "request_id": f"group-join-{now}",
            "id": member_id,
            "groupid": group_id,
        }
        sock.sendall(json.dumps(add_group_req).encode("utf-8"))
        add_group_resp = recv_json(sock)
        print("GROUP_JOIN", add_group_resp)
        if add_group_resp.get("errno") != ERR_OK:
            return 29

        add_extra_req = {
            "version": protocol_version,
            "msgid": add_group_msg,
            "request_id": f"group-join-extra-{now}",
            "id": extra_id,
            "groupid": group_id,
        }
        sock.sendall(json.dumps(add_extra_req).encode("utf-8"))
        add_extra_resp = recv_json(sock)
        print("GROUP_JOIN_EXTRA", add_extra_resp)
        if add_extra_resp.get("errno") != ERR_OK:
            return 30

        promote_admin_req = {
            "version": protocol_version,
            "msgid": set_role_msg,
            "request_id": f"group-promote-admin-{now}",
            "id": creator_id,
            "groupid": group_id,
            "targetid": member_id,
            "role": "admin",
        }
        sock.sendall(json.dumps(promote_admin_req).encode("utf-8"))
        promote_admin_resp = recv_json(sock)
        print("GROUP_PROMOTE_ADMIN", promote_admin_resp)
        if promote_admin_resp.get("errno") != ERR_OK:
            return 31

        set_creator_role_req = {
            "version": protocol_version,
            "msgid": set_role_msg,
            "request_id": f"group-setrole-{now}",
            "id": creator_id,
            "groupid": group_id,
            "targetid": creator_id,
            "role": "admin",
        }
        sock.sendall(json.dumps(set_creator_role_req).encode("utf-8"))
        set_creator_role_resp = recv_json(sock)
        print("GROUP_SETROLE_CREATOR", set_creator_role_resp)
        if set_creator_role_resp.get("errno") != ERR_GROUP_CANNOT_CHANGE_CREATOR_ROLE:
            return 32

        admin_downgrade_req = {
            "version": protocol_version,
            "msgid": set_role_msg,
            "request_id": f"group-admin-downgrade-{now}",
            "id": member_id,
            "groupid": group_id,
            "targetid": extra_id,
            "role": "normal",
        }
        sock.sendall(json.dumps(admin_downgrade_req).encode("utf-8"))
        admin_downgrade_resp = recv_json(sock)
        print("GROUP_ADMIN_DOWNGRADE", admin_downgrade_resp)
        if admin_downgrade_resp.get("errno") != ERR_OK:
            return 33

        admin_promote_req = {
            "version": protocol_version,
            "msgid": set_role_msg,
            "request_id": f"group-admin-promote-{now}",
            "id": member_id,
            "groupid": group_id,
            "targetid": extra_id,
            "role": "admin",
        }
        sock.sendall(json.dumps(admin_promote_req).encode("utf-8"))
        admin_promote_resp = recv_json(sock)
        print("GROUP_ADMIN_PROMOTE", admin_promote_resp)
        if admin_promote_resp.get("errno") != ERR_GROUP_ADMIN_CANNOT_GRANT_ADMIN:
            return 34

        role_rows = mysql_query_lines(
            f"SELECT grouprole FROM groupuser WHERE groupid = {group_id} AND userid = {creator_id};",
            args,
        )
        if not role_rows or role_rows[0] != "creator":
            return 35
        admin_role_rows = mysql_query_lines(
            f"SELECT grouprole FROM groupuser WHERE groupid = {group_id} AND userid = {member_id};",
            args,
        )
        if not admin_role_rows or admin_role_rows[0] != "admin":
            return 36

        announcement_req = {
            "version": protocol_version,
            "msgid": 38,
            "request_id": f"group-announcement-{now}",
            "id": creator_id,
            "groupid": group_id,
            "announcement": "maintenance at 22:00",
        }
        sock.sendall(json.dumps(announcement_req).encode("utf-8"))
        announcement_resp = recv_json(sock)
        print("GROUP_ANNOUNCEMENT", announcement_resp)
        if announcement_resp.get("errno") != ERR_OK:
            return 37

        mute_req = {
            "version": protocol_version,
            "msgid": 40,
            "request_id": f"group-mute-{now}",
            "id": creator_id,
            "groupid": group_id,
            "targetid": extra_id,
            "minutes": 10,
        }
        sock.sendall(json.dumps(mute_req).encode("utf-8"))
        mute_resp = recv_json(sock)
        print("GROUP_MUTE", mute_resp)
        if mute_resp.get("errno") != ERR_OK:
            return 38

        muted_chat_req = {
            "version": protocol_version,
            "msgid": 15,
            "request_id": f"group-muted-chat-{now}",
            "id": extra_id,
            "name": extra_name,
            "groupid": group_id,
            "msg": "should be blocked",
            "time": "2026-04-19 00:00:00",
        }
        sock.sendall(json.dumps(muted_chat_req).encode("utf-8"))
        muted_chat_resp = recv_json(sock)
        print("GROUP_MUTED_CHAT", muted_chat_resp)
        if muted_chat_resp.get("errno") != ERR_GROUP_MEMBER_MUTED:
            return 39

        kick_req = {
            "version": protocol_version,
            "msgid": 42,
            "request_id": f"group-kick-{now}",
            "id": creator_id,
            "groupid": group_id,
            "targetid": extra_id,
        }
        sock.sendall(json.dumps(kick_req).encode("utf-8"))
        kick_resp = recv_json(sock)
        print("GROUP_KICK", kick_resp)
        if kick_resp.get("errno") != ERR_OK:
            return 40

        kicked_rows = mysql_query_lines(
            f"SELECT COUNT(*) FROM groupuser WHERE groupid = {group_id} AND userid = {extra_id};",
            args,
        )
        if not kicked_rows or kicked_rows[0] != "0":
            return 41
        return 0
    finally:
        if creator_id > 0 and member_id > 0 and extra_id > 0:
            mysql_query_lines(f"DELETE FROM user WHERE id IN ({creator_id}, {member_id}, {extra_id});", args)
        elif creator_id > 0 and member_id > 0:
            mysql_query_lines(f"DELETE FROM user WHERE id IN ({creator_id}, {member_id});", args)
        elif creator_id > 0:
            mysql_query_lines(f"DELETE FROM user WHERE id = {creator_id};", args)
        elif member_id > 0:
            mysql_query_lines(f"DELETE FROM user WHERE id = {member_id};", args)
        elif extra_id > 0:
            mysql_query_lines(f"DELETE FROM user WHERE id = {extra_id};", args)


def verify_history_pagination(sock: socket.socket, now: int, args: argparse.Namespace) -> int:
    password = "codex_pass_123"
    sender_name = f"history_sender_{now}"
    receiver_name = f"history_receiver_{now}"
    sender_id = -1
    receiver_id = -1

    try:
        for prefix, name in (("s", sender_name), ("r", receiver_name)):
            req = {
                "version": 1,
                "msgid": 5,
                "request_id": f"hist-reg-{prefix}-{now}",
                "name": name,
                "password": password,
            }
            sock.sendall(json.dumps(req).encode("utf-8"))
            resp = recv_json(sock)
            print(f"HIST_REG_{prefix.upper()}", resp)
            if resp.get("errno") != ERR_OK:
                return 30
            if prefix == "s":
                sender_id = resp["id"]
            else:
                receiver_id = resp["id"]

        login_req = {
            "version": 1,
            "msgid": 1,
            "request_id": f"hist-login-{now}",
            "id": sender_id,
            "password": password,
        }
        sock.sendall(json.dumps(login_req).encode("utf-8"))
        login_resp = recv_json(sock)
        print("HIST_LOGIN", login_resp)
        if login_resp.get("errno") != ERR_OK:
            return 31

        for index in range(3):
            chat_req = {
                "version": 1,
                "msgid": 7,
                "request_id": f"hist-chat-{now}-{index}",
                "id": sender_id,
                "name": sender_name,
                "toid": receiver_id,
                "msg": f"history message {index}",
                "time": "2026-04-19 00:00:00",
            }
            sock.sendall(json.dumps(chat_req).encode("utf-8"))
            chat_resp = recv_json(sock)
            print("HIST_CHAT", chat_resp)
            if chat_resp.get("errno") != ERR_OK:
                return 32

        query_req = {
            "version": 1,
            "msgid": 26,
            "request_id": f"hist-query-{now}",
            "id": sender_id,
            "targetid": receiver_id,
            "limit": 2,
            "offset": 0,
        }
        sock.sendall(json.dumps(query_req).encode("utf-8"))
        query_resp = recv_json(sock)
        print("HIST_QUERY_PAGE1", query_resp)
        if query_resp.get("errno") != ERR_OK:
            return 33
        history_page1 = [json.loads(item) for item in query_resp.get("history", [])]
        if len(history_page1) != 2:
            return 34
        if history_page1[0].get("message") != "history message 2":
            return 35
        if history_page1[1].get("message") != "history message 1":
            return 36

        query_req["offset"] = 2
        query_req["request_id"] = f"hist-query-2-{now}"
        sock.sendall(json.dumps(query_req).encode("utf-8"))
        query_resp_2 = recv_json(sock)
        print("HIST_QUERY_PAGE2", query_resp_2)
        if query_resp_2.get("errno") != ERR_OK:
            return 37
        history_page2 = [json.loads(item) for item in query_resp_2.get("history", [])]
        if len(history_page2) != 1 or history_page2[0].get("message") != "history message 0":
            return 38

        asc_query_req = {
            "version": 1,
            "msgid": 26,
            "request_id": f"hist-query-asc-{now}",
            "id": sender_id,
            "targetid": receiver_id,
            "limit": 3,
            "offset": 0,
            "order": "asc",
        }
        sock.sendall(json.dumps(asc_query_req).encode("utf-8"))
        asc_query_resp = recv_json(sock)
        print("HIST_QUERY_ASC", asc_query_resp)
        if asc_query_resp.get("errno") != ERR_OK:
            return 39
        if asc_query_resp.get("order") != "asc":
            return 40
        history_asc = [json.loads(item) for item in asc_query_resp.get("history", [])]
        if len(history_asc) != 3:
            return 41
        if history_asc[0].get("message") != "history message 0":
            return 42
        if history_asc[2].get("message") != "history message 2":
            return 43
        return 0
    finally:
        if sender_id > 0 and receiver_id > 0:
            mysql_query_lines(f"DELETE FROM user WHERE id IN ({sender_id}, {receiver_id});", args)
        elif sender_id > 0:
            mysql_query_lines(f"DELETE FROM user WHERE id = {sender_id};", args)
        elif receiver_id > 0:
            mysql_query_lines(f"DELETE FROM user WHERE id = {receiver_id};", args)


def verify_search_and_blacklist(sock: socket.socket, now: int, args: argparse.Namespace) -> int:
    password = "codex_pass_123"
    actor_name = f"search_actor_{now}"
    target_name = f"search_target_{now}"
    actor_id = -1
    target_id = -1

    try:
        for prefix, name in (("a", actor_name), ("t", target_name)):
            req = {
                "version": 1,
                "msgid": 5,
                "request_id": f"search-reg-{prefix}-{now}",
                "name": name,
                "password": password,
            }
            sock.sendall(json.dumps(req).encode("utf-8"))
            resp = recv_json(sock)
            print(f"SEARCH_REG_{prefix.upper()}", resp)
            if resp.get("errno") != ERR_OK:
                return 39
            if prefix == "a":
                actor_id = resp["id"]
            else:
                target_id = resp["id"]

        login_req = {
            "version": 1,
            "msgid": 1,
            "request_id": f"search-login-{now}",
            "id": actor_id,
            "password": password,
        }
        sock.sendall(json.dumps(login_req).encode("utf-8"))
        login_resp = recv_json(sock)
        print("SEARCH_LOGIN", login_resp)
        if login_resp.get("errno") != ERR_OK:
            return 40

        search_req = {
            "version": 1,
            "msgid": 28,
            "request_id": f"search-query-{now}",
            "id": actor_id,
            "keyword": "search_target",
            "limit": 10,
            "offset": 0,
        }
        sock.sendall(json.dumps(search_req).encode("utf-8"))
        search_resp = recv_json(sock)
        print("SEARCH_BEFORE_BLOCK", search_resp)
        if search_resp.get("errno") != ERR_OK:
            return 41
        users = [json.loads(item) for item in search_resp.get("users", [])]
        matched = next((item for item in users if item.get("id") == target_id), None)
        if matched is None or matched.get("has_blocked"):
            return 42

        block_req = {
            "version": 1,
            "msgid": 30,
            "request_id": f"block-{now}",
            "id": actor_id,
            "targetid": target_id,
        }
        sock.sendall(json.dumps(block_req).encode("utf-8"))
        block_resp = recv_json(sock)
        print("BLOCK_ADD", block_resp)
        if block_resp.get("errno") != ERR_OK:
            return 43

        search_req["request_id"] = f"search-query-2-{now}"
        sock.sendall(json.dumps(search_req).encode("utf-8"))
        search_resp_after = recv_json(sock)
        print("SEARCH_AFTER_BLOCK", search_resp_after)
        if search_resp_after.get("errno") != ERR_OK:
            return 44
        users_after = [json.loads(item) for item in search_resp_after.get("users", [])]
        matched_after = next((item for item in users_after if item.get("id") == target_id), None)
        if matched_after is None or not matched_after.get("has_blocked"):
            return 45

        chat_req = {
            "version": 1,
            "msgid": 7,
            "request_id": f"blocked-chat-{now}",
            "id": actor_id,
            "name": actor_name,
            "toid": target_id,
            "msg": "should fail",
            "time": "2026-04-19 00:00:00",
        }
        sock.sendall(json.dumps(chat_req).encode("utf-8"))
        chat_resp = recv_json(sock)
        print("BLOCKED_CHAT", chat_resp)
        if chat_resp.get("errno") != ERR_USER_BLOCKED_RELATION:
            return 46

        unblock_req = {
            "version": 1,
            "msgid": 32,
            "request_id": f"unblock-{now}",
            "id": actor_id,
            "targetid": target_id,
        }
        sock.sendall(json.dumps(unblock_req).encode("utf-8"))
        unblock_resp = recv_json(sock)
        print("BLOCK_REMOVE", unblock_resp)
        if unblock_resp.get("errno") != ERR_OK:
            return 47
        return 0
    finally:
        if actor_id > 0 and target_id > 0:
            mysql_query_lines(f"DELETE FROM user WHERE id IN ({actor_id}, {target_id});", args)
        elif actor_id > 0:
            mysql_query_lines(f"DELETE FROM user WHERE id = {actor_id};", args)
        elif target_id > 0:
            mysql_query_lines(f"DELETE FROM user WHERE id = {target_id};", args)


def verify_busy_state(sock: socket.socket, now: int, args: argparse.Namespace) -> int:
    password = "codex_pass_123"
    username = f"busy_user_{now}"
    user_id = -1

    try:
        reg_req = {
            "version": 1,
            "msgid": 5,
            "request_id": f"busy-reg-{now}",
            "name": username,
            "password": password,
        }
        sock.sendall(json.dumps(reg_req).encode("utf-8"))
        reg_resp = recv_json(sock)
        print("BUSY_REG", reg_resp)
        if reg_resp.get("errno") != ERR_OK:
            return 48
        user_id = reg_resp["id"]

        login_req = {
            "version": 1,
            "msgid": 1,
            "request_id": f"busy-login-{now}",
            "id": user_id,
            "password": password,
        }
        sock.sendall(json.dumps(login_req).encode("utf-8"))
        login_resp = recv_json(sock)
        print("BUSY_LOGIN", login_resp)
        if login_resp.get("errno") != ERR_OK:
            return 49

        set_busy_req = {
            "version": 1,
            "msgid": 34,
            "request_id": f"busy-set-{now}",
            "id": user_id,
            "state": "busy",
        }
        sock.sendall(json.dumps(set_busy_req).encode("utf-8"))
        set_busy_resp = recv_json(sock)
        print("BUSY_SET", set_busy_resp)
        if set_busy_resp.get("errno") != ERR_OK:
            return 50
        if set_busy_resp.get("state") != "busy":
            return 51

        state_rows = mysql_query_lines(f"SELECT state FROM user WHERE id = {user_id};", args)
        if not state_rows or state_rows[0] != "busy":
            return 52

        second_sock = socket.create_connection((args.host, args.port), timeout=args.timeout)
        try:
            second_sock.sendall(json.dumps(login_req).encode("utf-8"))
            second_login_resp = recv_json(second_sock)
            print("BUSY_SECOND_LOGIN", second_login_resp)
            if second_login_resp.get("errno") != ERR_AUTH_ALREADY_ONLINE:
                return 53
        finally:
            second_sock.close()

        logout_req = {
            "version": 1,
            "msgid": 3,
            "request_id": f"busy-logout-{now}",
            "id": user_id,
        }
        sock.sendall(json.dumps(logout_req).encode("utf-8"))
        logout_resp = recv_json(sock)
        print("BUSY_LOGOUT", logout_resp)
        if logout_resp.get("errno") != ERR_OK:
            return 54
        return 0
    finally:
        if user_id > 0:
            mysql_query_lines(f"DELETE FROM user WHERE id = {user_id};", args)


def verify_nickname_update(sock: socket.socket, now: int, args: argparse.Namespace) -> int:
    password = "codex_pass_123"
    original_name = f"rename_user_{now}"
    updated_name = f"renamed_user_{now}"
    user_id = -1

    try:
        reg_req = {
            "version": 1,
            "msgid": 5,
            "request_id": f"rename-reg-{now}",
            "name": original_name,
            "password": password,
        }
        sock.sendall(json.dumps(reg_req).encode("utf-8"))
        reg_resp = recv_json(sock)
        print("RENAME_REG", reg_resp)
        if reg_resp.get("errno") != ERR_OK:
            return 55
        user_id = reg_resp["id"]

        login_req = {
            "version": 1,
            "msgid": 1,
            "request_id": f"rename-login-{now}",
            "id": user_id,
            "password": password,
        }
        sock.sendall(json.dumps(login_req).encode("utf-8"))
        login_resp = recv_json(sock)
        print("RENAME_LOGIN", login_resp)
        if login_resp.get("errno") != ERR_OK:
            return 56

        rename_req = {
            "version": 1,
            "msgid": 36,
            "request_id": f"rename-set-{now}",
            "id": user_id,
            "name": updated_name,
        }
        sock.sendall(json.dumps(rename_req).encode("utf-8"))
        rename_resp = recv_json(sock)
        print("RENAME_SET", rename_resp)
        if rename_resp.get("errno") != ERR_OK:
            return 57
        if rename_resp.get("name") != updated_name:
            return 58

        name_rows = mysql_query_lines(f"SELECT name FROM user WHERE id = {user_id};", args)
        if not name_rows or name_rows[0] != updated_name:
            return 59
        return 0
    finally:
        if user_id > 0:
            mysql_query_lines(f"DELETE FROM user WHERE id = {user_id};", args)


def main() -> int:
    args = parse_args()
    now = int(time.time())
    username = f"codex_user_{now}"
    password = "codex_pass_123"
    protocol_version = 1

    # keep numeric ids aligned with include/public.hpp
    LOGIN_MSG = 1
    LOGINOUT_MSG = 3
    REG_MSG = 5

    sock = socket.create_connection((args.host, args.port), timeout=args.timeout)
    try:
        if args.duplicate_one_chat:
            return verify_duplicate_one_chat(sock, now, args)
        if args.group_permission_checks:
            return verify_group_permission_checks(sock, now, args)
        if args.history_pagination_checks:
            return verify_history_pagination(sock, now, args)
        if args.search_blacklist_checks:
            return verify_search_and_blacklist(sock, now, args)
        if args.busy_state_checks:
            return verify_busy_state(sock, now, args)
        if args.nickname_checks:
            return verify_nickname_update(sock, now, args)

        reg_req = {
            "version": protocol_version,
            "msgid": REG_MSG,
            "request_id": f"reg-{now}",
            "name": username,
            "password": password,
        }
        sock.sendall(json.dumps(reg_req).encode("utf-8"))
        reg_resp = recv_json(sock)
        print("REG", reg_resp)
        if reg_resp.get("errno") != ERR_OK:
            return 1
        if reg_resp.get("version") != protocol_version:
            return 2

        user_id = reg_resp["id"]
        if args.ban_before_login:
            update_user_state(user_id, "banned", args)

        login_req = {
            "version": protocol_version,
            "msgid": LOGIN_MSG,
            "request_id": f"login-{now}",
            "id": user_id,
            "password": password,
        }
        sock.sendall(json.dumps(login_req).encode("utf-8"))
        login_resp = recv_json(sock)
        if args.ban_before_login:
            print("LOGIN_BANNED", login_resp)
            if login_resp.get("errno") != ERR_AUTH_BANNED:
                return 3
            if login_resp.get("errmsg") != "用户已被封禁":
                return 4
            if login_resp.get("request_id") != login_req["request_id"]:
                return 5
            if login_resp.get("version") != protocol_version:
                return 6
            return 0

        print("LOGIN", login_resp)
        if login_resp.get("errno") != ERR_OK:
            return 7
        if login_resp.get("version") != protocol_version:
            return 8

        logout_req = {
            "version": protocol_version,
            "msgid": LOGINOUT_MSG,
            "request_id": f"logout-{now}",
            "id": user_id,
        }
        sock.sendall(json.dumps(logout_req).encode("utf-8"))
        logout_resp = recv_json(sock)
        print("LOGOUT", logout_resp)
        if logout_resp.get("errno") != ERR_OK:
            return 9
        if logout_resp.get("version") != protocol_version:
            return 10
    finally:
        sock.close()

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
