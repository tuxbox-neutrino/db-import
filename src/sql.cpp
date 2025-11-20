/*
	mv2mariadb - convert MediathekView db to mysql
	Copyright (C) 2015-2017, M. Liebmann 'micha-bbg'

	License: GPL

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public
	License as published by the Free Software Foundation; either
	version 2 of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
	General Public License for more details.

	You should have received a copy of the GNU General Public
	License along with this program; if not, write to the
	Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
	Boston, MA  02110-1301, USA.
*/

#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <getopt.h>
#include <libgen.h>

#include <mysqld_error.h>

#include <iostream>
#include <fstream>
#include <climits>

#include "sql.h"
#include "common/helpers.h"
#include "common/filehelpers.h"
#include "types.h"

extern GSettings		g_settings;
extern const char*		g_progName;
extern const char*		g_progVersion;
extern const char*		g_dbVersion;
extern string			g_mvVersion;
extern time_t			g_mvDate;
extern bool			g_debugPrint;
extern string			g_passwordFile;
extern string			g_mysqlHost;

extern void myExit(int val);

CSql::CSql()
{
	Init();
}

void CSql::Init()
{
	VIDEO_DB			= g_settings.videoDb;
	VIDEO_DB_TMP_1			= g_settings.videoDbTmp1;
	VIDEO_DB_TEMPLATE		= g_settings.videoDbTemplate;
	if (g_settings.testMode) {
		VIDEO_DB		+= g_settings.testLabel;
		VIDEO_DB_TMP_1		+= g_settings.testLabel;
		VIDEO_DB_TEMPLATE	+= g_settings.testLabel;
	}
	VIDEO_TABLE			= g_settings.videoDb_TableVideo;
	INFO_TABLE			= g_settings.videoDb_TableInfo;
	VERSION_TABLE			= g_settings.videoDb_TableVersion;

	multiQuery			= true;
	mysqlCon			= NULL;
	dbDefaultCharacterSet		= "DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci";
}

CSql::~CSql()
{
	if (mysqlCon != NULL) {
		int maxAllowedPacket = 4194304;			// default
		if (mysql_optionsv(mysqlCon, MYSQL_OPT_MAX_ALLOWED_PACKET, (const void*)(&maxAllowedPacket)) != 0)
			show_error(__func__, __LINE__);

		mysql_close(mysqlCon);
	}
}

void CSql::show_error(const char* func, int line)
{
	printf("\n[%s:%d] Error(%d) [%s] \"%s\"\n",
	       				    func, line,
					    mysql_errno(mysqlCon),
					    mysql_sqlstate(mysqlCon),
					    mysql_error(mysqlCon));
	mysql_close(mysqlCon);
	mysqlCon = NULL;
	myExit(-1);
}

bool CSql::connectMysql()
{
	FILE* f = NULL;
	if (file_exists(g_passwordFile.c_str()))
		f = fopen(g_passwordFile.c_str(), "r");
	if (f == NULL) {
		printf("#### [%s:%d] error opening pw file: %s\n", __func__, __LINE__, g_passwordFile.c_str());
		myExit(1);
	}
	char buf[256];
	fgets(buf, sizeof(buf), f);
	fclose(f);
	string pw = buf;
	pw = trim(pw);
	vector<string> v = split(pw, ':');
	sqlUser = v[0];
	sqlPW   = v[1];

	mysqlCon = mysql_init(NULL);

	int maxAllowedPacketDef = 4194304;			// default
	int maxAllowedPacket = maxAllowedPacketDef*64;
//	int maxAllowedPacket = maxAllowedPacketDef*256;		// max value
	if (mysql_optionsv(mysqlCon, MYSQL_OPT_MAX_ALLOWED_PACKET, (const void*)(&maxAllowedPacket)) != 0)
		show_error(__func__, __LINE__);

	unsigned long flags = 0;
	if (multiQuery)
		flags |= CLIENT_MULTI_STATEMENTS;
//	flags |= CLIENT_COMPRESS;
	const char* host = g_mysqlHost.empty() ? "127.0.0.1" : g_mysqlHost.c_str();
	if (!mysql_real_connect(mysqlCon, host, sqlUser.c_str(), sqlPW.c_str(), NULL, 3306, NULL, flags))
		show_error(__func__, __LINE__);

	if (mysql_set_character_set(mysqlCon, "utf8mb4") != 0)
		show_error(__func__, __LINE__);

	return true;
}

