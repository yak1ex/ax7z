//fileInfoキャッシュクラス

#ifndef infcache_h
#define infcache_h

#include <windows.h>
#include "critsect.h"
#include "entryFuncs.h"

typedef struct ArcInfo
{
	HLOCAL hinfo;			// fileInfo[]
	char path[MAX_PATH];	// ファイルパス
} ArcInfo;

#define INFOCACHE_MAX 0x10
class InfoCache
{
public:
	InfoCache();
	~InfoCache();
	void Clear(void); //キャッシュクリア
	void Add(char *filepath, HLOCAL *ph); //キャッシュに追加。INFOCACHE_MAX を超えると古いのは消す。
	//キャッシュにあればアーカイブ情報をコピー。
	int Dupli(char *filepath, HLOCAL *ph, fileInfo *pinfo);
private:
	CriticalSection cs;
	ArcInfo arcinfo[INFOCACHE_MAX];
	int nowno;
	bool GetCache(char *filepath, HLOCAL *ph);
};

// ワイド文字版
typedef struct ArcInfoW
{
	HLOCAL hinfo;			// fileInfoW[]
	wchar_t path[MAX_PATH];	// ファイルパス
} ArcInfoW;

class InfoCacheW
{
public:
	InfoCacheW();
	~InfoCacheW();
	void Clear(void); //キャッシュクリア
	void Add(wchar_t *filepath, HLOCAL *ph); //キャッシュに追加。INFOCACHE_MAX を超えると古いのは消す。
	//キャッシュにあればアーカイブ情報をコピー。
	int Dupli(wchar_t *filepath, HLOCAL *ph, fileInfoW *pinfo);
private:
	CriticalSection cs;
	ArcInfoW arcinfo[INFOCACHE_MAX];
	int nowno;
	bool GetCache(wchar_t *filepath, HLOCAL *ph);
};

#endif
