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

#define PROGVERSION "0.4.1"
#define DBVERSION "3.0"
#define PROGNAME "mv2mariadb"
#define DEFAULTXZ "mv-movielist.xz"
#define DEFAULTDIFFXZ "mv-difflist.xz"

#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <getopt.h>
#include <libgen.h>
#include <errno.h>
#include <semaphore.h>
#include <signal.h>

#include <iostream>
#include <fstream>
#include <climits>
#include <cassert>
#include <exception>
#include <iomanip>
#include <sstream>

#include "sql.h"
#include "mv2mariadb.h"
#include "common/helpers.h"
#include "common/filehelpers.h"
#include "lzma_dec.h"
#include "curl.h"
#include "serverlist.h"

CMV2Mysql*		g_mainInstance;
GSettings		g_settings;
bool			g_debugPrint;
const char*		g_progName;
const char*		g_progCopyright;
const char*		g_progVersion;
const char*		g_dbVersion;
string			g_mvVersion;
time_t			g_mvDate;
string			g_passwordFile;
string			g_mysqlHost;

void myExit(int val);

string msgHead(string deb/*=""*/)
{
	return static_cast<string>("[") + g_progName + deb + "] ";
}

string msgHeadDebug()
{
	return msgHead("-debug");
}

string msgHeadFuncLine()
{
	return static_cast<string>("[") + __func__ + ":" + to_string(__LINE__) + "] ";
}

CMV2Mysql::CMV2Mysql()
: configFile('\t')
{
	Init();
}

void CMV2Mysql::Init()
{
	g_progName		= PROGNAME;
	g_progCopyright		= "Copyright (C) 2015-2017, M. Liebmann 'micha-bbg'";
	g_progVersion		= "v" PROGVERSION;
	g_dbVersion		= DBVERSION;
	defaultXZ		= (string)DEFAULTXZ;
	defaultDiffXZ		= (string)DEFAULTDIFFXZ;

	epoch			= 0; /* all data */
	cronMode		= 0;
	cronModeEcho		= false;
	g_debugPrint		= false;
	multiQuery		= true;
	downloadOnly		= false;
	createIndexes		= true;
	loadServerlist		= false;
	g_mvDate		= time(0);
	nowTime			= time(0);
	csql			= NULL;
	convertData		= true;
	forceConvertData	= false;
	dlSegmentSize		= 8192;
	diffMode		= diffMode_none;
	insertEntries		= 0;

	count_parser		= 0;
	keyCount_parser		= 0;
	videoInfoEntry.latest	= INT_MIN;
	videoInfoEntry.oldest	= INT_MAX;
	movieEntries		= 0;
	movieEntriesCounter	= 0;
	skippedUrls		= 0;
	cName			= "";
	tName			= "";
	videoEntrySqlBuf	= "";
	cCount			= 0;
	writeLen		= 0;
	writeStart		= true;
	maxWriteLen		= 1048576-4096;	/* 1MB */
//	maxWriteLen		= 524288;	/* 512KB */
	dbVersionInfoCount	= 0;


#ifdef PRIV_USERAGENT
	string agentTmp1	= "MediathekView data to sql - ";
	string agentTmp2a	= "versionscheck ";
	string agentTmp2b	= "downloader ";
	string agentTmp2c	= "listcheck ";
	string agentTmp3	= "(" + (string)g_progName + "/" + (string)PROGVERSION + ", curl/" + (string)LIBCURL_VERSION + ")";
	userAgentCheck		= agentTmp1 + agentTmp2a + agentTmp3;
	userAgentDownload	= agentTmp1 + agentTmp2b + agentTmp3;
	userAgentListCheck	= agentTmp1 + agentTmp2c + agentTmp3;
#else
	userAgentCheck		= "";
	userAgentDownload	= "";
	userAgentListCheck	= "";
#endif
}

CMV2Mysql::~CMV2Mysql()
{
	configFile.setModifiedFlag(true);
	unlink(configFileName.c_str());
	saveSetup(configFileName, true);
	videoInfo.clear();
	if (csql != NULL)
		delete csql;
}

void CMV2Mysql::printHeader()
{
	printf("%s %s\n", g_progName, g_progVersion);
}

void CMV2Mysql::printCopyright()
{
	printf("%s\n", g_progCopyright);
}