string CSql::createVideoTableQuery(int count, bool startRow, bool replace, TVideoEntry* videoEntry)
{
	string entry = "";
	if (startRow) {
		entry += (replace) ? "REPLACE" : "INSERT";
		entry += " INTO " + VIDEO_TABLE + " VALUES ";
	}
	else {
		entry += ",";
	}
	entry += "(";
	entry += checkInt(count) + ",";
	entry += checkString(videoEntry->channel, 128) + ",";
	entry += checkString(videoEntry->theme, 1024) + ",";
	entry += checkString(videoEntry->title, 1024) + ",";
	entry += checkInt(videoEntry->duration) + ",";
	entry += checkInt(videoEntry->size_mb) + ",";
	entry += checkString(videoEntry->description, 32768) + ",";
	entry += checkString(videoEntry->url, 1024) + ",";
	entry += checkString(videoEntry->website, 1024) + ",";
	entry += checkString(videoEntry->subtitle, 1024) + ",";
	entry += checkString(videoEntry->url_rtmp, 1024) + ",";
	entry += checkString(videoEntry->url_small, 1024) + ",";
	entry += checkString(videoEntry->url_rtmp_small, 1024) + ",";
	entry += checkString(videoEntry->url_hd, 1024) + ",";
	entry += checkString(videoEntry->url_rtmp_hd, 1024) + ",";
	entry += checkInt(videoEntry->date_unix) + ",";
	entry += checkString(videoEntry->url_history, 1024) + ",";
	entry += checkString(videoEntry->geo, 1024) + ",";
	entry += checkInt(0) + ",";
	entry += checkInt((int)videoEntry->new_entry) + ",";
	entry += checkInt((int)videoEntry->update);
	entry += ")";

	return entry;
}

size_t CSql::searchInfoEntry(string &channel, vector<TVideoInfoEntry> &videoInfo)
{
	for (size_t i = 0; i < videoInfo.size(); i++) {
		if (channel == videoInfo[i].channel)
			return i+1;
	}
	return 0;
}

void CSql::updateInfoTable(vector<TVideoInfoEntry> &videoInfoUpdate, vector<TVideoInfoEntry> &videoInfo)
{
	string entry = "SELECT * FROM " + INFO_TABLE + ";";
	executeSingleQueryString(entry);
	MYSQL_RES* result = mysql_store_result(mysqlCon);
	MYSQL_ROW row;
	while ((row = mysql_fetch_row(result))) {
		TVideoInfoEntry vie;
		vie.id      = atoi(row[0]);
		vie.channel = static_cast<string>(row[1]);
		vie.count   = atoi(row[2]);
		vie.latest  = atoi(row[3]);
		vie.oldest  = atoi(row[4]);
		size_t idx  = searchInfoEntry(vie.channel, videoInfo);
		if (idx > 0) {
			idx        -= 1;
			vie.count  += videoInfo[idx].count;
			vie.latest  = max(vie.latest, videoInfo[idx].latest);
			vie.oldest  = min(vie.oldest, videoInfo[idx].oldest);
		}
		videoInfoUpdate.push_back(vie);
	}
	mysql_free_result(result);
}

