/*
ax7z entry funcs
*/

#define NOMINMAX
#include <windows.h>
#undef NOMINMAX
#include <commctrl.h>
#include <shlobj.h>
#include <vector>
#include <algorithm>
#include <numeric>
#include <map>
#include <set>
#include "entryFuncs.h"
#include "infcache.h"
#include <sstream>
#include "resource.h"
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>

struct NoCaseLess
{
	bool operator()(const std::string &s1, const std::string &s2) const
	{
		return _stricmp(s1.c_str(), s2.c_str()) < 0;
	}
};

//拡張子管理クラス
class ExtManager
{
private:
	typedef std::string Ext;
	struct Info
	{
		std::vector<std::string> methods;
		bool enable;
	};
	std::map<Ext, Info, NoCaseLess> m_mTable;
	std::set<Ext, NoCaseLess> m_sTableUser;
public:
	static const char* const SECTION_NAME;
	static const char* const DEFAULT_EXTENSIONS;
	ExtManager() {}
	typedef std::vector<std::pair<std::string, std::string> > Conf;
	void Init(const Conf& conf);
	bool IsEnable(LPSTR filename) const;
	bool IsEnable(LPWSTR filename) const;
	void SetEnable(const std::string &sExt, bool fEnable)
	{
		m_mTable[sExt].enable = fEnable;
	}
	typedef std::map<Ext, Info, NoCaseLess>::value_type EachValueType;
	template<typename Functor>
	void Each(Functor f) const
	{
		std::for_each(m_mTable.begin(), m_mTable.end(), f);
	}
	void SetDefault();
	void Save(const std::string &sIniFileName) const;
	void Load(const std::string &sIniFileName);
	void SetPluginInfo(std::vector<std::string> &info) const;
	void AddUserExt(const std::string& sExt);
	void RemoveUserExt(const std::string& sExt);
	void SetUserExtensions(const std::string& sExts);
	std::string GetUserExtensions() const;
};

const char* const ExtManager::SECTION_NAME = "ax7z";
const char* const ExtManager::DEFAULT_EXTENSIONS = "bin;img;mdf";

void ExtManager::Init(const Conf& conf)
{
	Conf::const_iterator ciEnd = conf.end();
	for(Conf::const_iterator ci = conf.begin(); ci != ciEnd; ++ci) {
		m_mTable[ci->first].enable = true;
		m_mTable[ci->first].methods.push_back(ci->second);
	}
}

bool ExtManager::IsEnable(LPSTR filename) const
{
	char buf[_MAX_EXT];
	_splitpath(filename, NULL, NULL, NULL, buf);
	if(buf[0] == 0) buf[1] = 0; // guard
	std::string sBuf(buf+1); // skip period
	std::map<Ext, Info, NoCaseLess>::const_iterator ci = m_mTable.find(sBuf);
	if(ci != m_mTable.end()) {
		return ci->second.enable;
	}
	std::set<Ext, NoCaseLess>::const_iterator ciUser = m_sTableUser.find(sBuf);
	if(ciUser != m_sTableUser.end()) {
		return true;
	}
	return false;
}

// TODO: Fix just a quick hack
bool ExtManager::IsEnable(LPWSTR filename) const
{
	WCHAR buf_[_MAX_EXT];
	_wsplitpath(filename, NULL, NULL, NULL, buf_);
	char buf[_MAX_EXT];
	if(wcstombs(buf, buf_, sizeof(buf)) == static_cast<std::size_t>(-1)) return false;
	if(buf[0] == 0) buf[1] = 0; // guard
	std::string sBuf(buf+1); // skip period
	std::map<Ext, Info, NoCaseLess>::const_iterator ci = m_mTable.find(sBuf);
	if(ci != m_mTable.end()) {
		return ci->second.enable;
	}
	std::set<Ext, NoCaseLess>::const_iterator ciUser = m_sTableUser.find(sBuf);
	if(ciUser != m_sTableUser.end()) {
		return true;
	}
	return false;
}

void ExtManager::SetDefault()
{
	std::map<Ext, Info, NoCaseLess>::iterator it, itEnd = m_mTable.end();
	for(it = m_mTable.begin(); it != itEnd; ++it) {
		it->second.enable = true;
	}
}