int CMV2Mysql::loadSetup(string fname)
{
	int erg = 0;
	if (!configFile.loadConfig(fname.c_str()))
		/* file not exist */
		erg = 1;

	/* test mode */
	g_settings.testLabel		= configFile.getString("testLabel",            "_TEST");
	g_settings.testMode		= configFile.getBool  ("testMode",             true);

	/* database */
	g_settings.videoDbBaseName		= configFile.getString("videoDbBaseName",          "mediathek_1");
	g_settings.videoDb			= configFile.getString("videoDb",                  g_settings.videoDbBaseName);
	g_settings.videoDbTmp1			= configFile.getString("videoDbTmp1",              g_settings.videoDbBaseName + "_tmp1");
	g_settings.videoDbTemplate		= configFile.getString("videoDbTemplate",          g_settings.videoDbBaseName + "_template");
	g_settings.videoDb_TableVideo		= configFile.getString("videoDb_TableVideo",       "video");
	g_settings.videoDb_TableInfo		= configFile.getString("videoDb_TableInfo",        "channelinfo");
	g_settings.videoDb_TableVersion		= configFile.getString("videoDb_TableVersion",     "version");
	g_settings.mysqlHost			= configFile.getString("mysqlHost",                "db");
	VIDEO_DB_TMP_1				= g_settings.videoDbTmp1;
	VIDEO_DB				= g_settings.videoDb;
	if (g_settings.testMode) {
		VIDEO_DB_TMP_1	+= g_settings.testLabel;
		VIDEO_DB	+= g_settings.testLabel;
	}

	/* download server */
	loadDownloadServerSetup();

	/* password file */
	g_settings.passwordFile		= configFile.getString("passwordFile",         "pw_mariadb");

	/* server list */
	g_settings.serverListUrl	 = configFile.getString("serverListUrl",               "https://res.mediathekview.de/akt.xml");
	g_settings.serverListLastRefresh = (time_t)configFile.getInt64("serverListLastRefresh", 0);
	g_settings.serverListRefreshDays = configFile.getInt32("serverListRefreshDays",         7);

	if (erg)
		configFile.setModifiedFlag(true);
	return erg;
}

void CMV2Mysql::saveSetup(string fname, bool quiet/*=false*/)
{
	/* test mode */
	configFile.setString("testLabel",            g_settings.testLabel);
	configFile.setBool  ("testMode",             g_settings.testMode);

	/* database */
	configFile.setString("videoDbBaseName",          g_settings.videoDbBaseName);
	configFile.setString("videoDb",                  g_settings.videoDb);
	configFile.setString("videoDbTmp1",              g_settings.videoDbTmp1);
	configFile.setString("videoDbTemplate",          g_settings.videoDbTemplate);
	configFile.setString("videoDb_TableVideo",       g_settings.videoDb_TableVideo);
	configFile.setString("videoDb_TableInfo",        g_settings.videoDb_TableInfo);
	configFile.setString("videoDb_TableVersion",     g_settings.videoDb_TableVersion);
	configFile.setString("mysqlHost",                g_settings.mysqlHost);

	/* download server */
	saveDownloadServerSetup();

	/* password file */
	configFile.setString("passwordFile",         g_settings.passwordFile);

	/* server list */
	configFile.setString("serverListUrl",         g_settings.serverListUrl);
	configFile.setInt64 ("serverListLastRefresh", (int64_t)(g_settings.serverListLastRefresh));
	configFile.setString("serverListLastRefreshStr", time2str(g_settings.serverListLastRefresh));
	configFile.setInt32 ("serverListRefreshDays", g_settings.serverListRefreshDays);

	if (configFile.getModifiedFlag())
		configFile.saveConfig(fname.c_str(), '=', quiet);
}

void CMV2Mysql::loadDownloadServerSetup()
{
	char cfg_key[256];
	int count					= configFile.getInt32("downloadServerCount", 1);
	g_settings.downloadServerCount			= max(count, 1);
	g_settings.downloadServerCount			= min(count, static_cast<int>(maxDownloadServerCount));
	count						= configFile.getInt32("lastDownloadServer", 1);
	g_settings.lastDownloadServer			= max(count, 1);
	g_settings.lastDownloadServer			= min(count, g_settings.downloadServerCount);
	g_settings.lastDownloadTime			= (time_t)configFile.getInt64("lastDownloadTime", 0);
	g_settings.lastDiffDownloadTime			= (time_t)configFile.getInt64("lastDiffDownloadTime", 0);
	g_settings.aktFileName				= configFile.getString("aktFileName", "Filmliste-akt.xz");
	g_settings.diffFileName				= configFile.getString("diffFileName", "Filmliste-diff.xz");
	g_settings.downloadServerConnectFailsMax	= configFile.getInt32("downloadServerConnectFailsMax", 3);
	for (int i = 1; i <= g_settings.downloadServerCount; i++) {
		memset(cfg_key, 0, sizeof(cfg_key));
		snprintf(cfg_key, sizeof(cfg_key), "downloadServer_%02d", i);
		g_settings.downloadServer[i] = configFile.getString(cfg_key, "-");
		memset(cfg_key, 0, sizeof(cfg_key));
		snprintf(cfg_key, sizeof(cfg_key), "downloadServerConnectFail_%02d", i);
		g_settings.downloadServerConnectFail[i] = configFile.getInt32(cfg_key, 0);
	}
}

