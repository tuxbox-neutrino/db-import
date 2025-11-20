/*
 * simple curl functions
 *
 * (C) 2015-2017 M. Liebmann (micha-bbg)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <math.h>

#include "common/helpers.h"
#include "curl.h"

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
size_t CCurl::CurlWriteToString(void *ptr, size_t size, size_t nmemb, void *data)
{
	if (size * nmemb > 0) {
		string* pStr = (string*) data;
		pStr->append((char*) ptr, nmemb);
	}
	return size*nmemb;
}

int CCurl::CurlProgressFunc(void *p, curl_off_t dltotal, curl_off_t dlnow, curl_off_t /*ultotal*/, curl_off_t /*ulnow*/)
{
	if (dltotal == 0)
		return 0;

	struct progressData *_pgd = static_cast<struct progressData*>(p);
	if (_pgd->last_dlnow == dlnow)
		return 0;
	_pgd->last_dlnow = dlnow;

	double dlSpeed;
	long responseCode = 0;
#if defined(CURLINFO_SPEED_DOWNLOAD_T)
	curl_easy_getinfo(_pgd->curl, CURLINFO_SPEED_DOWNLOAD_T, &dlSpeed);
#else
	curl_easy_getinfo(_pgd->curl, CURLINFO_SPEED_DOWNLOAD, &dlSpeed);
#endif
	curl_easy_getinfo(_pgd->curl, CURLINFO_RESPONSE_CODE, &responseCode);

	uint32_t MUL = 0x7FFF;
	uint32_t dlFragment = (uint32_t)((dlnow * MUL) / dltotal);
	if (responseCode != 200) {
		dlFragment = 0;
		dlSpeed    = 0;
	}

	int showDots = 50;
	int dots = (dlFragment * showDots) / MUL;
	printf(" %d%% [", (dlFragment * 100) / MUL);
	int i = 0;
	for (; i < dots-1; i++)
		printf("=");
	if (i < showDots) {
		printf(">");
		i++;
	}
	for (; i < showDots; i++)
		printf(" ");
	printf("] speed: %.03f KB/sec     \r", dlSpeed/1024); fflush(stdout);
	return 0;
}

#if LIBCURL_VERSION_NUM < 0x072000
int CCurl::CurlProgressFunc_old(void *p, double dltotal, double dlnow, double ultotal, double ulnow)
{
	return CurlProgressFunc(p, (curl_off_t)dltotal, (curl_off_t)dlnow, (curl_off_t)ultotal, (curl_off_t)ulnow);
}
#endif

size_t CCurl::CurlGetContentLengthFunc(void *ptr, size_t size, size_t nmemb, void *stream)
{
	int r;
	long len = 0;
	char buf[1024];
	memset(buf, 0, sizeof(buf));
	r = sscanf((const char*)ptr, "Content-Range: bytes %s1023\n", buf);
	if (r) {
		char *pch;
		pch = strrchr(buf,'/');
		if (pch) {
			pch++;
			if (pch)
				len = atol(pch);
		}
		*((long*) stream) = len;
	}
	else {
		r = sscanf((const char*)ptr, "Content-Length: %ld\n", &len);
		if (r)
			*((long*) stream) = len;
	}

	return size * nmemb;
}