void ExtManager::Save(const std::string &sIniFileName) const
{
	if(m_sTableUser.size()) {
		const std::string &sResult = GetUserExtensions();
		WritePrivateProfileString(SECTION_NAME, "user_extensions", sResult.c_str(), sIniFileName.c_str());
	} else {
		WritePrivateProfileString(SECTION_NAME, "user_extensions", NULL, sIniFileName.c_str());
	}

	std::map<Ext, Info, NoCaseLess>::const_iterator ci, ciEnd = m_mTable.end();
	for(ci = m_mTable.begin(); ci != ciEnd; ++ci) {
		WritePrivateProfileString(SECTION_NAME, ci->first.c_str(), ci->second.enable ? "1" : "0", sIniFileName.c_str());
	}
}

void ExtManager::Load(const std::string &sIniFileName)
{
	std::vector<char> vBuf(1024);

	DWORD dwSize;
	do {
		vBuf.resize(vBuf.size() * 2);
		dwSize = GetPrivateProfileString(SECTION_NAME, "user_extensions", DEFAULT_EXTENSIONS, &vBuf[0], vBuf.size(), sIniFileName.c_str());
	} while(dwSize == vBuf.size() - 1);
	std::string sResult(&vBuf[0]);
	SetUserExtensions(sResult);

	std::map<Ext, Info, NoCaseLess>::iterator mi, miEnd = m_mTable.end();
	for(mi = m_mTable.begin(); mi != miEnd; ++mi) {
		mi->second.enable = GetPrivateProfileInt(SECTION_NAME, mi->first.c_str(), 1, sIniFileName.c_str()) != 0;
	}
}

struct MyConcat2
{
	std::string operator()(const std::string &s1, const std::string &s2) const
	{
		if(s1 == "")
			return "*." + s2;
		return s1 + ";" + "*." + s2;
	}
};

void ExtManager::SetPluginInfo(std::vector<std::string> &vsPluginInfo) const
{
	std::map<std::string, std::string> mResmap;
	std::map<Ext, Info, NoCaseLess>::const_iterator ci, ciEnd = m_mTable.end();	
	for(ci = m_mTable.begin(); ci != ciEnd; ++ci) {
		if(!ci->second.enable) continue;
		std::vector<std::string>::const_iterator ci2, ciEnd2 = ci->second.methods.end();
		for(ci2 = ci->second.methods.begin(); ci2 != ciEnd2; ++ci2) {
			if(!mResmap[*ci2].empty()) {
				mResmap[*ci2] += ";";
			}
			mResmap[*ci2] += "*.";
			mResmap[*ci2] += ci->first;
		}
	}

	vsPluginInfo.clear();
	vsPluginInfo.push_back("00AM");
    vsPluginInfo.push_back("7z extract library v0.7 for 7-zip 4.57+ y3b1 (C) Makito Miyano / enhanced by Yak!"); 

	std::map<std::string, std::string>::const_iterator ci2, ciEnd2 = mResmap.end();
	for(ci2 = mResmap.begin(); ci2 != ciEnd2; ++ci2) {
		vsPluginInfo.push_back(ci2->second);
		vsPluginInfo.push_back(ci2->first + " files");
	}
	if(m_sTableUser.size()) {
		vsPluginInfo.push_back(std::accumulate(m_sTableUser.begin(), m_sTableUser.end(), std::string(), MyConcat2()));
		vsPluginInfo.push_back("User-defined");
	}
}

void ExtManager::AddUserExt(const std::string& sExt)
{
	m_sTableUser.insert(sExt);
}

void ExtManager::RemoveUserExt(const std::string& sExt)
{
	m_sTableUser.erase(sExt);
}

struct MyConcat
{
	std::string operator()(const std::string &s1, const std::string &s2) const
	{
		if(s1 == "") return s2;
		return s1 + ";" + s2;
	}
};

std::string ExtManager::GetUserExtensions() const
{
	return std::accumulate(m_sTableUser.begin(), m_sTableUser.end(), std::string(), MyConcat());
}

void ExtManager::SetUserExtensions(const std::string& sExts)
{
	boost::algorithm::split(m_sTableUser, sExts, boost::algorithm::is_any_of(";"), boost::algorithm::token_compress_on);
	m_sTableUser.erase("");
}