string CSql::createInfoTableQuery(vector<TVideoInfoEntry> *videoInfo, int size, int diffMode)
{
	vector<TVideoInfoEntry> videoInfoUpdate;
	vector<TVideoInfoEntry> *videoInfoTmp;
	string entry = "";
	
	if (diffMode > diffMode_none) {
		updateInfoTable(videoInfoUpdate, *videoInfo);
		videoInfoTmp = &videoInfoUpdate;
	}
	else {
		videoInfoTmp = videoInfo;
	}

	for (vector<TVideoInfoEntry>::iterator it = videoInfoTmp->begin(); it != videoInfoTmp->end(); ++it) {
		if (diffMode > diffMode_none) {
			entry += "REPLACE INTO " + INFO_TABLE + " (id, channel, count, latest, oldest) VALUES (";
			entry += checkInt(it->id) + ", ";
		}
		else {
			entry += "INSERT INTO " + INFO_TABLE + " (channel, count, latest, oldest) VALUES (";
		}
		entry += checkString(it->channel, 256) + ", ";
		entry += checkInt(it->count) + ", ";
		entry += checkInt(it->latest) + ", ";
		entry += checkInt(it->oldest);
		entry += ");";
	}
	videoInfo->clear();
	if (diffMode > diffMode_none) {
		videoInfoUpdate.clear();
	}

	entry += (diffMode > diffMode_none) ? "REPLACE" : "INSERT";
	entry += " INTO " + VERSION_TABLE + " (id, version, vdate, mvversion, mvdate, mventrys, progname, progversion) VALUES (";
	entry += checkInt(1) + ", ";
	string tmpStr = (string)g_dbVersion;
	entry += checkString(tmpStr, 256) + ", ";
	entry += checkInt(time(0)) + ", ";
	entry += checkString(g_mvVersion, 256) + ", ";
	entry += checkInt(g_mvDate) + ", ";
	entry += checkInt(size) + ", ";
	tmpStr = (string)g_progName;
	entry += checkString(tmpStr, 256) + ", ";
	tmpStr = (string)g_progVersion;
	entry += checkString(tmpStr, 256);
	entry += ");";

	return entry;
}

bool CSql::executeSingleQueryString__(string query, const char* func, int line)
{
	bool ret = true;

	if (mysql_real_query(mysqlCon, query.c_str(), query.length()) != 0)
		show_error(func, line);

	return ret;
}

bool CSql::executeMultiQueryString__(string query, const char* func, int line)
{
	if (!multiQuery) {
		printf("[%s:%d] No multiple statement execution support.\n", func, line);
		myExit(1);
	}
	setServerMultiStatementsOn();

	int status = mysql_real_query(mysqlCon, query.c_str(), query.length());
	if (status)
		show_error(func, line);
	
	bool ret = true;
	/* process each statement result */
	do {
		ret = true;
		/* did current statement return data? */
		MYSQL_RES *result = mysql_store_result(mysqlCon);
		if (result) {
			/* yes; free the result set */
			mysql_free_result(result);
		}
		else {	/* no result set or error */
			if (mysql_field_count(mysqlCon) != 0) {
				/* some error occurred */
				printf("Could not retrieve result set\n");
				ret = false;
				break;
			}
		}
		/* more results? -1 = no, >0 = error, 0 = yes (keep looping) */
		if ((status = mysql_next_result(mysqlCon)) > 0) {
//			printf("Could not execute statement\n");
//			ret = false;
		}
	} while (status == 0);

	return ret;
}

bool CSql::createVideoDbFromTemplate(string name)
{
	return copyDatabase(VIDEO_DB_TEMPLATE, name, dbDefaultCharacterSet, true/* no data */);
}

void CSql::checkTemplateDB(string name)
{
	if (multiQuery)
		setServerMultiStatementsOff();

	bool dbExists = databaseExists(VIDEO_DB_TEMPLATE);
	if (dbExists && g_debugPrint)
		printf("[%s-debug] check i.o., database [%s] exists.\n", g_progName, VIDEO_DB_TEMPLATE.c_str());

	if (!dbExists) {
		bool ret = createTemplateDB(name, true);
		if (g_debugPrint && ret)
			printf("[%s-debug] database [%s] successfully created.\n", g_progName, VIDEO_DB_TEMPLATE.c_str());
	}

	if (multiQuery)
		setServerMultiStatementsOn();
}