void CMV2Mysql::saveDownloadServerSetup()
{
	char cfg_key[256];
	configFile.setInt32 ("downloadServerCount",           g_settings.downloadServerCount);
	configFile.setInt32 ("lastDownloadServer",            g_settings.lastDownloadServer);
	configFile.setInt64 ("lastDownloadTime",              (int64_t)(g_settings.lastDownloadTime));
	configFile.setString("lastDownloadTimeStr",           time2str(g_settings.lastDownloadTime));
	configFile.setInt64 ("lastDiffDownloadTime",          (int64_t)(g_settings.lastDiffDownloadTime));
	configFile.setString("lastDiffDownloadTimeStr",       time2str(g_settings.lastDiffDownloadTime));
	configFile.setString("aktFileName",                   g_settings.aktFileName);
	configFile.setString("diffFileName",                  g_settings.diffFileName);
	configFile.setInt32 ("downloadServerConnectFailsMax", g_settings.downloadServerConnectFailsMax);
	for (int i = 1; i <= g_settings.downloadServerCount; i++) {
		memset(cfg_key, 0, sizeof(cfg_key));
		snprintf(cfg_key, sizeof(cfg_key), "downloadServer_%02d", i);
		configFile.setString(cfg_key, g_settings.downloadServer[i]);
		memset(cfg_key, 0, sizeof(cfg_key));
		snprintf(cfg_key, sizeof(cfg_key), "downloadServerConnectFail_%02d", i);
		configFile.setInt32(cfg_key, g_settings.downloadServerConnectFail[i]);
	}
}

void CMV2Mysql::printHelp()
{
	printHeader();
	printCopyright();
	printf("  -e | --epoch xxx	 => Use not older entrys than 'xxx' days\n");
	printf("			    (default all data)\n");
	printf("  -f | --force-convert	 => Data also convert, when\n");
	printf("			    movie list is up-to-date.\n");
	printf("  -c | --cron-mode xxx	 => 'xxx' = time in minutes. Specifies the period during\n");
	printf("			    which no new version check is performed\n");
	printf("			    after the last download.\n");
	printf("  -C | --cron-mode-echo	 => Output message during --cron-mode to the log\n");
	printf("			    (Default: no output)\n");
	printf("  -D | --diff-mode	 => Use difference list instead of the complete movie list\n");
	printf("  -n | --no-indexes	 => Don't create indexes for database\n");
	printf("       --update		 => Create new config file and\n");
	printf("			    new template database, then exit.\n");
	printf("       --download-only	 => Download only (Don't convert\n");
	printf("			    to sql database).\n");
	printf("       --load-serverlist => Load new serverlist and exit.\n");

	printf("\n");
	printf("  -d | --debug-print	 => Print debug info\n");
	printf("  -v | --version	 => Display versions info and exit\n");
	printf("  -h | --help		 => Display this help screen and exit\n");
}

void CMV2Mysql::checkDiffMode()
{
	string format  = "%d.%m.%Y";
	string today_S = time2str(nowTime, format) + " 00:00";
	format += " %H:%M";
	time_t today = str2time(format, today_S);
	if (g_settings.lastDownloadTime < today)
		diffMode = diffMode_none;
}

