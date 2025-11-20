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

#ifndef __MV2MYSQL_H__
#define __MV2MYSQL_H__

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include <string>

#include "common/helpers.h"
#include "common/rapidjsonsax.h"
#include "configfile.h"
#include "types.h"

using namespace std;

string msgHead(string deb="");
string msgHeadDebug();
string msgHeadFuncLine();

class CSql;

#define list0Count 5
#define movieEntryCount 20

class CMV2Mysql
{
	private:
		bool multiQuery;
		CSql* csql;

		string jsondb;
		string jsonBuf;
		string workDir;
		int epoch;
		int cronMode;
		bool cronModeEcho;
		bool createIndexes;
		string configFileName;
		CConfigFile configFile;
		bool downloadOnly;
		bool loadServerlist;
		string defaultXZ;
		string defaultDiffXZ;
		bool convertData;
		bool forceConvertData;
		string debugChannelPattern;
		uint32_t dlSegmentSize;
		int diffMode;

		int count_parser;
		int keyCount_parser;
		TVideoInfoEntry videoInfoEntry;
		time_t nowTime;
		uint32_t movieEntries;
		uint32_t movieEntriesCounter;
		uint32_t skippedUrls;
		string videoEntrySqlBuf;
		string cName;
		string tName;
		int cCount;
		uint32_t writeLen;
		bool writeStart;
		uint32_t maxWriteLen;
		string dbVersionInfo;
		int dbVersionInfoCount;
		size_t insertEntries;

		typedef struct {
			string entry;
			string asString() { return entry; }
			const char* asCString() { return entry.c_str(); }
			int asInt() { return atoi(entry.c_str()); }
			bool asBool() { return ((entry != "false") && (entry != "FALSE") && (entry != "0")); }
		} movieEntryElement_t;
#if 0
		typedef struct {
			const char* entry;
			string asString() { return static_cast<string>(entry); }
			const char* asCString() { return entry; }
			int asInt() { return atoi(entry); }
			bool asBool() { return (!((strcmp(entry, "false") == 0) || (strcmp(entry, "FALSE") == 0) || (strcmp(entry, "0") == 0))); }
		} movieEntryElement_cstr_t;
#endif
		typedef struct {
			movieEntryElement_t el[list0Count];
		} list0Entry_t;

		typedef struct {
			movieEntryElement_t el[movieEntryCount];
		} list1Entry_t;

		typedef struct {
			movieEntryElement_t el[movieEntryCount];
		} movieEntry_t;

		list0Entry_t list0Entry;
		list1Entry_t list1Entry;
		movieEntry_t movieEntry;
		vector<TVideoEntry> videoEntriesNew;

		string	jsonDbName;
		string	xzName;
		string	templateDBFile;
		vector<TVideoInfoEntry> videoInfo;
		string VIDEO_DB_TMP_1;
		string VIDEO_DB;
		string userAgentCheck;
		string userAgentDownload;
		string userAgentListCheck;

		void Init();
		void printHeader();
		void printCopyright();
		void printHelp();
		long getDbVersion(string file);
		bool checkNumberList(vector<uint32_t>* numberList, uint32_t number);
		bool getDownloadUrlList();
		long getVersionFromXZ(string xz_, string json_);
		bool downloadDB(string url);
		double startTimer();
		string getTimer_str(double startTime, string txt, int preci=3);
		double getTimer_double(double startTime);
		bool readEntry(int index);
		static void verCallback(int type, string data, int parseMode, CRapidJsonSAX* instance);
		static void parseCallback(int type, string data, int parseMode, CRapidJsonSAX* instance);
		void parseCallbackInternal(int type, string data, int parseMode, CRapidJsonSAX* instance);
		size_t insertNewEntries();
		bool parseDB();
		string convertUrl(string url1, string url2);
		void checkDiffMode();

		int loadSetup(string fname);
		void saveSetup(string fname, bool quiet = false);
		void setDbFileNames(string xz);

	public:
		CMV2Mysql();
		~CMV2Mysql();
		int run(int argc, char *argv[]);

		void loadDownloadServerSetup();
		void saveDownloadServerSetup();
		CConfigFile* getConfig() { return &configFile; };

};

#endif // __MV2MYSQL_H__
