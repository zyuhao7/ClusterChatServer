#ifndef MESSAGEHISTORYMODAL_H
#define MESSAGEHISTORYMODAL_H

#include <string>
#include <vector>

class MessageHistoryModal
{
public:
    long long insertDirect(
        const std::string &request_id,
        int sender_id,
        int receiver_id,
        const std::string &message);

    long long insertGroup(
        const std::string &request_id,
        int sender_id,
        int group_id,
        const std::string &message);

    long long queryMessageIdByRequestId(const std::string &request_id);

    bool markRead(long long message_id, int userid);
    bool recallMessage(long long message_id, int operator_id);

    std::vector<std::string> queryConversation(
        int user_a,
        int user_b,
        int limit,
        int offset,
        bool ascending);

    std::vector<std::string> queryGroupConversation(
        int group_id,
        int limit,
        int offset,
        bool ascending);
};

#endif
