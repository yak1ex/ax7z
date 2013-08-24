#include "infcache.h"

InfoCache::InfoCache()
{
	nowno = 0;
	for (int i=0; i<INFOCACHE_MAX; i++) {
		arcinfo[i].hinfo = NULL;
	}
}

InfoCache::~InfoCache()
{
	for (int i=0; i<INFOCACHE_MAX; i++) {
		if (arcinfo[i].hinfo) LocalFree(arcinfo[i].hinfo);
	}
}

void InfoCache::Clear(void)
{
	cs.Enter();
	for (int i=0; i<INFOCACHE_MAX; i++) {
		if (arcinfo[i].hinfo) {
			LocalFree(arcinfo[i].hinfo);
			arcinfo[i].hinfo = NULL;
		}
	}
	nowno = 0;
	cs.Leave();
}

//キャッシュ追加
//filepath:ファイルパス
//ph:ハンドルへのポインタ
//INFOCACHE_MAX超えたら古いのは消す
void InfoCache::Add(char *filepath, HLOCAL *ph)
{
	cs.Enter();
	if (arcinfo[nowno].hinfo) LocalFree(arcinfo[nowno].hinfo);
	arcinfo[nowno].hinfo = *ph;
	lstrcpy(arcinfo[nowno].path, filepath);
	nowno = (nowno+1)%INFOCACHE_MAX;
	cs.Leave();
}

//キャッシュにあればハンドルを返す
bool InfoCache::GetCache(char *filepath, HLOCAL *ph)
{
	bool ret = false;
	int no = nowno-1;
	if (no < 0) no = INFOCACHE_MAX -1;
	for (int i=0; i<INFOCACHE_MAX; i++) {
		if (arcinfo[no].hinfo == NULL) break;
		if (lstrcmpi(arcinfo[no].path, filepath) == 0) {
			*ph = arcinfo[no].hinfo;
			ret = true;
			break;
		}
		no--;
		if (no < 0) no = INFOCACHE_MAX -1;
	}
	return ret;
}

//キャッシュにあればコピー
//ph:アーカイブ情報を受け取るハンドルへのポインタ
//pinfo:アーカイブのファイル情報を受け取るポインタ
//		あらかじめ pinfo に filename か position をセットしておく。
//		キャッシュがあれば filename(position) の一致する情報を返す。
//キャッシュになければ、SPI_NO_FUNCTION が返る。
//キャッシュにあれば SPI_ALL_RIGHT が返る。
//アーカイブ情報はキャッシュにあるが、filename(position) が一致しない場合は
//SPI_NOT_SUPPORT が返る。エラーの場合はエラーコードが返る。
int InfoCache::Dupli(char *filepath, HLOCAL *ph, fileInfo *pinfo)
{
	cs.Enter();
	HLOCAL hinfo;
	int ret = GetCache(filepath, &hinfo);

if (ret) {
	ret = SPI_ALL_RIGHT;
	if (ph != NULL) {
		UINT size = LocalSize(hinfo);
		/* 出力用のメモリの割り当て */
		*ph = LocalAlloc(LMEM_FIXED, size);
		if (*ph == NULL) {
			ret = SPI_NO_MEMORY;
		} else {
			memcpy(*ph, (void*)hinfo, size);
		}
	} else {
		fileInfo *ptmp = (fileInfo *)hinfo;
		if (pinfo->filename[0] != '\0') {
			for (;;) {
				if (ptmp->method[0] == '\0') {
					ret = SPI_NOT_SUPPORT;
					break;
				}
				// complete path relative to archive root
				char path[sizeof(ptmp->path)+sizeof(ptmp->filename)];
				strcpy(path, ptmp->path);
				size_t len = strlen(path);
				if(len && path[len-1] != '/' && path[len-1] != '\\') // need delimiter
					strcat(path, "\\");
				strcat(path, ptmp->filename);
				if (lstrcmpi(path, pinfo->filename) == 0) break;
				ptmp++;
			}
		} else {
			for (;;) {
				if (ptmp->method[0] == '\0') {
					ret = SPI_NOT_SUPPORT;
					break;
				}
				if (ptmp->position == pinfo->position) break;
				ptmp++;
			}
		}
		if (ret == SPI_ALL_RIGHT) *pinfo = *ptmp;
	}
} else {
	ret = SPI_NO_FUNCTION;
}

	cs.Leave();
	return ret;
}

