USE chat;

CREATE TABLE IF NOT EXISTS user_blacklist (
    userid INT NOT NULL,
    blocked_userid INT NOT NULL,
    PRIMARY KEY (userid, blocked_userid),
    CONSTRAINT chk_blacklist_not_self CHECK (userid <> blocked_userid),
    CONSTRAINT fk_blacklist_user FOREIGN KEY (userid) REFERENCES user(id) ON DELETE CASCADE,
    CONSTRAINT fk_blacklist_blocked_user FOREIGN KEY (blocked_userid) REFERENCES user(id) ON DELETE CASCADE,
    INDEX idx_blacklist_blocked_userid (blocked_userid)
);