int CMV2Mysql::run(int argc, char *argv[])
{
	/* Initialization random number generator */
	srand((uint32_t)time(0));

	/* set name for configFileName */
	string arg0         = (string)argv[0];
	string path0        = getPathName(arg0);
	path0               = getRealPath(path0);
	configFileName      = path0 + "/" + getBaseName(arg0) + ".conf";
	templateDBFile      = path0 + "/sql/"+ "template.sql";
	workDir             = path0 + "/dl/work";
	CFileHelpers cfh;
	if (!cfh.createDir(workDir, 0755)) {
		printf("Error: create dir %s\n", workDir.c_str());
		myExit(1);
	}

	int loadSettingsErg = loadSetup(configFileName);

	if (loadSettingsErg) {
		configFile.setModifiedFlag(true);
		saveSetup(configFileName);
	}

	/* set name for passwordFile */
	if ((g_settings.passwordFile)[0] == '/')
		g_passwordFile = g_settings.passwordFile;
	else
		g_passwordFile = path0 + "/" + g_settings.passwordFile;
	g_mysqlHost = g_settings.mysqlHost;

	csql = new CSql();
	multiQuery = csql->multiQuery;

	int noParam       = 0;
	int requiredParam = 1;
//	int optionalParam = 2;
	static struct option long_options[] = {
		{"epoch",		requiredParam, NULL, 'e'},
		{"force-convert",	noParam,       NULL, 'f'},
		{"cron-mode",		requiredParam, NULL, 'c'},
		{"cron-mode-echo",	noParam,       NULL, 'C'},
		{"diff-mode",		requiredParam, NULL, 'D'},
		{"no-indexes",		noParam,       NULL, 'n'},
		{"update",		noParam,       NULL, '1'},
		{"download-only",	noParam,       NULL, '2'},
		{"load-serverlist",	noParam,       NULL, '3'},
		{"debug-print",		noParam,       NULL, 'd'},
		{"version",		noParam,       NULL, 'v'},
		{"help",		noParam,       NULL, 'h'},
		{NULL,			0,             NULL,  0 }
	};
	int c, opt;
	while ((opt = getopt_long(argc, argv, "e:fc:CD:n123dvh?", long_options, &c)) >= 0) {
		switch (opt) {
			case 'e':
				/* >=0 and <=24800 */
				epoch = max(min(atoi(optarg), 24800), 0);
				break;
			case 'f':
				forceConvertData = true;
				break;
			case 'c':
				/* >=10 min. and <=600 min. */
				cronMode = max(min(atoi(optarg), 600), 10);
				break;
			case 'C':
				cronModeEcho = true;
				break;
			case 'D':
				/* >=1 and <=2 */
				diffMode = max(min(atoi(optarg), 2), 1);
				break;
			case 'n':
				createIndexes = false;
				break;
			case '1':
				configFile.setModifiedFlag(true);
				unlink(configFileName.c_str());
				saveSetup(configFileName);
				csql->connectMysql();
				csql->createTemplateDB(templateDBFile);
				return 0;
			case '2':
				downloadOnly = true;
				break;
			case '3':
				loadServerlist = true;
				break;
			case 'd':
				g_debugPrint = true;
				break;
			case 'v':
				printHeader();
				printCopyright();
				return 0;
			case 'h':
			case '?':
				printHelp();
				return (opt == '?') ? -1 : 0;
			default:
				break;
		}
	}

	if (diffMode > diffMode_none)
		checkDiffMode();

	if (cronMode > 0) {
		time_t lastDlTime = (diffMode > diffMode_none) ? g_settings.lastDiffDownloadTime : g_settings.lastDownloadTime;
		if ((time(0) - lastDlTime) < (cronMode*60)) {
			if (cronModeEcho) {
				printf("[%s] The last download is recent enough.\n", g_progName);

				char buf[256];
				memset(buf, 0, sizeof(buf));
				time_t tt = lastDlTime;
				struct tm* xTime = localtime(&tt);
				strftime(buf, sizeof(buf)-1, "%d.%m.%Y %H:%M", xTime);
				printf("[%s] Time of  last download is %s\n", g_progName, buf);

				memset(buf, 0, sizeof(buf));
				tt = lastDlTime + cronMode*60;
				xTime = localtime(&tt);
				strftime(buf, sizeof(buf)-1, "%d.%m.%Y %H:%M", xTime);
				printf("[%s] Next possible download is %s\n", g_progName, buf);
				fflush(stdout);
			}
			/* The last download is recent enough, exit. */
			return 0;
		}
	}

	if (loadServerlist || ((g_settings.serverListLastRefresh + g_settings.serverListRefreshDays*24*3600) < time(0))) {
		CServerlist* csl = new CServerlist(userAgentListCheck);
		csl->getServerList();
		delete csl;
		printf("[%s] update download server list...\n", g_progName);
		if (g_debugPrint) {
			if (g_settings.downloadServerCount > 0) {
				for (int i = 1; i <= g_settings.downloadServerCount; i++) {
					printf("[%s] download server found: %s\n", g_progName, (g_settings.downloadServer[i]).c_str());
				}
			}
		}
		if (g_settings.downloadServerCount < 1) {
			printf("[%s] no download server found ;-(\n", g_progName);
			return 1;
		}
		g_settings.serverListLastRefresh = time(0);
	}

	if (!getDownloadUrlList())
		return 1;
	if (downloadOnly || !convertData) {
		CLZMAdec* xzDec = new CLZMAdec();
		xzDec->decodeXZ(xzName, jsonDbName);
		delete xzDec;
		const char* msg = (downloadOnly) ? "download only" : "no changes";
		printf("[%s] %s, don't convert to sql database\n", g_progName, msg);
		fflush(stdout);
		return 0;
	}

	csql->connectMysql();
	csql->checkTemplateDB(templateDBFile);
	parseDB();

	return 0;
}

void CMV2Mysql::setDbFileNames(string xz)
{
	string path0   = getPathName(xz);
	path0          = getRealPath(path0);
	string file0   = getBaseName(xz);
	xzName         = path0 + "/" + file0;
	jsonDbName     = workDir + "/" + ((diffMode > diffMode_none) ? getFileName(defaultDiffXZ) : getFileName(defaultXZ));
}

void CMV2Mysql::verCallback(int type, string data, int parseMode, CRapidJsonSAX* /*instance*/)
{
	if ((parseMode == CRapidJsonSAX::parser_Work) && (type == CRapidJsonSAX::type_String)) {
		if (g_mainInstance->dbVersionInfoCount == 0) {
			g_mainInstance->dbVersionInfo = data;
		}
		g_mainInstance->dbVersionInfoCount++;
	}
}

long CMV2Mysql::getDbVersion(string file)
{
	char buf[512];
	memset(buf, 0, sizeof(buf));
	FILE* f = fopen(file.c_str(), "r");
	fread(buf, sizeof(buf)-1, 1, f);
	fclose(f);

	string str1 = (string)buf;
	string search = "\"Filmliste\":";
	size_t pos1 = str1.find(search);
	if (pos1 != string::npos) {
		pos1 = str1.find(search, pos1+1);
		if (pos1 != string::npos) {
			str1 = str1.substr(0, pos1);
			pos1 = str1.find_last_of(']');
			str1 = str1.substr(0, pos1+1) + "}";
			dbVersionInfoCount = 0;
			dbVersionInfo = "";

			/* parse versions info */
			CRapidJsonSAX* rjs = new CRapidJsonSAX();
			if (rjs != NULL) {
				rjs->parseString(str1, &verCallback);
				delete rjs;
			}

			if (dbVersionInfo.empty())
				return -1;

			/* 28.08.2017, 05:19 */
			return str2time("%d.%m.%Y, %H:%M", dbVersionInfo);
		}
	}
	return -1;
}

bool CMV2Mysql::checkNumberList(vector<uint32_t>* numberList, uint32_t number)
{
	if (numberList->empty())
		return false;
	for (vector<uint32_t>::iterator it = numberList->begin(); it != numberList->end(); ++it) {
		if ((uint32_t)(it[0]) == number)
			return true;
	}
	return false;
}