bool CSql::createIndex(int drop)
{
	struct timeval t1;
	double nowDTms;
	double workDTms;

	string sql = "";
	gettimeofday(&t1, NULL);
	nowDTms = (double)t1.tv_sec*1000ULL + ((double)t1.tv_usec)/1000ULL;
	if (drop > diffMode_none) {
		printf("[%s] update indexes on database...", g_progName);
		fflush(stdout);

		sql = "";
		sql += "START TRANSACTION;";
		sql += "SET autocommit = 0;";
		sql += "USE `" + VIDEO_DB + "`;";

		sql += "ALTER TABLE `" + VIDEO_TABLE + "` DROP INDEX `channel`;";
		sql += "ALTER TABLE `" + VIDEO_TABLE + "` DROP INDEX `date_unix`;";
		sql += "ALTER TABLE `" + VIDEO_TABLE + "` DROP INDEX `duration`;";
		sql += "ALTER TABLE `" + VIDEO_TABLE + "` DROP INDEX `theme`;";
		sql += "ALTER TABLE `" + VIDEO_TABLE + "` DROP INDEX `title`;";

		sql += "COMMIT;";
		executeMultiQueryString(sql);
	}
	else {
		printf("[%s] create indexes on database...", g_progName);
		fflush(stdout);
	}

	sql = "";
	sql += "START TRANSACTION;";
	sql += "SET autocommit = 0;";
	sql += "USE `" + VIDEO_DB + "`;";

	sql += "ALTER TABLE `" + VIDEO_TABLE + "` ADD INDEX `channel` (`channel`);";
	sql += "ALTER TABLE `" + VIDEO_TABLE + "` ADD INDEX `date_unix` (`date_unix`);";
	sql += "ALTER TABLE `" + VIDEO_TABLE + "` ADD INDEX `duration` (`duration`);";
#if 1
	sql += "ALTER TABLE `" + VIDEO_TABLE + "` ADD INDEX `theme` (`theme`(128));";
	sql += "ALTER TABLE `" + VIDEO_TABLE + "` ADD INDEX `title` (`title`(128));";
#else
	sql += "ALTER TABLE `" + VIDEO_TABLE + "` ADD FULLTEXT INDEX `title` (`title`);";
	sql += "ALTER TABLE `" + VIDEO_TABLE + "` ADD FULLTEXT INDEX `theme` (`theme`);";
#endif
	sql += "COMMIT;";
	bool ret = executeMultiQueryString(sql);

	gettimeofday(&t1, NULL);
	workDTms = (double)t1.tv_sec*1000ULL + ((double)t1.tv_usec)/1000ULL;
	printf("done (%.02f sec)\n", (workDTms-nowDTms)/1000); fflush(stdout);

	return ret;
}

bool CSql::createTemplateDB(string name, bool quiet/* = false*/)
{
	size_t size  = 0;
	size_t size_ = 0;
	char* buf = NULL;
	if (file_exists(name.c_str())) {
		FILE* f = fopen(name.c_str(), "r");
		if (f != NULL) {
			size = file_size(name.c_str());
			buf = new char[static_cast<unsigned long>(size+1)];
			if (buf == NULL) {
				printf("\n[%s] memory error read database template [%s]\n", g_progName, name.c_str());
				fclose(f);
				myExit(1);
			}
			size_ = fread((void*)buf, size, 1, f);
			buf[size] = '\0';
			fclose(f);
		}

	}
	if (size_ == 0) {
		printf("\n[%s] error read database template [%s]\n", g_progName, name.c_str());
		if (buf != NULL)
			delete [] buf;
		myExit(1);
	}
	string sql = static_cast<string>(buf);
	delete [] buf;

	string search = "@@@db_template@@@";
	sql = str_replace(search, VIDEO_DB_TEMPLATE, sql);
	search = "@@@tab_channelinfo@@@";
	sql = str_replace(search, INFO_TABLE, sql);
	search = "@@@tab_version@@@";
	sql = str_replace(search, VERSION_TABLE, sql);
	search = "@@@tab_video@@@";
	sql = str_replace(search, VIDEO_TABLE, sql);

	bool ret = executeMultiQueryString(sql);
	if (!quiet)
		printf("\n[%s] database [%s] successfully created or updated.\n", g_progName, VIDEO_DB_TEMPLATE.c_str());
	return ret;
}

