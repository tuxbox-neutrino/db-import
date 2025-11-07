
#ifndef __TYPES_H__
#define __TYPES_H__

#include <string>

using namespace std;

enum : int {
	diffMode_none     = 0,
	diffMode_normal   = 1,
	diffMode_extended = 2
};

typedef struct VideoEntry
{
	int    replaceID;
	string channel;
	string theme;
	string title;
	int    duration;
	int    size_mb;
	string description;
	string url;
	string website;
	string subtitle;
	string url_rtmp;
	string url_small;
	string url_rtmp_small;
	string url_hd;
	string url_rtmp_hd;
	int    date_unix;
	string url_history;
	string geo;
	bool   new_entry;
	int    update;
} TVideoEntry;

typedef struct VideoInfoEntry
{
	int    id;
	string channel;
	int    count;
	int    latest;
	int    oldest;
} TVideoInfoEntry;

enum : int {
	maxDownloadServerCount = 32
};

struct GSettings
{
	/* test mode */
	string testLabel;
	bool   testMode;

	/* database */
	string videoDbBaseName;
	string videoDb;
	string videoDbTmp1;
	string videoDbTemplate;
	string videoDb_TableVideo;
	string videoDb_TableInfo;
	string videoDb_TableVersion;
	string mysqlHost;

	/* download server */
	string downloadServer[maxDownloadServerCount];
	int    downloadServerConnectFail[maxDownloadServerCount];
	int    downloadServerCount;
	int    lastDownloadServer;
	time_t lastDownloadTime;
	time_t lastDiffDownloadTime;
	int    downloadServerConnectFailsMax;
	string aktFileName;
	string diffFileName;

	/* password file */
	string passwordFile;

	/* server list */
	string serverListUrl;
	time_t serverListLastRefresh;
	int    serverListRefreshDays;
};

#endif // __TYPES_H__