int CCurl::CurlDownload(string url,
			string& output,
			bool outputToFile/*=true*/,
			string userAgent/*=""*/,
			bool silent/*=false*/,
			bool verbose/*=false*/,
			const char* range/*=NULL*/,
			bool noDisplayHttpError/*=false*/,
			bool passHeader/*=false*/,
			string postfields/*=""*/,
			int connectTimeout/*=20*/,
			int ipv/*=10*/,
			bool useProxy/*=false*/,
			bool followRedir/*=true*/,
			int maxRedirs/*=20*/)
{
	(void)useProxy;
/*
	parameter	typ		default
	----------------------------------------
**	url		string		required
**	o, output	string		when empty then save to string
					as secund return value
**	A, userAgent	string		empty
**	P, postfields	string		empty
**	v, verbose	bool		false
**	s, silent	bool		false
**	h, header	bool		false
**	connectTimeout	number		20
**	ipv4		bool		false
**	ipv6		bool		false
**	useProxy	bool		true (default)
**	followRedir	bool		true
**	maxRedirs	number		20
*/

#define CURL_MSG_ERROR "[curl:download \33[1;31mERROR!\33[0m]"

	char errMsg[1024]={0};
	CURL *curl_handle = curl_easy_init();
	if (!curl_handle) {
		memset(errMsg, '\0', sizeof(errMsg));
		snprintf(errMsg, sizeof(errMsg)-1, "error creating cUrl handle.");
		printf("%s %s\n", CURL_MSG_ERROR, errMsg);
		return PRIV_CURL_ERR_HANDLE;
	}

	if (url.empty()) {
		curl_easy_cleanup(curl_handle);
		memset(errMsg, '\0', sizeof(errMsg));
		snprintf(errMsg, sizeof(errMsg)-1, "no url given.");
		printf("%s %s\n", CURL_MSG_ERROR, errMsg);
		return PRIV_CURL_ERR_NO_URL;
	}

	FILE *fp = NULL;
	if (outputToFile) {
		fp = fopen(output.c_str(), "wb");
		if (fp == NULL) {
			memset(errMsg, '\0', sizeof(errMsg));
			snprintf(errMsg, sizeof(errMsg)-1, "Can't create %s", output.c_str());
			printf("%s %s\n", CURL_MSG_ERROR, errMsg);
			curl_easy_cleanup(curl_handle);
			return PRIV_CURL_ERR_CREATE_FILE;
		}
	}

	bool ipv4 = (ipv == 4) ? true : false;
	bool ipv6 = (ipv == 6) ? true : false;
	if (!ipv4 && !ipv6) {
		ipv4 = true;
		ipv6 = true;
	}

	string retString = "";
	curl_easy_setopt(curl_handle, CURLOPT_URL, url.c_str());
	if (outputToFile) {
		curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, NULL);
		curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, fp);
	}
	else {
		curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, &CCurl::CurlWriteToString);
		curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void*)&retString);
	}
	curl_easy_setopt(curl_handle, CURLOPT_FAILONERROR, 1L);
	curl_easy_setopt(curl_handle, CURLOPT_TIMEOUT, (long)(4*connectTimeout));
	curl_easy_setopt(curl_handle, CURLOPT_CONNECTTIMEOUT, (long)connectTimeout);
	curl_easy_setopt(curl_handle, CURLOPT_NOSIGNAL, 1L);
	curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYPEER, 0L);
	/* enable all supported built-in compressions */
	curl_easy_setopt(curl_handle, CURLOPT_ACCEPT_ENCODING, "");
	curl_easy_setopt(curl_handle, CURLOPT_COOKIE, "");
	if (!userAgent.empty())
		curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, userAgent.c_str());

	if (!postfields.empty())
		curl_easy_setopt(curl_handle, CURLOPT_POSTFIELDS, postfields.c_str());

	if (ipv4 && ipv6)
		curl_easy_setopt(curl_handle, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_WHATEVER);
	else if (ipv4)
		curl_easy_setopt(curl_handle, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4);
	else if (ipv6)
		curl_easy_setopt(curl_handle, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V6);

#if 0
	if (!g_settings.softupdate_proxyserver.empty() && useProxy) {
		curl_easy_setopt(curl_handle, CURLOPT_PROXY, g_settings.softupdate_proxyserver.c_str());
		if (!g_settings.softupdate_proxyusername.empty()) {
			string tmp = g_settings.softupdate_proxyusername + ":" + g_settings.softupdate_proxypassword;
			curl_easy_setopt(curl_handle, CURLOPT_PROXYUSERPWD, tmp.c_str());
		}
	}