bool CMV2Mysql::getDownloadUrlList()
{
	uint32_t randStart  = 1;
	uint32_t randEnd    = g_settings.downloadServerCount;
	uint32_t whileCount = 0;
	uint32_t randValue;
	vector<uint32_t> numberList;

	while (true) {
		randValue = (rand() % ((randEnd + 1) - randStart)) + randStart;
		if (!checkNumberList(&numberList, randValue))
			numberList.push_back(randValue);
		if (numberList.size() >= randEnd)
			break;

		/* fallback for random error */
		whileCount++;
		if (whileCount >= randEnd*100)
			break;
	}

	for (uint32_t i = 0; i < numberList.size(); i++) {
		if (g_settings.downloadServerConnectFail[numberList[i]] >= g_settings.downloadServerConnectFailsMax)
			continue;
		string dlServer = g_settings.downloadServer[numberList[i]];
		string tmpPath  = getPathName(dlServer);
		dlServer        = tmpPath + "/" + ((diffMode > diffMode_none) ? g_settings.diffFileName : g_settings.aktFileName);
		if (g_debugPrint)
			printf("[%s-debug] check %s", g_progName, dlServer.c_str());
		if (downloadDB(dlServer)) {
			g_settings.downloadServerConnectFail[numberList[i]] = 0;
			g_settings.lastDownloadServer = numberList[i];
			return true;
		} else {
			g_settings.downloadServerConnectFail[numberList[i]] += 1;
			if (g_debugPrint)
				printf(" ERROR\n");
		}
	}
	printf("[%s] No download server found. ;-(\n", g_progName);
	return false;
}

long CMV2Mysql::getVersionFromXZ(string xz_, string json_)
{
	char* buf = new char[dlSegmentSize];
	FILE* f = fopen(xzName.c_str(), "r");
	fread(buf, dlSegmentSize, 1, f);
	fclose(f);
	f = fopen(xz_.c_str(), "w+");
	fwrite(buf, dlSegmentSize, 1, f);
	fclose(f);
	delete [] buf;

	CLZMAdec* xzDec = new CLZMAdec();
	xzDec->decodeXZ(xz_, json_, false);
	delete xzDec;
	return getDbVersion(json_);
}

bool CMV2Mysql::downloadDB(string url)
{
	if ((xzName.empty()) || (jsonDbName.empty())) {
		string xz = getPathName(workDir) + "/" + ((diffMode > diffMode_none) ? defaultDiffXZ : defaultXZ);
		setDbFileNames(xz);
	}

	bool versionOK    = true;
	bool toFile       = true;
	string tmpXzOld   = getPathName(workDir) + "/tmp-old.xz";
	string tmpXzNew   = getPathName(workDir) + "/tmp-new.xz";
	string tmpJsonOld = getPathName(workDir) + "/tmp-old.json";
	string tmpJsonNew = getPathName(workDir) + "/tmp-new.json";
	CCurl* curl       = new CCurl();
	int ret;

	if (file_exists(xzName.c_str())) {
		/* check version */
		long oldVersion = getVersionFromXZ(tmpXzOld, tmpJsonOld);
		string range_ = (string)"0-" + to_string(dlSegmentSize-1);
		const char* range = range_.c_str();
		ret = curl->CurlDownload(url, tmpXzNew, toFile, userAgentCheck, true, false, range, true);
		if (ret != 0) {
			delete curl;
			return false;
		}
		if (!g_debugPrint)
			printf("[%s] version check %s\n", g_progName, url.c_str());
		CLZMAdec* xzDec = new CLZMAdec();
		xzDec->decodeXZ(tmpXzNew, tmpJsonNew, false);
		long newVersion = getDbVersion(tmpJsonNew);
		delete xzDec;

		if ((oldVersion != -1) && (newVersion != -1)) {
			if (newVersion > oldVersion)
				versionOK = false;
		} else
			versionOK = false;
	} else
		versionOK = false;

	convertData = (forceConvertData) ? true : !versionOK;

	unlink(tmpXzOld.c_str());
	unlink(tmpXzNew.c_str());
	unlink(tmpJsonOld.c_str());
	unlink(tmpJsonNew.c_str());

	if (!versionOK) {
		const char* range = NULL;
		ret = curl->CurlDownload(url, xzName, toFile, userAgentDownload, true, false, range, true);
		if (ret != 0) {
			delete curl;
			return false;
		}
		if (g_debugPrint)
			printf("\n");
		printf("[%s] movie list has been changed\n", g_progName);
		printf("[%s] curl download %s\n", g_progName, url.c_str());
		if (diffMode > diffMode_none)
			g_settings.lastDiffDownloadTime = time(0);
		else
			g_settings.lastDownloadTime = time(0);
	} else {
		if (g_debugPrint)
			printf("\n");
		printf("[%s] movie list is up-to-date, don't download\n", g_progName);
	}

	/* get version */
	long newVersion = getVersionFromXZ(tmpXzNew, tmpJsonNew);
	unlink(tmpXzNew.c_str());
	unlink(tmpJsonNew.c_str());

	struct tm* versionTime = gmtime(&newVersion);
	char buf[256];
	memset(buf, 0, sizeof(buf));
	strftime(buf, sizeof(buf)-1, "%d.%m.%Y %H:%M", versionTime);
	printf("[%s] movie list version: %s\n", g_progName, buf);
	fflush(stdout);

	delete curl;
	return true;
}

