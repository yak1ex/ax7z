/*
ax7z entry funcs
*/

#include <windows.h>
#include <commctrl.h>
#include <shlobj.h>
#include <vector>
#include <algorithm>
#include "entryFuncs.h"
#include "infcache.h"
#include <sstream>
#include "resource.h"
#include "7z/7zip/Bundles/ax7z/SolidCache.h"

//グローバル変数
static InfoCache infocache; //アーカイブ情報キャッシュクラス
static InfoCacheW infocacheW; //アーカイブ情報キャッシュクラス
static std::string g_sIniFileName; // ini ファイル名
static int s_nEnable7z = 1;
static int s_nEnableRar = 1;
static int s_nEnableCbr = 1;
static int s_nEnableCab = 1;
static int s_nEnableArj = 1;
static int s_nEnableLzh = 1;
HINSTANCE g_hInstance;
int g_nSolidEnable7z = 1;
int g_nSolidEnableRar = 1;

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
    s_nEnable7z = 1;
    s_nEnableRar = 1;
    s_nEnableCbr = 1;
    s_nEnableCab = 1;
    s_nEnableArj = 1;
    s_nEnableLzh = 1;

    g_nSolidEnable7z = 1;
    g_nSolidEnableRar = 1;

	SolidCache& sc = SolidCache::GetInstance();

	sc.SetMaxLookAhead(-1);
	sc.SetMaxMemory(-1);
	sc.SetMaxDisk(-1);
//    char buf[2048];
//    GetTempPath(sizeof(buf), buf);
//	sc.SetCacheFolder(buf);
}

std::string GetIniFileName()
{
    return g_sIniFileName;
}

void LoadFromIni()
{
    SetParamDefault();

    std::string sIniFileName = GetIniFileName();

    s_nEnable7z = GetPrivateProfileInt("ax7z", "7z", s_nEnable7z, sIniFileName.c_str());
    s_nEnableRar = GetPrivateProfileInt("ax7z", "rar", s_nEnableRar, sIniFileName.c_str());
    s_nEnableCbr = GetPrivateProfileInt("ax7z", "cbr", s_nEnableCbr, sIniFileName.c_str());
    s_nEnableCab = GetPrivateProfileInt("ax7z", "cab", s_nEnableCab, sIniFileName.c_str());
    s_nEnableArj = GetPrivateProfileInt("ax7z", "arj", s_nEnableArj, sIniFileName.c_str());
    s_nEnableLzh = GetPrivateProfileInt("ax7z", "lzh", s_nEnableLzh, sIniFileName.c_str());

	SolidCache& sc = SolidCache::GetInstance();

    g_nSolidEnable7z = GetPrivateProfileInt("ax7z", "solid7z", g_nSolidEnable7z, sIniFileName.c_str());
    g_nSolidEnableRar = GetPrivateProfileInt("ax7z", "solidrar", g_nSolidEnableRar, sIniFileName.c_str());
	sc.SetMaxLookAhead(GetPrivateProfileInt("ax7z", "lookahead", sc.GetMaxLookAhead(), sIniFileName.c_str()));
    sc.SetMaxMemory(GetPrivateProfileInt("ax7z", "memory", sc.GetMaxMemory(), sIniFileName.c_str()));
    sc.SetMaxDisk(GetPrivateProfileInt("ax7z", "disk", sc.GetMaxDisk(), sIniFileName.c_str()));
    char buf[2048], buf2[2048];
    GetTempPath(sizeof(buf2), buf2);
    GetPrivateProfileString("ax7z", "folder", buf2, buf, sizeof(buf), sIniFileName.c_str());
    sc.SetCacheFolder(buf);
}

