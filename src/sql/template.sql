START TRANSACTION;
SET autocommit = 0;
DROP DATABASE IF EXISTS `@@@db_template@@@`;
CREATE DATABASE `@@@db_template@@@` DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
USE `@@@db_template@@@`;

CREATE TABLE `@@@tab_channelinfo@@@` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `channel` varchar(256) DEFAULT NULL,
  `count` int(11) DEFAULT NULL,
  `latest` int(11) DEFAULT NULL,
  `oldest` int(11) DEFAULT NULL,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE `@@@tab_version@@@` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `version` varchar(256) DEFAULT NULL,
  `vdate` int(11) DEFAULT NULL,
  `mvversion` varchar(256) DEFAULT NULL,
  `mvdate` int(11) DEFAULT NULL,
  `mventrys` int(11) DEFAULT NULL,
  `progname` varchar(256) DEFAULT NULL,
  `progversion` varchar(256) DEFAULT NULL,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE `@@@tab_video@@@` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `channel` varchar(128) DEFAULT NULL,
  `theme` varchar(1024) DEFAULT NULL,
  `title` varchar(1024) DEFAULT NULL,
  `duration` int(11) DEFAULT NULL,
  `size_mb` int(11) DEFAULT NULL,
  `description` text,
  `url` varchar(1024) DEFAULT NULL,
  `website` varchar(1024) DEFAULT NULL,
  `subtitle` varchar(1024) DEFAULT NULL,
  `url_rtmp` varchar(1024) DEFAULT NULL,
  `url_small` varchar(1024) DEFAULT NULL,
  `url_rtmp_small` varchar(1024) DEFAULT NULL,
  `url_hd` varchar(1024) DEFAULT NULL,
  `url_rtmp_hd` varchar(1024) DEFAULT NULL,
  `date_unix` int(11) DEFAULT NULL,
  `url_history` varchar(1024) DEFAULT NULL,
  `geo` varchar(1024) DEFAULT NULL,
  `parse_m3u8` int(11) NOT NULL DEFAULT '0',
  `new_entry` tinyint(1) DEFAULT NULL,
  `update` int(11) DEFAULT NULL,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

COMMIT;