double CMV2Mysql::startTimer()
{
	struct timeval t1;
	gettimeofday(&t1, NULL);
	return (double)t1.tv_sec*1000ULL + ((double)t1.tv_usec)/1000ULL;
}

string CMV2Mysql::getTimer_str(double startTime, string txt, int preci/*=3*/)
{
	struct timeval t1;
	gettimeofday(&t1, NULL);
	double workDTms = (double)t1.tv_sec*1000ULL + ((double)t1.tv_usec)/1000ULL;
	ostringstream tmp;
	tmp << txt << setprecision(preci) << ((workDTms - startTime) / 1000ULL) << " sec";
	return tmp.str();
}

double CMV2Mysql::getTimer_double(double startTime)
{
	struct timeval t1;
	gettimeofday(&t1, NULL);
	double workDTms = (double)t1.tv_sec*1000ULL + ((double)t1.tv_usec)/1000ULL;
	return ((workDTms - startTime) / 1000ULL);
}

bool CMV2Mysql::readEntry(int index)
{
	static bool replaceEntry = false;
	if (index == 0) {		/* "Filmliste" 0 */
		string tmp  = list0Entry.el[1].asString();
		g_mvDate    = str2time("%d.%m.%Y, %H:%M", tmp);
		g_mvVersion = list0Entry.el[3].asString();
	} else if (index == 1) {	/* "Filmliste" 1 */
		/* Not currently used */
	} else {			/* "X" (data)    */
		TVideoEntry videoEntry;
		videoEntry.replaceID = 0;
		videoEntry.update = 0;
		videoEntry.channel = movieEntry.el[0].asString();
		if ((videoEntry.channel != "") && (videoEntry.channel != cName)) {
			if (cName != "") {
				videoInfo.push_back(videoInfoEntry);
			}
			cName = videoEntry.channel;
			cCount = 0;
			videoInfoEntry.latest = INT_MIN;
			videoInfoEntry.oldest = INT_MAX;
		}

		videoEntry.theme		= movieEntry.el[1].asString();
		if (videoEntry.theme != "") {
			tName = videoEntry.theme;
		} else
			videoEntry.theme	= tName;

		videoEntry.title		= movieEntry.el[2].asString();
		videoEntry.duration		= duration2time(movieEntry.el[5].asString());
		videoEntry.size_mb		= movieEntry.el[6].asInt();
		videoEntry.description		= movieEntry.el[7].asString();
		videoEntry.url			= movieEntry.el[8].asString();
		videoEntry.website		= movieEntry.el[9].asString();
		videoEntry.subtitle		= movieEntry.el[10].asString();
		videoEntry.url_rtmp		= convertUrl(videoEntry.url, movieEntry.el[11].asString());
		videoEntry.url_small		= convertUrl(videoEntry.url, movieEntry.el[12].asString());
		videoEntry.url_rtmp_small	= convertUrl(videoEntry.url, movieEntry.el[13].asString());
		videoEntry.url_hd		= convertUrl(videoEntry.url, movieEntry.el[14].asString());
		videoEntry.url_rtmp_hd		= convertUrl(videoEntry.url, movieEntry.el[15].asString());

		if ((videoEntry.url.empty())	    &&
				(videoEntry.url_rtmp.empty())       &&
				(videoEntry.url_small.empty())      &&
				(videoEntry.url_rtmp_small.empty()) &&
				(videoEntry.url_hd.empty())	 &&
				(videoEntry.url_rtmp_hd.empty())) {
			skippedUrls++;
			return true;
		}

		videoEntry.date_unix		= movieEntry.el[16].asInt();
		if ((videoEntry.date_unix == 0) && (movieEntry.el[3].asString() != "") && (movieEntry.el[4].asString() != "")) {
			videoEntry.date_unix = str2time("%d.%m.%Y %H:%M:%S", movieEntry.el[3].asString() + " " + movieEntry.el[4].asString());
		}
		if ((videoEntry.date_unix > 0) && (epoch > 0)) {
			time_t maxDiff = (24*3600) * epoch; /* Not older than 'epoch' days (default all data) */
			if (videoEntry.date_unix < (nowTime - maxDiff))
				return true;
		}

		videoEntry.url_history		= movieEntry.el[17].asString();
		videoEntry.geo			= movieEntry.el[18].asString();
		videoEntry.new_entry		= movieEntry.el[19].asBool();
		videoEntry.channel		= cName;
		cCount++;
		videoInfoEntry.channel		= cName;
		videoInfoEntry.count		= cCount;
		videoInfoEntry.latest		= max(videoEntry.date_unix, videoInfoEntry.latest);
		if (videoEntry.date_unix != 0)
			videoInfoEntry.oldest	= min(videoEntry.date_unix, videoInfoEntry.oldest);

		movieEntries++;
		if (diffMode > diffMode_none)
			movieEntriesCounter++;
		else
			movieEntriesCounter = movieEntries;
		if (g_debugPrint) {
			if ((movieEntries % 32) == 0) {
				cout << msgHeadDebug() << "Processed entries: " << setfill(' ') << setw(6);
				cout << movieEntriesCounter << ", skip (no url) " << skippedUrls << "\r";
			}
			if ((movieEntries % 32*8) == 0)
				cout.flush();
		}
		
		uint32_t entryIdx = movieEntries;
		if (diffMode > diffMode_none) {
			uint32_t id_ = csql->checkEntryForUpdate(&videoEntry);
			if (id_ > 0) {
				entryIdx = id_;
				replaceEntry = true;
				videoEntry.update = nowTime;
			}
			else {
				// INSERT NEW
				videoEntriesNew.push_back(videoEntry);
				return true;
			}
		}

		string vQuery = csql->createVideoTableQuery(entryIdx, writeStart, replaceEntry, &videoEntry);
		writeStart = false;

		if ((writeLen + vQuery.length()) >= maxWriteLen) {
			videoEntrySqlBuf += ";\n";
			csql->executeSingleQueryString(videoEntrySqlBuf);
			vQuery = csql->createVideoTableQuery(entryIdx, true, replaceEntry, &videoEntry);
			videoEntrySqlBuf = "";
			writeLen = 0;
		}
		videoEntrySqlBuf += vQuery;
		writeLen = videoEntrySqlBuf.length();
	}
	return true;
}

