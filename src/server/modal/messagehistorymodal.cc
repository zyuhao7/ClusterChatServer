#include "messagehistorymodal.hpp"

#include "db.h"
#include <cstdlib>
#include <cstdio>

long long MessageHistoryModal::insertDirect(
    const std::string &request_id,
    int sender_id,
    int receiver_id,
    const std::string &message)
{
    char sql[4096] = {0};
    sprintf(sql,
            "insert into message_history(request_id, sender_id, receiver_id, group_id, message, msg_type) "
            "values('%s', %d, %d, NULL, '%s', 'direct')",
            request_id.c_str(), sender_id, receiver_id, message.c_str());

    MySQL mysql;
    if (!mysql.connect())
    {
        return -1;
    }
    if (!mysql.update(sql))
    {
        return -1;
    }
    return static_cast<long long>(mysql_insert_id(mysql.getConnection()));
}

long long MessageHistoryModal::insertGroup(
    const std::string &request_id,
    int sender_id,
    int group_id,
    const std::string &message)
{
    char sql[4096] = {0};
    sprintf(sql,
            "insert into message_history(request_id, sender_id, receiver_id, group_id, message, msg_type) "
            "values('%s', %d, NULL, %d, '%s', 'group')",
            request_id.c_str(), sender_id, group_id, message.c_str());

    MySQL mysql;
    if (!mysql.connect())
    {
        return -1;
    }
    if (!mysql.update(sql))
    {
        return -1;
    }
    return static_cast<long long>(mysql_insert_id(mysql.getConnection()));
}

long long MessageHistoryModal::queryMessageIdByRequestId(const std::string &request_id)
{
    if (request_id.empty())
    {
        return -1;
    }

    char sql[1024] = {0};
    sprintf(sql,
            "select id from message_history where request_id = '%s' order by id asc limit 1",
            request_id.c_str());

    MySQL mysql;
    if (!mysql.connect())
    {
        return -1;
    }

    MYSQL_RES *res = mysql.query(sql);
    if (res == nullptr)
    {
        return -1;
    }

    long long message_id = -1;
    MYSQL_ROW row = mysql_fetch_row(res);
    if (row != nullptr)
    {
        message_id = atoll(row[0]);
    }
    mysql_free_result(res);
    return message_id;
}

bool MessageHistoryModal::markRead(long long message_id, int userid)
{
    char sql[1024] = {0};
    sprintf(sql,
            "update message_history set read_state = 'read', read_at = now() "
            "where id = %lld and receiver_id = %d and read_state = 'unread'",
            message_id, userid);

    MySQL mysql;
    if (!mysql.connect())
    {
        return false;
    }
    return mysql.update(sql);
}

bool MessageHistoryModal::recallMessage(long long message_id, int operator_id)
{
    char sql[1024] = {0};
    sprintf(sql,
            "update message_history set recalled = 1, recalled_at = now(), message='[recalled]' "
            "where id = %lld and sender_id = %d",
            message_id, operator_id);

    MySQL mysql;
    if (!mysql.connect())
    {
        return false;
    }
    return mysql.update(sql);
}

std::vector<std::string> MessageHistoryModal::queryConversation(
    int user_a,
    int user_b,
    int limit,
    int offset)
{
    char sql[1024] = {0};
    sprintf(sql,
            "select id, sender_id, receiver_id, message, read_state, recalled, created_at "
            "from message_history where msg_type='direct' and "
            "((sender_id=%d and receiver_id=%d) or (sender_id=%d and receiver_id=%d)) "
            "order by id desc limit %d offset %d",
            user_a, user_b, user_b, user_a, limit, offset);

    std::vector<std::string> result;
    MySQL mysql;
    if (!mysql.connect())
    {
        return result;
    }

    MYSQL_RES *res = mysql.query(sql);
    if (res == nullptr)
    {
        return result;
    }

    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res)) != nullptr)
    {
        std::string line = "{";
        line += "\"id\":" + std::string(row[0] ? row[0] : "0");
        line += ",\"sender_id\":" + std::string(row[1] ? row[1] : "0");
        line += ",\"receiver_id\":" + std::string(row[2] ? row[2] : "0");
        line += ",\"message\":\"" + std::string(row[3] ? row[3] : "") + "\"";
        line += ",\"read_state\":\"" + std::string(row[4] ? row[4] : "unread") + "\"";
        line += ",\"recalled\":" + std::string(row[5] ? row[5] : "0");
        line += ",\"created_at\":\"" + std::string(row[6] ? row[6] : "") + "\"";
        line += "}";
        result.push_back(line);
    }

    mysql_free_result(res);
    return result;
}

std::vector<std::string> MessageHistoryModal::queryGroupConversation(
    int group_id,
    int limit,
    int offset)
{
    char sql[1024] = {0};
    sprintf(sql,
            "select id, sender_id, group_id, message, read_state, recalled, created_at "
            "from message_history where msg_type='group' and group_id=%d "
            "order by id desc limit %d offset %d",
            group_id, limit, offset);

    std::vector<std::string> result;
    MySQL mysql;
    if (!mysql.connect())
    {
        return result;
    }

    MYSQL_RES *res = mysql.query(sql);
    if (res == nullptr)
    {
        return result;
    }

    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res)) != nullptr)
    {
        std::string line = "{";
        line += "\"id\":" + std::string(row[0] ? row[0] : "0");
        line += ",\"sender_id\":" + std::string(row[1] ? row[1] : "0");
        line += ",\"group_id\":" + std::string(row[2] ? row[2] : "0");
        line += ",\"message\":\"" + std::string(row[3] ? row[3] : "") + "\"";
        line += ",\"read_state\":\"" + std::string(row[4] ? row[4] : "unread") + "\"";
        line += ",\"recalled\":" + std::string(row[5] ? row[5] : "0");
        line += ",\"created_at\":\"" + std::string(row[6] ? row[6] : "") + "\"";
        line += "}";
        result.push_back(line);
    }

    mysql_free_result(res);
    return result;
}
