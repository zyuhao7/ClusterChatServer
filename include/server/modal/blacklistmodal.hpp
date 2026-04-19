#ifndef BLACKLISTMODAL_H
#define BLACKLISTMODAL_H

class BlacklistModal
{
public:
    bool insert(int userid, int blocked_userid);
    bool remove(int userid, int blocked_userid);
    bool isBlocked(int userid, int blocked_userid);
};

#endif