//////////////////////////////////////////
// ワイド文字版
//////////////////////////////////////////
InfoCacheW::InfoCacheW()
{
	nowno = 0;
	for (int i=0; i<INFOCACHE_MAX; i++) {
		arcinfo[i].hinfo = NULL;
	}
}

InfoCacheW::~InfoCacheW()
{
	for (int i=0; i<INFOCACHE_MAX; i++) {
		if (arcinfo[i].hinfo) LocalFree(arcinfo[i].hinfo);
	}
}

void InfoCacheW::Clear(void)
{
	cs.Enter();
	for (int i=0; i<INFOCACHE_MAX; i++) {
		if (arcinfo[i].hinfo) {
			LocalFree(arcinfo[i].hinfo);
			arcinfo[i].hinfo = NULL;
		}
	}
	nowno = 0;
	cs.Leave();
}

//キャッシュ追加
//filepath:ファイルパス
//ph:ハンドルへのポインタ
//INFOCACHE_MAX超えたら古いのは消す
void InfoCacheW::Add(wchar_t *filepath, HLOCAL *ph)
{
	cs.Enter();
	if (arcinfo[nowno].hinfo) LocalFree(arcinfo[nowno].hinfo);
	arcinfo[nowno].hinfo = *ph;
	wcscpy(arcinfo[nowno].path, filepath);
	nowno = (nowno+1)%INFOCACHE_MAX;
	cs.Leave();
}

//キャッシュにあればハンドルを返す
bool InfoCacheW::GetCache(wchar_t *filepath, HLOCAL *ph)
{
	bool ret = false;
	int no = nowno-1;
	if (no < 0) no = INFOCACHE_MAX -1;
	for (int i=0; i<INFOCACHE_MAX; i++) {
		if (arcinfo[no].hinfo == NULL) break;
		if (wcsicmp(arcinfo[no].path, filepath) == 0) {
			*ph = arcinfo[no].hinfo;
			ret = true;
			break;
		}
		no--;
		if (no < 0) no = INFOCACHE_MAX -1;
	}
	return ret;
}

//キャッシュにあればコピー
//ph:アーカイブ情報を受け取るハンドルへのポインタ
//pinfo:アーカイブのファイル情報を受け取るポインタ
//		あらかじめ pinfo に filename か position をセットしておく。
//		キャッシュがあれば filename(position) の一致する情報を返す。
//キャッシュになければ、SPI_NO_FUNCTION が返る。
//キャッシュにあれば SPI_ALL_RIGHT が返る。
//アーカイブ情報はキャッシュにあるが、filename(position) が一致しない場合は
//SPI_NOT_SUPPORT が返る。エラーの場合はエラーコードが返る。
int InfoCacheW::Dupli(wchar_t *filepath, HLOCAL *ph, fileInfoW *pinfo)
{
	cs.Enter();
	HLOCAL hinfo;
	int ret = GetCache(filepath, &hinfo);

	if (ret) {
		ret = SPI_ALL_RIGHT;
		if (ph != NULL) {
			UINT size = LocalSize(hinfo);
			/* 出力用のメモリの割り当て */
			*ph = LocalAlloc(LMEM_FIXED, size);
			if (*ph == NULL) {
				ret = SPI_NO_MEMORY;
			} else {
				memcpy(*ph, (void*)hinfo, size);
			}
		} else {
			fileInfoW *ptmp = (fileInfoW *)hinfo;
			if (pinfo->filename[0] != L'\0') {
				for (;;) {
					if (ptmp->method[0] == '\0') {
						ret = SPI_NOT_SUPPORT;
						break;
					}
					// complete path relative to archive root
					wchar_t path[sizeof(ptmp->path)+sizeof(ptmp->filename)];
					wcscpy(path, ptmp->path);
					size_t len = wcslen(path);
					if(len && path[len-1] != L'/' && path[len-1] != L'\\') // need delimiter
						wcscat(path, L"\\");
					wcscat(path, ptmp->filename);
					if (wcsicmp(path, pinfo->filename) == 0) break;
					ptmp++;
				}
			} else {
				for (;;) {
					if (ptmp->method[0] == '\0') {
						ret = SPI_NOT_SUPPORT;
						break;
					}
					if (ptmp->position == pinfo->position) break;
					ptmp++;
				}
			}
			if (ret == SPI_ALL_RIGHT) *pinfo = *ptmp;
		}
	} else {
		ret = SPI_NO_FUNCTION;
	}

	cs.Leave();
	return ret;
}