void CMV2Mysql::parseCallback(int type, string data, int parseMode, CRapidJsonSAX* instance)
{
	g_mainInstance->parseCallbackInternal(type, data, parseMode, instance);
}

void CMV2Mysql::parseCallbackInternal(int type, string data, int parseMode, CRapidJsonSAX* /*instance*/)
{
	if (parseMode == CRapidJsonSAX::parser_Work) {
		if (type == CRapidJsonSAX::type_String) {
			if (count_parser > 1) {		// "X"
				movieEntry.el[keyCount_parser].entry = data;
			} else if (count_parser == 0) {	// "Filmliste" 0
				list0Entry.el[keyCount_parser].entry = data;
			} else if (count_parser == 1) {	// "Filmliste" 1
				list1Entry.el[keyCount_parser].entry = data;
			}
			keyCount_parser++;
		} else if (type == CRapidJsonSAX::type_StartArray) {
			keyCount_parser = 0;
		} else if (type == CRapidJsonSAX::type_EndArray) {
			readEntry(count_parser);
			keyCount_parser = 0;
			count_parser++;
		}
	}
}

bool CMV2Mysql::parseDB()
{
	cout << msgHead() << "parse json db & write temporary database...";
	cout.flush();

	videoEntriesNew.clear();

	/* extract movie list */
	CLZMAdec* xzDec = new CLZMAdec();
	xzDec->decodeXZ(xzName, jsonDbName);
	delete xzDec;

	double parseStartTime = startTimer();
	if (g_debugPrint) {
		printCursorOff();
		cout << endl;
	}

	/* startup operations sql db */
	csql->executeSingleQueryString("START TRANSACTION;");
	csql->executeSingleQueryString("SET autocommit = 0;");
	string usedDB = (diffMode > diffMode_none) ? VIDEO_DB : VIDEO_DB_TMP_1;
	if (diffMode == diffMode_none) {
		csql->createVideoDbFromTemplate(usedDB);
	}
	csql->setUsedDatabase(usedDB);

	if (multiQuery) {
		csql->setServerMultiStatementsOff();
	}

	if (diffMode > diffMode_none) {
		movieEntries = csql->getTableEntries(VIDEO_DB, g_settings.videoDb_TableVideo);
	}

	/* parse the movie list */
	CRapidJsonSAX* rjs = new CRapidJsonSAX();
	if (rjs != NULL) {
		rjs->setFileStreamBufSize(4194304);	// 4MB
		rjs->parseFile(jsonDbName, &parseCallback);
		delete rjs;
	}

	if (g_debugPrint) {
		cout << msgHeadDebug() << "Processed entries: " << setfill(' ') << setw(6);
		cout << movieEntriesCounter << ", skip (no url) " << skippedUrls << "\r";
	}

	/* final operations sql db */
	if (!videoEntrySqlBuf.empty()) {
		csql->executeSingleQueryString(videoEntrySqlBuf);
		videoEntrySqlBuf.clear();
	}

	if ((diffMode > diffMode_none) && (!videoEntriesNew.empty())) {
		insertEntries = insertNewEntries();
	}

	if (multiQuery) {
		csql->setServerMultiStatementsOn();
	}
	videoInfo.push_back(videoInfoEntry);
	string itq = csql->createInfoTableQuery(&videoInfo, csql->getTableEntries(VIDEO_DB, g_settings.videoDb_TableVideo), diffMode);
	csql->executeMultiQueryString(itq);
	csql->executeSingleQueryString("COMMIT;");
	csql->executeSingleQueryString("SET autocommit = 1;");

	if (g_debugPrint) {
		printCursorOn();
	}
	cout << endl;
	if (movieEntries < 1000) {
		cout << endl << msgHead() << "Video list too small (" << movieEntries;
		cout << " entries), no transfer to the database." << endl;
		cout.flush();
		return false;
	}

	if (diffMode == diffMode_none) {
		csql->renameDB();
	}
	if (createIndexes) {
		csql->createIndex(diffMode);
	}

	if (skippedUrls > 0) {
		cout << msgHead() << "skiped entrys (no url) " << skippedUrls << endl;
	}
	string days_s = (epoch > 0) ? to_string(epoch) + " days" : "all data";
	string parseEndTime = getTimer_str(parseStartTime, "");
	double entryTime = (getTimer_double(parseStartTime) / movieEntriesCounter) * 1000;
	cout << msgHead() << "all tasks done (" << movieEntriesCounter << " (";
	cout << days_s << ") / " << count_parser-2 << " entries)" << endl;

	if (diffMode > diffMode_none) {
		uint32_t aktEntries = csql->getTableEntries(VIDEO_DB, g_settings.videoDb_TableVideo);
		cout << msgHead() << "diffMode: " << movieEntriesCounter << " changed, (";
		cout << (movieEntriesCounter - insertEntries) << " updated, " << insertEntries;
		cout << " new, " << aktEntries << " all)" << endl;
	}

	cout << msgHead() << "duration: " << parseEndTime << " (";
	cout << setprecision(3) << entryTime << " msec/entry)" << endl;
	cout.flush();

	return true;
}