//グローバル変数
static InfoCache infocache; //アーカイブ情報キャッシュクラス
static InfoCacheW infocacheW; //アーカイブ情報キャッシュクラス
static std::string g_sIniFileName; // ini ファイル名
static ExtManager g_extManager;
HINSTANCE g_hInstance;

#ifndef _UNICODE
bool g_IsNT = false;
static inline bool IsItWindowsNT()
{
    OSVERSIONINFO versionInfo;
    versionInfo.dwOSVersionInfoSize = sizeof(versionInfo);
    if (!::GetVersionEx(&versionInfo)) 
        return false;
    return (versionInfo.dwPlatformId == VER_PLATFORM_WIN32_NT);
}
#endif

void SetParamDefault()
{
	g_extManager.SetDefault();
	g_extManager.SetUserExtensions(ExtManager::DEFAULT_EXTENSIONS);
}

std::string GetIniFileName()
{
    return g_sIniFileName;
}

void LoadFromIni()
{
    SetParamDefault();

    std::string sIniFileName = GetIniFileName();

	g_extManager.Load(sIniFileName);
}

void SaveToIni()
{
    std::string sIniFileName = GetIniFileName();

	g_extManager.Save(sIniFileName);
}

void SetIniFileName(HANDLE hModule)
{
    std::vector<char> vModulePath(1024);
    size_t nLen = GetModuleFileName((HMODULE)hModule, &vModulePath[0], (DWORD)vModulePath.size());
    vModulePath.resize(nLen + 1);
    // 本来は2バイト文字対策が必要だが、プラグイン名に日本語はないと判断して手抜き
    while (!vModulePath.empty() && vModulePath.back() != '\\') {
        vModulePath.pop_back();
    }

    g_sIniFileName = &vModulePath[0];
    g_sIniFileName +=".ini";
}

/* エントリポイント */
BOOL APIENTRY SpiEntryPoint(HANDLE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    bool bInitPath = false;
    switch (ul_reason_for_call) {
        case DLL_PROCESS_ATTACH:
#ifndef _UNICODE
            g_IsNT = IsItWindowsNT();
#endif
            CoInitialize(NULL);
		{
			extern void GetFormats(ExtManager::Conf &res);
			ExtManager::Conf v;
			GetFormats(v);
			g_extManager.Init(v);
		}
            SetIniFileName(hModule);
            LoadFromIni();
            bInitPath = true;
			break;
		case DLL_THREAD_ATTACH:
            CoInitialize(NULL);
            SetIniFileName(hModule);
            LoadFromIni();
            break;
        case DLL_THREAD_DETACH:
            CoUninitialize();
            break;
        case DLL_PROCESS_DETACH:
            CoUninitialize();
            break;
    }

    return TRUE;
}

//---------------------------------------------------------------------------
/* エントリポイント */
BOOL APIENTRY DllMain(HANDLE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    int a = sizeof(fileInfoW);
    switch (ul_reason_for_call) {
        case DLL_PROCESS_DETACH:
            infocache.Clear();
            infocacheW.Clear();
            break;
    }
    g_hInstance = (HINSTANCE)hModule;
    return SpiEntryPoint(hModule, ul_reason_for_call, lpReserved);
}

/***************************************************************************
 * SPI関数
 ***************************************************************************/
//---------------------------------------------------------------------------
int __stdcall GetPluginInfo(int infono, LPSTR buf, int buflen)
{
    std::vector<std::string> vsPluginInfo;

	g_extManager.SetPluginInfo(vsPluginInfo);

	if (infono < 0 || infono >= (int)vsPluginInfo.size()) {
        return 0;
    }

    lstrcpyn(buf, vsPluginInfo[infono].c_str(), buflen);

    return lstrlen(buf);
}

static bool CheckFileExtension(const char* pFileName, const char* pExtension)
{
    int nExtensionLen = strlen(pExtension);
    int nFileNameLen = strlen(pFileName);

    // ピリオドを入れてファイル名本体が存在するか?
    if (nFileNameLen <= nExtensionLen + 1) {
        return false;
    }

    return (strnicmp(pFileName + nFileNameLen - nExtensionLen, pExtension, nExtensionLen) == 0);
}
static bool CheckFileExtensionW(const wchar_t* pFileName, const wchar_t* pExtension)
{
    int nExtensionLen = wcslen(pExtension);
    int nFileNameLen = wcslen(pFileName);

    // ピリオドを入れてファイル名本体が存在するか?
    if (nFileNameLen <= nExtensionLen + 1) {
        return false;
    }

    return (wcsnicmp(pFileName + nFileNameLen - nExtensionLen, pExtension, nExtensionLen) == 0);
}