#endif

	curl_easy_setopt(curl_handle, CURLOPT_FOLLOWLOCATION, (followRedir)?1L:0L);
	curl_easy_setopt(curl_handle, CURLOPT_MAXREDIRS, (long)maxRedirs);
	curl_easy_setopt(curl_handle, CURLOPT_NOPROGRESS, (silent)?1L:0L);
	curl_easy_setopt(curl_handle, CURLOPT_VERBOSE, (verbose)?1L:0L);
	curl_easy_setopt(curl_handle, CURLOPT_HEADER, (passHeader)?1L:0L);
	if (range != NULL)
		curl_easy_setopt(curl_handle, CURLOPT_RANGE, (char*)range);

	long ContentLength = 0;
	curl_easy_setopt(curl_handle, CURLOPT_HEADERFUNCTION, CurlGetContentLengthFunc);
	curl_easy_setopt(curl_handle, CURLOPT_HEADERDATA, &ContentLength);

	progressData pgd;

	if (!silent) {
		pgd.curl = curl_handle;
		pgd.last_dlnow = -1;
#if LIBCURL_VERSION_NUM >= 0x072000
		curl_easy_setopt(curl_handle, CURLOPT_XFERINFOFUNCTION, CCurl::CurlProgressFunc);
		curl_easy_setopt(curl_handle, CURLOPT_XFERINFODATA, &pgd);
#else
		curl_easy_setopt(curl_handle, CURLOPT_PROGRESSFUNCTION, CCurl::CurlProgressFunc_old);
		curl_easy_setopt(curl_handle, CURLOPT_PROGRESSDATA, &pgd);
#endif
	}

	char cerror[CURL_ERROR_SIZE]={0};
	curl_easy_setopt(curl_handle, CURLOPT_ERRORBUFFER, cerror);

	if (!silent)
		printf("\n[curl:download] download %s => %s\n", url.c_str(), (outputToFile)?output.c_str():"return string");
	if (!silent) {
		printCursorOff();
		printf("\n");
	}
	CURLcode ret = curl_easy_perform(curl_handle);
	if (!silent) {
		printCursorOn();
		printf("\n");
	}

	string msg;
	if (!silent) {
		double dsize, dtime;
		char *dredirect=NULL, *deffektive=NULL;
#if defined(CURLINFO_SIZE_DOWNLOAD_T)
				curl_easy_getinfo(curl_handle, CURLINFO_SIZE_DOWNLOAD_T, &dsize);
#else
				curl_easy_getinfo(curl_handle, CURLINFO_SIZE_DOWNLOAD, &dsize);
#endif
		curl_easy_getinfo(curl_handle, CURLINFO_TOTAL_TIME, &dtime);
		CURLcode res1 = curl_easy_getinfo(curl_handle, CURLINFO_EFFECTIVE_URL, &deffektive);
		CURLcode res2 = curl_easy_getinfo(curl_handle, CURLINFO_REDIRECT_URL, &dredirect);

		char msg1[1024]={0};
		memset(msg1, '\0', sizeof(msg1));
		snprintf(msg1, sizeof(msg1)-1, "\n[curl:download] O.K. size: %.0f bytes, time: %.02f sec.", dsize, dtime);
		msg = msg1;
		if (outputToFile)
			msg += string("\n		     file: ") + output;
		else
			msg += string("\n		   output: return string");
		msg += string("\n		      url: ") + url;
		if ((res1 == CURLE_OK) && deffektive) {
			string tmp1 = string(deffektive);
			string tmp2 = url;
			if (trim(tmp1, " /") != trim(tmp2, " /"))
				msg += string("\n	    effektive url: ") + deffektive;
		}
		if ((res2 == CURLE_OK) && dredirect)
			msg += string("\n	      redirect to: ") + dredirect;
	}

	curl_easy_cleanup(curl_handle);
	if (outputToFile)
		fclose(fp);

	if (ret != 0) {
		if (! ((  (ret == CURLE_HTTP_RETURNED_ERROR)  ||
			  (ret == CURLE_COULDNT_RESOLVE_HOST) ||
			  (ret == CURLE_COULDNT_CONNECT)
		       ) && noDisplayHttpError) ) {
			memset(errMsg, '\0', sizeof(errMsg));
			snprintf(errMsg, sizeof(errMsg)-1, "%s", cerror);
			printf("%s curl error: %s - %d\n", CURL_MSG_ERROR, errMsg, ret);
		}
		if (outputToFile)
			unlink(output.c_str());
		return PRIV_CURL_ERR_CURL;
	}

	if (verbose == true)
		printf("%s\n \n", msg.c_str());

	if (!outputToFile)
		output = retString;

//printf("\nContentLength: %ld\n \n", ContentLength);

return PRIV_CURL_OK;
}

#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