size_t CMV2Mysql::insertNewEntries()
{
	cout << endl << msgHead() << "insert new entries...";
	videoEntrySqlBuf.clear();
	writeLen = 0;
//	int entryIdx = csql->getTableEntries(VIDEO_DB, g_settings.videoDb_TableVideo);
	int entryIdx = csql->getLastIndex(VIDEO_DB, g_settings.videoDb_TableVideo);
	writeStart = true;
	size_t i = 0;
	for (i = 0; i < videoEntriesNew.size(); i++) {
		videoEntriesNew[i].new_entry = true;
		videoEntriesNew[i].update = nowTime;
		entryIdx++;
		string vQuery = csql->createVideoTableQuery(entryIdx, writeStart, false, &(videoEntriesNew[i]));
		writeStart = false;

		if ((writeLen + vQuery.length()) >= maxWriteLen) {
			videoEntrySqlBuf += ";\n";
			csql->executeSingleQueryString(videoEntrySqlBuf);
			vQuery = csql->createVideoTableQuery(entryIdx, true, false, &(videoEntriesNew[i]));
			videoEntrySqlBuf = "";
			writeLen = 0;
		}
		videoEntrySqlBuf += vQuery;
		writeLen = videoEntrySqlBuf.length();
	}
	videoEntriesNew.clear();
	if (!videoEntrySqlBuf.empty()) {
		csql->executeSingleQueryString(videoEntrySqlBuf);
		videoEntrySqlBuf.clear();
	}

	cout << "done.";
	return i;
}

string CMV2Mysql::convertUrl(string url1, string url2)
{
	/* format url_small / url_rtmp_small etc:
		55|xxx.yyy
		 |    |
		 |    ----------  replace string
		 ---------------  replace pos in videoEntry.url (url1) */

	string ret = "";
	size_t pos = url2.find_first_of("|");
	if (pos != string::npos) {
		int pos1 = atoi(url2.substr(0, pos).c_str());
		ret = url1.substr(0, pos1) + url2.substr(pos+1);
	} else
		ret = url2;

	return ret;
}

const char* mySEMID = PROGNAME "_SEMID";
sem_t* mySemHandle = NULL;

void myExit(int val)
{
	/* Remove semaphore */
	if (mySemHandle != NULL) {
		sem_close(mySemHandle);
	}
	sem_unlink(mySEMID);
	/* exit program */
	printCursorOn();
	exit(val);
}

void sighandler (int signum)
{
	switch (signum) {
		case SIGTERM:
		case SIGINT:
		case SIGABRT:
			signal(signum, SIG_IGN);
			cout << "\nExit with sighandler (" << signum << ")" << endl;
			myExit(0);
			break;
		case SIGUSR1:
		case SIGUSR2:
			signal(signum, SIG_IGN);
			break;
		default:
			signal(signum, SIG_IGN);
			break;
	}
}

int main(int argc, char *argv[])
{
	g_mainInstance = NULL;

	/* install sighandler */
	signal(SIGTERM, sighandler);
	signal(SIGINT,  sighandler);
	signal(SIGABRT, sighandler);
	signal(SIGUSR1, sighandler);
	signal(SIGUSR2, sighandler);

	/* Create semaphore to correctly identify
	 * the program to prevent multiple instances. */
	mySemHandle = sem_open(mySEMID, O_CREAT|O_EXCL);
	if ((mySemHandle == SEM_FAILED) && (errno == EEXIST)) {
		cout << "[" << PROGNAME << "] An instance of '" << PROGNAME << "' is already running, this exits.\n" << endl;
		return 1;
	}

	/* main prog */
	g_mainInstance = new CMV2Mysql();
	int ret = g_mainInstance->run(argc, argv);
	delete g_mainInstance;

	/* Remove semaphore */
	if (mySemHandle != NULL) {
		sem_close(mySemHandle);
	}
	sem_unlink(mySEMID);

	return ret;
}