//---------------------------------------------------------------------------
int __stdcall IsSupported(LPSTR filename, DWORD dw)
{
    // 現時点では名前のみで判断
	return g_extManager.IsEnable(filename);
}

int __stdcall IsSupportedW(LPWSTR filename, DWORD dw)
{
    // 現時点では名前のみで判断
	return g_extManager.IsEnable(filename);
}

//---------------------------------------------------------------------------
//アーカイブ情報をキャッシュする
int GetArchiveInfoCache(char *filename, long len, HLOCAL *phinfo, fileInfo *pinfo)
{
    int ret = infocache.Dupli(filename, phinfo, pinfo);
    if (ret != SPI_NO_FUNCTION) return ret;

    //キャッシュに無い
    HLOCAL hinfo;
    ret = GetArchiveInfoEx(filename, len, &hinfo);
    if (ret != SPI_ALL_RIGHT) return ret;

    //キャッシュ
    infocache.Add(filename, &hinfo);

    if (phinfo != NULL) {
        UINT size = LocalSize(hinfo);
        /* 出力用のメモリの割り当て */
        *phinfo = LocalAlloc(LMEM_FIXED, size);
        if (*phinfo == NULL) {
            return SPI_NO_MEMORY;
        }

        memcpy(*phinfo, (void*)hinfo, size);
    } else {
        fileInfo *ptmp = (fileInfo *)hinfo;
        if (pinfo->filename[0] != '\0') {
            for (;;) {
                if (ptmp->method[0] == '\0') return SPI_NO_FUNCTION;
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
                if (ptmp->method[0] == '\0') return SPI_NO_FUNCTION;
                if (ptmp->position == pinfo->position) break;
                ptmp++;
            }
        }
        *pinfo = *ptmp;
    }
    return SPI_ALL_RIGHT;
}
int GetArchiveInfoCacheW(wchar_t *filename, long len, HLOCAL *phinfo, fileInfoW *pinfo)
{
    int ret = infocacheW.Dupli(filename, phinfo, pinfo);
    if (ret != SPI_NO_FUNCTION) return ret;

    //キャッシュに無い
    HLOCAL hinfo;
    ret = GetArchiveInfoWEx(filename, len, &hinfo);
    if (ret != SPI_ALL_RIGHT) return ret;

    //キャッシュ
    infocacheW.Add(filename, &hinfo);

    if (phinfo != NULL) {
        UINT size = LocalSize(hinfo);
        /* 出力用のメモリの割り当て */
        *phinfo = LocalAlloc(LMEM_FIXED, size);
        if (*phinfo == NULL) {
            return SPI_NO_MEMORY;
        }

        memcpy(*phinfo, (void*)hinfo, size);
    } else {
        fileInfoW *ptmp = (fileInfoW *)hinfo;
        if (pinfo->filename[0] != L'\0') {
            for (;;) {
                if (ptmp->method[0] == '\0') return SPI_NO_FUNCTION;
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
                if (ptmp->method[0] == '\0') return SPI_NO_FUNCTION;
                if (ptmp->position == pinfo->position) break;
                ptmp++;
            }
        }
        *pinfo = *ptmp;
    }
    return SPI_ALL_RIGHT;
}
//---------------------------------------------------------------------------
int __stdcall GetArchiveInfo(LPSTR buf, long len, unsigned int flag, HLOCAL *lphInf)
{
    //メモリ入力には対応しない
    if ((flag & 7) != 0) return SPI_NO_FUNCTION;

    *lphInf = NULL;
    return GetArchiveInfoCache(buf, len, lphInf, NULL);
}
int __stdcall GetArchiveInfoW(LPWSTR buf, long len, unsigned int flag, HLOCAL *lphInf)
{
    //メモリ入力には対応しない
    if ((flag & 7) != 0) return SPI_NO_FUNCTION;

    *lphInf = NULL;
    return GetArchiveInfoCacheW(buf, len, lphInf, NULL);
}

