CREATE DATABASE IF NOT EXISTS chat;
USE chat;

CREATE TABLE IF NOT EXISTS user (
    id INT PRIMARY KEY AUTO_INCREMENT,
    name VARCHAR(50) NOT NULL,
    password VARCHAR(50) NOT NULL,
    state VARCHAR(20) NOT NULL DEFAULT 'offline'
);

CREATE TABLE IF NOT EXISTS friend (
    userid INT NOT NULL,
    friendid INT NOT NULL
);

CREATE TABLE IF NOT EXISTS allgroup (
    id INT PRIMARY KEY AUTO_INCREMENT,
    groupname VARCHAR(50) NOT NULL,
    groupdesc VARCHAR(255) DEFAULT ''
);

CREATE TABLE IF NOT EXISTS groupuser (
    groupid INT NOT NULL,
    userid INT NOT NULL,
    grouprole VARCHAR(20) NOT NULL DEFAULT 'normal'
);

CREATE TABLE IF NOT EXISTS offlinemessage (
    userid INT NOT NULL,
    message VARCHAR(1024) NOT NULL
);