void SaveToIni()
{
    std::string sIniFileName = GetIniFileName();

    WritePrivateProfileString("ax7z", "7z", s_nEnable7z ? "1" : "0", sIniFileName.c_str());
    WritePrivateProfileString("ax7z", "rar", s_nEnableRar ? "1" : "0", sIniFileName.c_str());
    WritePrivateProfileString("ax7z", "cbr", s_nEnableCbr ? "1" : "0", sIniFileName.c_str());
    WritePrivateProfileString("ax7z", "cab", s_nEnableCab ? "1" : "0", sIniFileName.c_str());
    WritePrivateProfileString("ax7z", "arj", s_nEnableArj ? "1" : "0", sIniFileName.c_str());
    WritePrivateProfileString("ax7z", "lzh", s_nEnableLzh ? "1" : "0", sIniFileName.c_str());

    char buf[2048];
    WritePrivateProfileString("ax7z", "solid7z", g_nSolidEnable7z ? "1" : "0", sIniFileName.c_str());
    WritePrivateProfileString("ax7z", "solidrar", g_nSolidEnableRar ? "1" : "0", sIniFileName.c_str());

	SolidCache& sc = SolidCache::GetInstance();
	wsprintf(buf, "%d", sc.GetMaxLookAhead());
    WritePrivateProfileString("ax7z", "lookahead", buf, sIniFileName.c_str());
    wsprintf(buf, "%d", sc.GetMaxMemory());
    WritePrivateProfileString("ax7z", "memory", buf, sIniFileName.c_str());
    wsprintf(buf, "%d", sc.GetMaxDisk());
    WritePrivateProfileString("ax7z", "disk", buf, sIniFileName.c_str());
    WritePrivateProfileString("ax7z", "folder", sc.GetCacheFolder().c_str(), sIniFileName.c_str());
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
    INITCOMMONCONTROLSEX ice = { sizeof(INITCOMMONCONTROLSEX), ICC_PROGRESS_CLASS };
    switch (ul_reason_for_call) {
        case DLL_PROCESS_ATTACH:
#ifndef _UNICODE
            g_IsNT = IsItWindowsNT();
#endif
            CoInitialize(NULL);
            InitCommonControlsEx(&ice);
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
    vsPluginInfo.push_back("00AM");
    vsPluginInfo.push_back("7z extract library v0.7 for 7-zip 4.57 y1 (C) Makito Miyano / patched by Yak!"); 
    if (s_nEnable7z) {
        vsPluginInfo.push_back("*.7z");
        vsPluginInfo.push_back("7-zip files");
    }
    if (s_nEnableRar) {
        vsPluginInfo.push_back("*.rar");
        vsPluginInfo.push_back("Rar files");
    }
    if (s_nEnableCbr) {
        vsPluginInfo.push_back("*.cbr");
        vsPluginInfo.push_back("Cbr(rar) files");
    }
    if (s_nEnableCab) {
        vsPluginInfo.push_back("*.cab");
        vsPluginInfo.push_back("Cab files");
    }
    if (s_nEnableArj) {
        vsPluginInfo.push_back("*.arj");
        vsPluginInfo.push_back("Arj files");
    }
    if (s_nEnableLzh) {
        vsPluginInfo.push_back("*.lzh");
        vsPluginInfo.push_back("LZH files");
    }

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
    int nLen = strlen(filename);
    if (nLen < 4) {
        // 最低でも4文字は必要なはず (a.7zが最短)
        return 0;
    }

    if ((s_nEnable7z && CheckFileExtension(filename, "7z"))
        || (s_nEnableRar && CheckFileExtension(filename, "rar"))
        || (s_nEnableCbr && CheckFileExtension(filename, "cbr"))
        || (s_nEnableCab && CheckFileExtension(filename, "cab"))
        || (s_nEnableLzh && CheckFileExtension(filename, "lzh"))
        || (s_nEnableArj && CheckFileExtension(filename, "arj"))) {
        // サポートしていると判断
        return 1;
    }

    return 0;
}

int __stdcall IsSupportedW(LPWSTR filename, DWORD dw)
{
    // 現時点では名前のみで判断
    int nLen = wcslen(filename);
    if (nLen < 4) {
        // 最低でも4文字は必要なはず (a.7zが最短)
        return 0;
    }

    if ((s_nEnable7z && CheckFileExtensionW(filename, L"7z"))
        || (s_nEnableRar && CheckFileExtensionW(filename, L"rar"))
        || (s_nEnableCbr && CheckFileExtensionW(filename, L"cbr"))
        || (s_nEnableCab && CheckFileExtensionW(filename, L"cab"))
        || (s_nEnableLzh && CheckFileExtensionW(filename, L"lzh"))
        || (s_nEnableArj && CheckFileExtensionW(filename, L"arj"))) {
        // サポートしていると判断
        return 1;
    }

    return 0;
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
static void UpdateSolidDialogItem(HWND hDlgWnd)
{
    SendDlgItemMessage(hDlgWnd, IDC_SOLID_7Z_CHECK, BM_SETCHECK, (WPARAM)g_nSolidEnable7z, 0L);
    SendDlgItemMessage(hDlgWnd, IDC_SOLID_RAR_CHECK, BM_SETCHECK, (WPARAM)g_nSolidEnableRar, 0L);

	SolidCache& sc = SolidCache::GetInstance();
    char buf[2048];
    wsprintf(buf, "%d", sc.GetMaxLookAhead());
    SendDlgItemMessage(hDlgWnd, IDC_MAX_LOOKAHEAD_EDIT, WM_SETTEXT, 0, (LPARAM)buf);
    wsprintf(buf, "%d", sc.GetMaxMemory());
    SendDlgItemMessage(hDlgWnd, IDC_MAX_MEMORY_EDIT, WM_SETTEXT, 0, (LPARAM)buf);
    wsprintf(buf, "%d", sc.GetMaxDisk());
    SendDlgItemMessage(hDlgWnd, IDC_MAX_DISK_EDIT, WM_SETTEXT, 0, (LPARAM)buf);
    SendDlgItemMessage(hDlgWnd, IDC_CACHE_FOLDER_EDIT, WM_SETTEXT, 0, (LPARAM)sc.GetCacheFolder().c_str());
}

static bool IsChecked2(HWND hDlgWnd, int nID, int &nVar)
{
    int nOldVar = nVar;
    if (IsDlgButtonChecked(hDlgWnd, nID) == BST_CHECKED) {
        nVar = 1;
    } else {
        nVar = 0;
    }
    return (nVar != nOldVar);
}

static bool GetIntValue(HWND hDlgWnd, int nID, int (SolidCache::*mfGetter)() const, int (SolidCache::*mfSetter)(int))
{
    char buf[2048];
    SendDlgItemMessage(hDlgWnd, nID, WM_GETTEXT, sizeof(buf), (LPARAM)buf);
    int nTempVar = atoi(buf);
	if(nTempVar != (SolidCache::GetInstance().*mfGetter)()) {
		(SolidCache::GetInstance().*mfSetter)(nTempVar);
        return true;
    }
    return false;
}

static bool UpdateSolidValue(HWND hDlgWnd)
{
    bool fChanged = false;
    fChanged |= IsChecked2(hDlgWnd, IDC_SOLID_7Z_CHECK, g_nSolidEnable7z);
    fChanged |= IsChecked2(hDlgWnd, IDC_SOLID_RAR_CHECK, g_nSolidEnableRar);
	fChanged |= GetIntValue(hDlgWnd, IDC_MAX_LOOKAHEAD_EDIT, SolidCache::GetMaxLookAhead, SolidCache::SetMaxLookAhead);
	fChanged |= GetIntValue(hDlgWnd, IDC_MAX_MEMORY_EDIT, SolidCache::GetMaxMemory, SolidCache::SetMaxMemory);
	fChanged |= GetIntValue(hDlgWnd, IDC_MAX_DISK_EDIT, SolidCache::GetMaxDisk, SolidCache::SetMaxDisk);
    char buf[2048];
    SendDlgItemMessage(hDlgWnd, IDC_CACHE_FOLDER_EDIT, WM_GETTEXT, sizeof(buf), (LPARAM)buf);
	if(lstrcmp(buf, SolidCache::GetInstance().GetCacheFolder().c_str())) {
        fChanged = true;
		SolidCache::GetInstance().SetCacheFolder(buf);
    }

    return fChanged;
}

LRESULT CALLBACK SolidConfigDlgProc(HWND hDlgWnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
        case WM_INITDIALOG:
            UpdateSolidDialogItem(hDlgWnd);
            return FALSE;
        case WM_COMMAND:
            switch (LOWORD(wp)) {
                case IDOK:
                    if (UpdateSolidValue(hDlgWnd)) {
                        SaveToIni();
                    }
                    EndDialog(hDlgWnd, IDOK);
                    break;
                case IDCANCEL:
                    EndDialog(hDlgWnd, IDCANCEL);
                    break;
				case IDC_BROWSE_BUTTON:
				{
					char buf[MAX_PATH];
					BROWSEINFO bwi = { hDlgWnd, NULL, buf, "Specify cache folder", BIF_RETURNONLYFSDIRS };
					LPITEMIDLIST piid = SHBrowseForFolder(&bwi);
					if(piid) {
						SHGetPathFromIDList(piid, buf);
						SendDlgItemMessage(hDlgWnd, IDC_CACHE_FOLDER_EDIT, WM_SETTEXT, 0, (LPARAM)buf);
						CoTaskMemFree(piid);
					}
				}
                    break;
                default:
                    return FALSE;
            }
    }
    return FALSE;
}

//---------------------------------------------------------------------------
void UpdateDialogItem(HWND hDlgWnd)
{
    std::ostringstream ost;
    SendMessage(GetDlgItem(hDlgWnd, IDC_7Z_CHECK), BM_SETCHECK, (WPARAM)s_nEnable7z, 0L);
    SendMessage(GetDlgItem(hDlgWnd, IDC_RAR_CHECK), BM_SETCHECK, (WPARAM)s_nEnableRar, 0L);
    SendMessage(GetDlgItem(hDlgWnd, IDC_CBR_CHECK), BM_SETCHECK, (WPARAM)s_nEnableCbr, 0L);
    SendMessage(GetDlgItem(hDlgWnd, IDC_CAB_CHECK), BM_SETCHECK, (WPARAM)s_nEnableCab, 0L);
    SendMessage(GetDlgItem(hDlgWnd, IDC_ARJ_CHECK), BM_SETCHECK, (WPARAM)s_nEnableArj, 0L);
    SendMessage(GetDlgItem(hDlgWnd, IDC_LZH_CHECK), BM_SETCHECK, (WPARAM)s_nEnableLzh, 0L);
}

int IsChecked(HWND hDlgWnd, int nID)
{
    if (IsDlgButtonChecked(hDlgWnd, nID) == BST_CHECKED) {
        return 1;
    } else {
        return 0;
    }
}

bool UpdateValue(HWND hDlgWnd)
{
    s_nEnable7z = IsChecked(hDlgWnd, IDC_7Z_CHECK);
    s_nEnableRar = IsChecked(hDlgWnd, IDC_RAR_CHECK);
    s_nEnableCbr = IsChecked(hDlgWnd, IDC_CBR_CHECK);
    s_nEnableCab = IsChecked(hDlgWnd, IDC_CAB_CHECK);
    s_nEnableArj = IsChecked(hDlgWnd, IDC_ARJ_CHECK);
    s_nEnableLzh = IsChecked(hDlgWnd, IDC_LZH_CHECK);

    return true;
}

LRESULT CALLBACK ConfigDlgProc(HWND hDlgWnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
        case WM_INITDIALOG:
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
                case IDC_SOLID_BUTTON:
                    DialogBox(g_hInstance, MAKEINTRESOURCE(IDD_SOLID_CONFIG_DIALOG), hDlgWnd, (DLGPROC)SolidConfigDlgProc);
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
        DialogBox((HINSTANCE)g_hInstance, MAKEINTRESOURCE(IDD_CONFIG_DIALOG), parent, (DLGPROC)ConfigDlgProc);
    }
    return 0;
}

int __stdcall ExtractSolidArchive(LPCWSTR filename, SPI_OnWriteCallback pCallback)
{
    return ExtractSolidArchiveEx(filename, pCallback);
}
