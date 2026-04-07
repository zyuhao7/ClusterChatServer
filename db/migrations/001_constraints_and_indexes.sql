USE chat;

ALTER TABLE groupuser
    ADD PRIMARY KEY (groupid, userid);

ALTER TABLE groupuser
    ADD CONSTRAINT fk_groupuser_group FOREIGN KEY (groupid) REFERENCES allgroup(id) ON DELETE CASCADE,
    ADD CONSTRAINT fk_groupuser_user FOREIGN KEY (userid) REFERENCES user(id) ON DELETE CASCADE,
    ADD INDEX idx_groupuser_userid (userid),
    ADD INDEX idx_groupuser_role (grouprole);

ALTER TABLE offlinemessage
    ADD COLUMN id BIGINT PRIMARY KEY AUTO_INCREMENT FIRST,
    ADD COLUMN request_id VARCHAR(64) DEFAULT '' AFTER message,
    ADD COLUMN created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP AFTER request_id,
    ADD INDEX idx_offlinemsg_userid (userid),
    ADD INDEX idx_offlinemsg_created_at (created_at);
