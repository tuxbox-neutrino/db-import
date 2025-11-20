/*
	mv2mariadb - convert MediathekView db to mariadb
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

#ifndef __SQL_H__
#define __SQL_H__

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include <string>

#include <mysql.h>

#include "common/helpers.h"
#include "mv2mariadb.h"

using namespace std;

#define executeSingleQueryString(a)   executeSingleQueryString__(a, __func__, __LINE__)
#define executeMultiQueryString(a)    executeMultiQueryString__(a, __func__, __LINE__)
#define setServerMultiStatementsOff() setServerMultiStatementsOff__(__func__, __LINE__)
#define setServerMultiStatementsOn()  setServerMultiStatementsOn__(__func__, __LINE__)

class CSql
{
	private:
		enum {
			db_mode_copy,
			db_mode_rename
		};

		MYSQL *mysqlCon;

		string dbDefaultCharacterSet;

		string VIDEO_DB;
		string VIDEO_DB_TMP_1;
		string VIDEO_DB_TEMPLATE;
		string VIDEO_TABLE;
		string INFO_TABLE;
		string VERSION_TABLE;

		string sqlUser;
		string sqlPW;

		void Init();
		void show_error(const char* func, int line);
		char checkStringBuff[0xFFFF];
		inline string checkString(string& str, int size) {
			size_t size_ = ((size_t)size > (sizeof(checkStringBuff)-1)) ? sizeof(checkStringBuff)-1 : size;
			string str2 = (str.length() > size_) ? str.substr(0, size_) : str;
//			memset(checkStringBuff, 0, sizeof(checkStringBuff));
			memset(checkStringBuff, 0, size_+1);
			mysql_real_escape_string(mysqlCon, checkStringBuff, str2.c_str(), str2.length());
			str2 = (string)checkStringBuff;
			return "'" + str2 + "'";
		}
		inline string checkInt(int i) { return to_string(i); }
		size_t searchInfoEntry(string &channel, vector<TVideoInfoEntry> &videoInfo);
		void updateInfoTable(vector<TVideoInfoEntry> &videoInfoUpdate, vector<TVideoInfoEntry> &videoInfo);

	public:
		bool multiQuery;

		CSql();
		~CSql();
		bool connectMysql();

		string createVideoTableQuery(int count, bool startRow, bool replace, TVideoEntry* videoEntry);
		string createInfoTableQuery(vector<TVideoInfoEntry> *videoInfo, int size, int diffMode);
		bool executeSingleQueryString__(string query, const char* func, int line);
		bool executeMultiQueryString__(string query, const char* func, int line);
		bool createVideoDbFromTemplate(string name);
		void checkTemplateDB(string name);
		bool createIndex(int drop);
		bool createTemplateDB(string name, bool quiet = false);
		bool renameDB();
		void setServerMultiStatementsOff__(const char* func, int line);
		void setServerMultiStatementsOn__(const char* func, int line);
		string getDefaultCharacterSet() { return dbDefaultCharacterSet; };
		uint32_t checkEntryForUpdate(TVideoEntry* videoEntry);
		bool debugChannelMapping(const string& pattern);

	/* sql-common.cpp */
	/* TODO: Separate class for shared sql functions */
	private:
		bool intCopyOrRenameDatabase(string fromDB, string toDB, string characterSet, int mode, bool noData=false);

	public:
		bool databaseExists(string db);
		uint32_t getTableEntries(string db, string table);
		uint32_t getLastIndex(string db, string table);
		string getUsedDatabase();
		void setUsedDatabase(string db);
		bool copyDatabase(string fromDB, string toDB, string characterSet, bool noData=false);
		bool renameDatabase(string fromDB, string toDB, string characterSet);
};

#endif // __SQL_H__