bool CSql::renameDB()
{
	struct timeval t1;
	gettimeofday(&t1, NULL);
	double nowDTms = (double)t1.tv_sec*1000ULL + ((double)t1.tv_usec)/1000ULL;
	printf("[%s] rename temporary database...", g_progName); fflush(stdout);

	bool ret = renameDatabase(VIDEO_DB_TMP_1, VIDEO_DB, dbDefaultCharacterSet);

	gettimeofday(&t1, NULL);
	double workDTms = (double)t1.tv_sec*1000ULL + ((double)t1.tv_usec)/1000ULL;
	printf("done (%.02f sec)\n", (workDTms-nowDTms)/1000); fflush(stdout);

	return ret;
}

void CSql::setServerMultiStatementsOff__(const char* func, int line)
{
	if (mysql_set_server_option(mysqlCon, MYSQL_OPTION_MULTI_STATEMENTS_OFF) != 0)
		show_error(func, line);
}

void CSql::setServerMultiStatementsOn__(const char* func, int line)
{
	if (mysql_set_server_option(mysqlCon, MYSQL_OPTION_MULTI_STATEMENTS_ON) != 0)
		show_error(func, line);
}

uint32_t CSql::checkEntryForUpdate(TVideoEntry* videoEntry)
{
	string sChannel = checkString(videoEntry->channel, 128);
	string sTheme   = checkString(videoEntry->theme, 1024);
	string sTitle   = checkString(videoEntry->title, 1024);
	string sDate    = checkInt(videoEntry->date_unix);

	string sql = "";
	sql += "SELECT MAX(id) FROM ( ";
		sql += "SELECT id, theme, title FROM " + g_settings.videoDb_TableVideo;
		sql += " WHERE ( channel LIKE " + sChannel;
		sql += " AND date_unix = " + sDate + " )";
	sql += " ) AS dingens WHERE ( theme LIKE " + sTheme + " AND title LIKE " + sTitle + " );";

	executeSingleQueryString(sql);

	MYSQL_RES* result = mysql_store_result(mysqlCon);
	MYSQL_ROW row;
	uint32_t id_ = 0;
	uint32_t rowsCount = mysql_num_fields(result);
	if (rowsCount > 0) {
		row = mysql_fetch_row(result);
		if ((row != NULL) && (row[0] != NULL))
			id_ = atoi(row[0]);
	}
	mysql_free_result(result);

	return id_;
}

bool CSql::debugChannelMapping(const string& pattern)
{
	string likePattern = "%" + pattern + "%";
	string sql = "";
	sql += "SELECT v.channel, ci.channel AS mapped, COUNT(*) AS cnt ";
	sql += "FROM " + VIDEO_DB + "." + VIDEO_TABLE + " v ";
	sql += "LEFT JOIN " + VIDEO_DB + "." + INFO_TABLE + " ci ON v.channel = ci.channel ";
	sql += "WHERE LOWER(v.channel) LIKE LOWER(" + checkString(likePattern, 255) + ") ";
	sql += "GROUP BY v.channel, ci.channel ";
	sql += "ORDER BY cnt DESC;";

	if (!executeSingleQueryString(sql))
		return false;

	MYSQL_RES* result = mysql_store_result(mysqlCon);
	if (result == NULL)
		return false;

	printf("[%s] Channel mapping for pattern \"%s\":\n", g_progName, pattern.c_str());
	MYSQL_ROW row;
	while ((row = mysql_fetch_row(result))) {
		const char* src = (row[0] != NULL) ? row[0] : "";
		const char* mapped = (row[1] != NULL) ? row[1] : "(null)";
		const char* cnt = (row[2] != NULL) ? row[2] : "0";
		printf("  %-30s -> %-30s (%s)\n", src, mapped, cnt);
	}
	mysql_free_result(result);
	return true;
}

/* TODO: Separate class for shared sql functions */
#include "sql-common.cpp"