//---------------------------------------------------------------------------
int __stdcall GetFileInfo
(LPSTR buf, long len, LPSTR filename, unsigned int flag, struct fileInfo *lpInfo)
{
    //メモリ入力には対応しない
    if ((flag & 7) != 0) return SPI_NO_FUNCTION;

    lstrcpy(lpInfo->filename, filename);

    return GetArchiveInfoCache(buf, len, NULL, lpInfo);
}
int __stdcall GetFileInfoW
(LPWSTR buf, long len, LPWSTR filename, unsigned int flag, struct fileInfoW *lpInfo)
{
    //メモリ入力には対応しない
    if ((flag & 7) != 0) return SPI_NO_FUNCTION;

    wcscpy(lpInfo->filename, filename);

    return GetArchiveInfoCacheW(buf, len, NULL, lpInfo);
}
//---------------------------------------------------------------------------
int __stdcall GetFile(LPSTR src, long len,
               LPSTR dest, unsigned int flag,
               SPI_PROGRESS lpPrgressCallback, long lData)
{
    //メモリ入力には対応しない
    if ((flag & 7) != 0) return SPI_NO_FUNCTION;

    fileInfo info;
    info.filename[0] = '\0';
    info.position = len;
    int ret = GetArchiveInfoCache(src, 0, NULL, &info);
    if (ret != SPI_ALL_RIGHT) {
        CoUninitialize();
        return ret;
    }

    int nRet;
    if ((flag & 0x700) == 0) {
        //ファイルへの出力の場合
        std::string s = dest;
        s += "\\";
        s += info.filename;
        nRet = GetFileEx(src, NULL, s.c_str(), &info, lpPrgressCallback, lData);
    } else {
        // メモリへの出力の場合
        nRet = GetFileEx(src, (HLOCAL *)dest, NULL, &info, lpPrgressCallback, lData);
    }
    return nRet;
}
int __stdcall GetFileW(LPWSTR src, long len,
               LPWSTR dest, unsigned int flag,
               SPI_PROGRESS lpPrgressCallback, long lData)
{
    //メモリ入力には対応しない
    if ((flag & 7) != 0) return SPI_NO_FUNCTION;

    fileInfoW info;
    info.filename[0] = L'\0';
    info.position = len;
    int ret = GetArchiveInfoCacheW(src, 0, NULL, &info);
    if (ret != SPI_ALL_RIGHT) {
        CoUninitialize();
        return ret;
    }

    int nRet;
    if ((flag & 0x700) == 0) {
        //ファイルへの出力の場合
        std::wstring s = dest;
        s += L"\\";
        s += info.filename;
        nRet = GetFileWEx(src, NULL, s.c_str(), &info, lpPrgressCallback, lData);
    } else {
        // メモリへの出力の場合
        nRet = GetFileWEx(src, (HLOCAL *)dest, NULL, &info, lpPrgressCallback, lData);
    }
    return nRet;
}
//---------------------------------------------------------------------------
LRESULT CALLBACK AboutDlgProc(HWND hDlgWnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
        case WM_INITDIALOG:
            return FALSE;
        case WM_COMMAND:
            switch (LOWORD(wp)) {
                case IDOK:
                    EndDialog(hDlgWnd, IDOK);
                    break;
                case IDCANCEL:
                    EndDialog(hDlgWnd, IDCANCEL);
                    break;
                default:
                    return FALSE;
            }
        default:
            return FALSE;
    }
    return TRUE;
}

//---------------------------------------------------------------------------
struct MyConcat3
{
	std::string operator()(const std::string &s1, const std::string &s2) const
	{
		if(s1 == "") return s2;
		return s1 + ", " + s2;
	}
};

struct ListUpdater
{
	ListUpdater(HWND hwnd, std::vector<std::string> &v) : hwnd(hwnd), v(v) {}
	HWND hwnd;
	std::vector<std::string> &v;
	void operator()(const ExtManager::EachValueType& value)
	{
		std::string sLine("*.");
		sLine += value.first;
		if(value.first.size() < 8)
			sLine += std::string(8 - value.first.size(), ' ');
		sLine += std::accumulate(value.second.methods.begin(), value.second.methods.end(), std::string(), MyConcat3());
		LRESULT lResult = SendMessage(hwnd, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(static_cast<const void*>(sLine.c_str())));
		v.push_back(value.first);
		SendMessage(hwnd, LB_SETSEL, value.second.enable, lResult);
	}
};

