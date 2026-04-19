USE chat;

ALTER TABLE allgroup
    ADD COLUMN announcement VARCHAR(255) NOT NULL DEFAULT '';

ALTER TABLE groupuser
    ADD COLUMN muted_until DATETIME NULL;
