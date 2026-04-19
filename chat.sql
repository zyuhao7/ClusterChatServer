CREATE DATABASE IF NOT EXISTS chat;
USE chat;

CREATE TABLE IF NOT EXISTS user (
    id INT PRIMARY KEY AUTO_INCREMENT,
    name VARCHAR(50) NOT NULL,
    password VARCHAR(255) NOT NULL,
    state VARCHAR(20) NOT NULL DEFAULT 'offline'
);

CREATE TABLE IF NOT EXISTS friend (
    userid INT NOT NULL,
    friendid INT NOT NULL,
    PRIMARY KEY (userid, friendid),
    CONSTRAINT chk_friend_not_self CHECK (userid <> friendid),
    CONSTRAINT fk_friend_user FOREIGN KEY (userid) REFERENCES user(id) ON DELETE CASCADE,
    CONSTRAINT fk_friend_friend FOREIGN KEY (friendid) REFERENCES user(id) ON DELETE CASCADE
);

CREATE TABLE IF NOT EXISTS user_blacklist (
    userid INT NOT NULL,
    blocked_userid INT NOT NULL,
    PRIMARY KEY (userid, blocked_userid),
    CONSTRAINT chk_blacklist_not_self CHECK (userid <> blocked_userid),
    CONSTRAINT fk_blacklist_user FOREIGN KEY (userid) REFERENCES user(id) ON DELETE CASCADE,
    CONSTRAINT fk_blacklist_blocked_user FOREIGN KEY (blocked_userid) REFERENCES user(id) ON DELETE CASCADE,
    INDEX idx_blacklist_blocked_userid (blocked_userid)
);

CREATE TABLE IF NOT EXISTS allgroup (
    id INT PRIMARY KEY AUTO_INCREMENT,
    groupname VARCHAR(50) NOT NULL,
    groupdesc VARCHAR(255) DEFAULT ''
);

CREATE TABLE IF NOT EXISTS groupuser (
    groupid INT NOT NULL,
    userid INT NOT NULL,
    grouprole VARCHAR(20) NOT NULL DEFAULT 'normal',
    PRIMARY KEY (groupid, userid),
    CONSTRAINT fk_groupuser_group FOREIGN KEY (groupid) REFERENCES allgroup(id) ON DELETE CASCADE,
    CONSTRAINT fk_groupuser_user FOREIGN KEY (userid) REFERENCES user(id) ON DELETE CASCADE,
    INDEX idx_groupuser_userid (userid),
    INDEX idx_groupuser_role (grouprole)
);

CREATE TABLE IF NOT EXISTS offlinemessage (
    id BIGINT PRIMARY KEY AUTO_INCREMENT,
    userid INT NOT NULL,
    message VARCHAR(1024) NOT NULL,
    request_id VARCHAR(64) DEFAULT '',
    created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    CONSTRAINT fk_offlinemessage_user FOREIGN KEY (userid) REFERENCES user(id) ON DELETE CASCADE,
    INDEX idx_offlinemsg_userid (userid),
    INDEX idx_offlinemsg_created_at (created_at)
);

CREATE TABLE IF NOT EXISTS message_history (
    id BIGINT PRIMARY KEY AUTO_INCREMENT,
    request_id VARCHAR(64) NOT NULL DEFAULT '',
    sender_id INT NOT NULL,
    receiver_id INT NULL,
    group_id INT NULL,
    message VARCHAR(1024) NOT NULL,
    msg_type ENUM('direct', 'group') NOT NULL,
    read_state ENUM('unread', 'read') NOT NULL DEFAULT 'unread',
    recalled TINYINT(1) NOT NULL DEFAULT 0,
    created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    read_at DATETIME NULL,
    recalled_at DATETIME NULL,
    CONSTRAINT fk_hist_sender FOREIGN KEY (sender_id) REFERENCES user(id) ON DELETE CASCADE,
    CONSTRAINT fk_hist_receiver FOREIGN KEY (receiver_id) REFERENCES user(id) ON DELETE CASCADE,
    CONSTRAINT fk_hist_group FOREIGN KEY (group_id) REFERENCES allgroup(id) ON DELETE CASCADE,
    INDEX idx_hist_sender_created (sender_id, created_at),
    INDEX idx_hist_receiver_created (receiver_id, created_at),
    INDEX idx_hist_group_created (group_id, created_at),
    INDEX idx_hist_request_id (request_id)
);

CREATE TABLE IF NOT EXISTS admin_audit_log (
    id BIGINT PRIMARY KEY AUTO_INCREMENT,
    actor VARCHAR(64) NOT NULL,
    action VARCHAR(64) NOT NULL,
    target_type VARCHAR(32) NOT NULL,
    target_id VARCHAR(64) NOT NULL,
    detail VARCHAR(1024) NOT NULL DEFAULT '',
    created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    INDEX idx_admin_audit_created_at (created_at)
);

CREATE TABLE IF NOT EXISTS admin_operation_log (
    id BIGINT PRIMARY KEY AUTO_INCREMENT,
    operator_name VARCHAR(64) NOT NULL,
    operation VARCHAR(128) NOT NULL,
    status VARCHAR(16) NOT NULL,
    detail VARCHAR(1024) NOT NULL DEFAULT '',
    created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    INDEX idx_admin_op_created_at (created_at)
);