void UpdateDialogItem(HWND hDlgWnd)
{
	std::vector<std::string> *pvMap = static_cast<std::vector<std::string>*>(reinterpret_cast<void*>(GetWindowLongPtr(hDlgWnd, DWLP_USER)));

	pvMap->clear();
	SendDlgItemMessage(hDlgWnd, IDC_EXTENSION_LIST, LB_RESETCONTENT, 0L, 0L);

	g_extManager.Each(ListUpdater(GetDlgItem(hDlgWnd, IDC_EXTENSION_LIST), *pvMap));
	SendDlgItemMessage(hDlgWnd, IDC_EXTENSION_LIST, LB_SETCARETINDEX, 0, 0);

	SendDlgItemMessage(hDlgWnd, IDC_EXTENSION_EDIT, WM_SETTEXT, 0, reinterpret_cast<LPARAM>(static_cast<const void*>(g_extManager.GetUserExtensions().c_str())));
}

bool UpdateValue(HWND hDlgWnd)
{
	std::vector<std::string> *pvMap = static_cast<std::vector<std::string>*>(reinterpret_cast<void*>(GetWindowLongPtr(hDlgWnd, DWLP_USER)));

	int nNum = pvMap->size();
	for(int i = 0; i < nNum; ++i) {
		g_extManager.SetEnable((*pvMap)[i], SendDlgItemMessage(hDlgWnd, IDC_EXTENSION_LIST, LB_GETSEL, i, 0) > 0);
	}

	LRESULT lLen = SendDlgItemMessage(hDlgWnd, IDC_EXTENSION_EDIT, WM_GETTEXTLENGTH, 0, 0);
	std::vector<char> vBuf(lLen+1);
	SendDlgItemMessage(hDlgWnd, IDC_EXTENSION_EDIT, WM_GETTEXT, lLen+1, reinterpret_cast<LPARAM>(&vBuf[0]));
	g_extManager.SetUserExtensions(&vBuf[0]);

    return true;
}

LRESULT CALLBACK ConfigDlgProc(HWND hDlgWnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
        case WM_INITDIALOG:
			SetWindowLongPtr(hDlgWnd, DWLP_USER, lp);
            UpdateDialogItem(hDlgWnd);
            return TRUE;
        case WM_COMMAND:
            switch (LOWORD(wp)) {
                case IDOK:
                    if (UpdateValue(hDlgWnd)) {
                        SaveToIni();
                        EndDialog(hDlgWnd, IDOK);
                    }
                    break;
                case IDCANCEL:
                    EndDialog(hDlgWnd, IDCANCEL);
                    break;
                case IDC_DEFAULT_BUTTON:
                    SetParamDefault();
                    UpdateDialogItem(hDlgWnd);
                    break;
				case IDC_SELECT_ALL_BUTTON:
					SendDlgItemMessage(hDlgWnd, IDC_EXTENSION_LIST, LB_SELITEMRANGE, TRUE, MAKELPARAM(0, 0xFFFFU));
					break;
				case IDC_UNSELECT_ALL_BUTTON:
					SendDlgItemMessage(hDlgWnd, IDC_EXTENSION_LIST, LB_SELITEMRANGE, FALSE, MAKELPARAM(0, 0xFFFFU));
					break;
                default:
                    return FALSE;
            }
        default:
            return FALSE;
    }
    return TRUE;
}

int __stdcall ConfigurationDlg(HWND parent, int fnc)
{
    if (fnc == 0) {
        //about
        DialogBox((HINSTANCE)g_hInstance, MAKEINTRESOURCE(IDD_ABOUT_DIALOG), parent, (DLGPROC)AboutDlgProc);
    } else {
		std::vector<std::string> vMap;
        DialogBoxParam((HINSTANCE)g_hInstance, MAKEINTRESOURCE(IDD_CONFIG_DIALOG), parent, (DLGPROC)ConfigDlgProc, reinterpret_cast<LPARAM>(static_cast<void*>(&vMap)));
    }
    return 0;
}

int __stdcall ExtractSolidArchive(LPCWSTR filename, SPI_OnWriteCallback pCallback)
{
    return ExtractSolidArchiveEx(filename, pCallback);
}
