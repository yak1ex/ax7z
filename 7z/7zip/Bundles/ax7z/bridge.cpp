/*
7-zip decode engine - susie bridge
*/

#include <windows.h>
#include <commctrl.h>
#include <initguid.h>
#include <time.h>

#include "entryFuncs.h"

#include "Common/StringConvert.h"
#include "Common/Wildcard.h"
#include "Windows/FileDir.h"
#include "Windows/PropVariant.h"
#include "Windows/PropVariantConversions.h"
#include "../../ICoder.h"
#include "../../UI/Common/DefaultName.h"
#include "../../UI/Common/OpenArchive.h"

#ifndef EXCLUDE_COM
#include "Windows/DLL.h"
#endif

#include "ExtractCallback.h"
#include "SolidArchiveExtractCallback.h"
#include "OpenCallback.h"
#include "SolidCache.h"
#include "resource.h"

extern HINSTANCE g_hInstance;
extern int g_nSolidEnable7z;
extern int g_nSolidEnableRar;

using namespace NWindows;
using namespace NFile;

static bool MyOpenArchive(CCodecs *cc, const UString &archiveName, 
                         const NFind::CFileInfoW &archiveFileInfo,
                         IInArchive **archiveHandler,
                         UString &defaultItemName,
                         bool &passwordEnabled, 
                         UString &password)
{
    COpenCallbackImp2 *openCallbackSpec = new COpenCallbackImp2;
    CMyComPtr<IArchiveOpenCallback> openCallback = openCallbackSpec;
    if (passwordEnabled)
    {
        openCallbackSpec->PasswordIsDefined = passwordEnabled;
        openCallbackSpec->Password = password;
    }

    UString fullName;
    int fileNamePartStartIndex;
    NFile::NDirectory::MyGetFullPathName(archiveName, fullName, fileNamePartStartIndex);
    openCallbackSpec->LoadFileInfo(
        fullName.Left(fileNamePartStartIndex), 
        fullName.Mid(fileNamePartStartIndex));

    int dummy;
    HRESULT result = OpenArchive(cc, archiveName, 
        archiveHandler, 
        dummy,
        defaultItemName,
        openCallback);
    if (result == S_FALSE) {
        return false;
    }
    if (result != S_OK) {
        return false;
    }
//    defaultItemName = GetDefaultName(archiveName, 
//        archiverInfo.Extensions[subExtIndex].Extension, 
//        archiverInfo.Extensions[subExtIndex].AddExtension);
    passwordEnabled = openCallbackSpec->PasswordIsDefined;
    password = openCallbackSpec->Password;

    return true;
}

static bool GetUINT64Value(IInArchive *archive, UINT32 index, 
                           PROPID propID, UINT64 &value)
{
    NCOM::CPropVariant propVariant;
    if (archive->GetProperty(index, propID, &propVariant) != S_OK) {
        return false;
    }
    if (propVariant.vt == VT_EMPTY)
        return false;
    value = ConvertPropVariantToUInt64(propVariant);
    return true;
}

enum SOLID_TYPE
{
    SOLID_NONE,
    SOLID_RAR,
    SOLID_7Z
};

static SOLID_TYPE IsSolid(IInArchive* archive, const NFind::CFileInfoW& archiverInfo, UINT32 numItems)
{
    // rar の場合は kpidSolidが設定されているようなのでそちらを使用
    // 7z の場合でも設定されている場合あり
    NCOM::CPropVariant propVariant;
    if (S_OK == archive->GetArchiveProperty(kpidSolid, &propVariant)) {
        if (propVariant.vt == VT_BOOL) {
            if(propVariant.bVal) {
                UString extension;
                {
                    int dotPos = archiverInfo.Name.ReverseFind(L'.');
                    if (dotPos >= 0)
                        extension = archiverInfo.Name.Mid(dotPos + 1);
                }
                if (extension.CompareNoCase(L"7z") == 0) {
                    return SOLID_7Z;
                } else {
                    return SOLID_RAR;
                }
            } else {
                return SOLID_NONE;
            }
        }
    }

    // 7z の場合
    UString extension;
    {
        int dotPos = archiverInfo.Name.ReverseFind(L'.');
        if (dotPos >= 0)
            extension = archiverInfo.Name.Mid(dotPos + 1);
    }
    if (extension.CompareNoCase(L"7z") == 0) {
        NWindows::NCOM::CPropVariant aPropVariant;
        BOOL bFlag = 0;
        for (size_t i = 0; i < numItems; ++i) {
            archive->GetProperty(i, kpidBlock, &aPropVariant);
            if (aPropVariant.vt == 0) {
                continue;
            }
            if (aPropVariant.ulVal > 0) {
                break;
            }
            if (bFlag) {
                return SOLID_7Z;
            }
            bFlag = TRUE;
        }
    }
    return SOLID_NONE;
}

int GetArchiveInfoEx(LPSTR filename, long len, HLOCAL *lphInf)
{
    UString archiveName = MultiByteToUnicodeString(filename);

    NFind::CFileInfoW archiveFileInfo;
    if (!NFind::FindFile(archiveName, archiveFileInfo) || archiveFileInfo.IsDirectory()) {
        return SPI_FILE_READ_ERROR;
    }

    if (archiveFileInfo.IsDirectory()) {
        return SPI_FILE_READ_ERROR;
    }

  CCodecs *codecs = new CCodecs;
  CMyComPtr<
    #ifdef EXTERNAL_CODECS
    ICompressCodecsInfo
    #else
    IUnknown
    #endif
    > compressCodecsInfo = codecs;
  HRESULT result0 = codecs->Load();

    bool passwordEnabled = false;
    UString password;
    UString defaultItemName;

    // 書庫を開く
    CMyComPtr<IInArchive> archiveHandler;
    if (!MyOpenArchive(codecs, archiveName, archiveFileInfo, 
        &archiveHandler, 
        defaultItemName, passwordEnabled, password)) {
        return SPI_FILE_READ_ERROR;
    }

    UINT32 numItems;
    if (S_OK != archiveHandler->GetNumberOfItems(&numItems)) {
        return SPI_FILE_READ_ERROR;
    }

    // solid?
    SOLID_TYPE stSolid = IsSolid(archiveHandler, archiveFileInfo, numItems);

    std::vector<fileInfo> vFileInfos;
    for (UINT32 i = 0; i < numItems; i++) {
        NCOM::CPropVariant propVariant;
        if (S_OK != archiveHandler->GetProperty(i, kpidPath, &propVariant)) {
            continue;
        }
        UString filePath;
        if (propVariant.vt == VT_EMPTY) {
            continue;
        } else if(propVariant.vt != VT_BSTR) {
            continue;
        } else {
            filePath = propVariant.bstrVal;
        }

        UINT64 unpackSize = 0;
        if (!GetUINT64Value(archiveHandler, i, kpidSize, unpackSize)) {
            continue;
        }
        if (unpackSize == 0) {
            continue;
        }

        UINT64 packSize = 0;
        if (!GetUINT64Value(archiveHandler, i, kpidPackedSize, packSize)) {
//            continue;
        }

        NCOM::CPropVariant property;
        if (S_OK != archiveHandler->GetProperty(i, kpidLastWriteTime, &property)) {
            continue;
        }
        if (property.vt != VT_FILETIME) {
            continue;
        }
        FILETIME fileTime = property.filetime;
        // first convert file time (UTC time) to local time
        FILETIME localTime;
        if (!FileTimeToLocalFileTime(&fileTime, &localTime)) {
            continue;
        }
        // then convert that time to system time
        SYSTEMTIME sysTime;
        if (!FileTimeToSystemTime(&localTime, &sysTime)) {
            continue;
        }
        struct tm atm;
        atm.tm_sec = sysTime.wSecond;
        atm.tm_min = sysTime.wMinute;
        atm.tm_hour = sysTime.wHour;
        atm.tm_mday = sysTime.wDay;
        atm.tm_mon = sysTime.wMonth - 1;        // tm_mon is 0 based
        atm.tm_year = sysTime.wYear - 1900;     // tm_year is 1900 based
        atm.tm_isdst = -1;
        time_t time = mktime(&atm);
    
        vFileInfos.push_back(fileInfo());
        fileInfo* pinfo = &vFileInfos.back();
        switch(stSolid) {
        case SOLID_NONE:
            lstrcpy((char*)pinfo->method, "7zip");
            break;
        case SOLID_RAR:
            lstrcpy((char*)pinfo->method, "7zip_s");
            pinfo->method[7] = 'R';
            break;
        case SOLID_7Z:
            lstrcpy((char*)pinfo->method, "7zip_s");
            pinfo->method[7] = '7';
            break;
        }
        pinfo->position = i;
        pinfo->compsize = static_cast<unsigned long>(packSize);
        pinfo->filesize = static_cast<unsigned long>(unpackSize);
        pinfo->timestamp = time;
        // path
        UStringVector vPaths;
        SplitPathToParts(filePath, vPaths);
        if (vPaths.Size() == 0) {
            continue;
        }
        bool bError = false;
        int iCurPos = 0;
        for (int j = 0; j < (vPaths.Size() - 1); ++j) {
            AString s = UnicodeStringToMultiByte(vPaths[j]);
            int iNextPos = iCurPos + s.Length() + 1;
            if (iNextPos >= 200) {
                bError = true;
                break;
            }
            strcpy(&pinfo->path[iCurPos], (LPCSTR)s);
            pinfo->path[iNextPos - 1] = '\\';
            iCurPos = iNextPos;
        }
        if (bError) {
            continue;
        }
        pinfo->path[iCurPos] = '\0';
        // filename        
        AString s = UnicodeStringToMultiByte(vPaths.Back());
        if (s.Length() >= 200) {
            s = s.Left(199);
        }
        strcpy(pinfo->filename, (LPCSTR)s);

        pinfo->crc = 0;
        if (S_OK == archiveHandler->GetProperty(i, kpidCRC, &property)) {
            if (property.vt == VT_UI4) {
                pinfo->crc = property.ulVal;
            }
        }
    }

    /* 出力用のメモリの割り当て(ファイル数+1だけの領域が必要)、0で初期化しておく */
    *lphInf = LocalAlloc(LPTR, sizeof(fileInfo) * (vFileInfos.size() + 1));
    if (*lphInf == NULL) {
        return SPI_NO_MEMORY;
    }

    // 結果の作成
    memcpy(*lphInf, &vFileInfos[0], sizeof(fileInfo) * vFileInfos.size());
    fileInfo* pFirst = (fileInfo*)*lphInf;
    fileInfo* pLast = pFirst + vFileInfos.size();
    pLast->method[0] = '\0';

    return SPI_ALL_RIGHT;
}

static int GetArchiveInfoWEx_impl(LPCWSTR filename, std::vector<fileInfoW>& vFileInfos)
{
    UString archiveName = filename;

    NFind::CFileInfoW archiveFileInfo;
    if (!NFind::FindFile(archiveName, archiveFileInfo) || archiveFileInfo.IsDirectory()) {
        return SPI_FILE_READ_ERROR;
    }

    if (archiveFileInfo.IsDirectory()) {
        return SPI_FILE_READ_ERROR;
    }

    CCodecs *codecs = new CCodecs;
    CMyComPtr<
        #ifdef EXTERNAL_CODECS
        ICompressCodecsInfo
        #else
        IUnknown
        #endif
    > compressCodecsInfo = codecs;
    HRESULT result = codecs->Load();

    bool passwordEnabled = false;
    UString password;
    UString defaultItemName;

    // 書庫を開く
    CMyComPtr<IInArchive> archiveHandler;
    if (!MyOpenArchive(codecs, archiveName, archiveFileInfo, 
        &archiveHandler, 
        defaultItemName, passwordEnabled, password)) {
        return SPI_FILE_READ_ERROR;
    }

    UINT32 numItems;
    if (S_OK != archiveHandler->GetNumberOfItems(&numItems)) {
        return SPI_FILE_READ_ERROR;
    }

    // solid?
    SOLID_TYPE stSolid = IsSolid(archiveHandler, archiveFileInfo, numItems);

    for (UINT32 i = 0; i < numItems; i++) {
        NCOM::CPropVariant propVariant;
        if (S_OK != archiveHandler->GetProperty(i, kpidPath, &propVariant)) {
            continue;
        }
        UString filePath;
        if (propVariant.vt == VT_EMPTY) {
            continue;
        } else if(propVariant.vt != VT_BSTR) {
            continue;
        } else {
            filePath = propVariant.bstrVal;
        }

        UINT64 unpackSize = 0;
        if (!GetUINT64Value(archiveHandler, i, kpidSize, unpackSize)) {
            continue;
        }
        if (unpackSize == 0) {
            continue;
        }

        UINT64 packSize = 0;
        if (!GetUINT64Value(archiveHandler, i, kpidPackedSize, packSize)) {
//            continue;
        }

        NCOM::CPropVariant property;
        if (S_OK != archiveHandler->GetProperty(i, kpidLastWriteTime, &property)) {
            continue;
        }
        if (property.vt != VT_FILETIME) {
            continue;
        }
        FILETIME fileTime = property.filetime;
        // first convert file time (UTC time) to local time
        FILETIME localTime;
        if (!FileTimeToLocalFileTime(&fileTime, &localTime)) {
            continue;
        }
        // then convert that time to system time
        SYSTEMTIME sysTime;
        if (!FileTimeToSystemTime(&localTime, &sysTime)) {
            continue;
        }
        struct tm atm;
        atm.tm_sec = sysTime.wSecond;
        atm.tm_min = sysTime.wMinute;
        atm.tm_hour = sysTime.wHour;
        atm.tm_mday = sysTime.wDay;
        atm.tm_mon = sysTime.wMonth - 1;        // tm_mon is 0 based
        atm.tm_year = sysTime.wYear - 1900;     // tm_year is 1900 based
        atm.tm_isdst = -1;
        time_t time = mktime(&atm);
    
        vFileInfos.push_back(fileInfoW());
        fileInfoW* pinfo = &vFileInfos.back();
        switch(stSolid) {
        case SOLID_NONE:
            lstrcpy((char*)pinfo->method, "7zip");
            break;
        case SOLID_RAR:
            lstrcpy((char*)pinfo->method, "7zip_s");
            pinfo->method[7] = 'R';
            break;
        case SOLID_7Z:
            lstrcpy((char*)pinfo->method, "7zip_s");
            pinfo->method[7] = '7';
            break;
        }
        pinfo->position = i;
        pinfo->compsize = static_cast<unsigned long>(packSize);
        pinfo->filesize = static_cast<unsigned long>(unpackSize);
        pinfo->timestamp = time;
        // path
        UStringVector vPaths;
        SplitPathToParts(filePath, vPaths);
        if (vPaths.Size() == 0) {
            continue;
        }
        bool bError = false;
        int iCurPos = 0;
        for (int j = 0; j < (vPaths.Size() - 1); ++j) {
            UString s = vPaths[j];
            int iNextPos = iCurPos + s.Length() + 1;
            if (iNextPos >= 200) {
                bError = true;
                break;
            }
            wcscpy(&pinfo->path[iCurPos], (LPCWSTR)s);
            pinfo->path[iNextPos - 1] = L'\\';
            iCurPos = iNextPos;
        }
        if (bError) {
            continue;
        }
        pinfo->path[iCurPos] = L'\0';
        // filename        
        UString s = vPaths.Back();
        if (s.Length() >= 200) {
            s = s.Left(199);
        }
        wcscpy(pinfo->filename, (LPCWSTR)s);
        pinfo->crc = 0;
        if (S_OK == archiveHandler->GetProperty(i, kpidCRC, &property)) {
            if (property.vt == VT_UI4) {
                pinfo->crc = property.ulVal;
            }
        }
    }
    return SPI_ALL_RIGHT;
}

int GetArchiveInfoWEx(LPWSTR filename, long len, HLOCAL *lphInf)
{
    std::vector<fileInfoW> vFileInfos;
    int nRet = GetArchiveInfoWEx_impl(filename, vFileInfos);
    if (nRet != SPI_ALL_RIGHT) {
        return nRet;
    }

    /* 出力用のメモリの割り当て(ファイル数+1だけの領域が必要)、0で初期化しておく */
    *lphInf = LocalAlloc(LPTR, sizeof(fileInfoW) * (vFileInfos.size() + 1));
    if (*lphInf == NULL) {
        return SPI_NO_MEMORY;
    }

    // 結果の作成
    memcpy(*lphInf, &vFileInfos[0], sizeof(fileInfoW) * vFileInfos.size());
    fileInfoW* pFirst = (fileInfoW*)*lphInf;
    fileInfoW* pLast = pFirst + vFileInfos.size();
    pLast->method[0] = '\0';

    return SPI_ALL_RIGHT;
}

int GetFileExImp_Cached(HLOCAL *dest, const char* pOutFile, UINT32 iExtractFileIndex, UINT64 unpackSize, SolidFileCache scCache)
{
    FILE* fp = NULL;
    if (dest) {
        *dest = LocalAlloc(LMEM_FIXED, static_cast<size_t>(unpackSize));
        if (*dest == NULL) {
            return SPI_NO_MEMORY;
        }

        scCache.GetContent(iExtractFileIndex, *dest, unpackSize);
    } else {
        fp = fopen(pOutFile, "wb");
        if (fp == NULL) {
            return SPI_FILE_WRITE_ERROR;
        }
        scCache.OutputContent(iExtractFileIndex, unpackSize, fp);
    }

    if (fp) {
        fclose(fp);
    }

    return SPI_ALL_RIGHT;
}

INT_PTR CALLBACK ProgressDlgProc(HWND hDlgWnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
        case WM_INITDIALOG:
            return FALSE;
    }
    return FALSE;
}

struct ProgressArgument
{
    SPI_PROGRESS procTrueProgress;
    long lTrueData;
    HWND hwnd;
};

int PASCAL ProgressFunc(int nNum, int nDenom, long lData)
{
    ProgressArgument *pArg = reinterpret_cast<ProgressArgument*>(lData);
    SendDlgItemMessage(pArg->hwnd, IDC_PROGRESSBAR, PBM_SETRANGE32, 0, nDenom);
    SendDlgItemMessage(pArg->hwnd, IDC_PROGRESSBAR, PBM_SETPOS, nNum, 0);
    char buf[1024];
    wsprintf(buf, "%d / %d", nNum, nDenom);
    SendDlgItemMessage(pArg->hwnd, IDC_PROGRESSTEXT, WM_SETTEXT, 0, reinterpret_cast<LPARAM>(buf));
    if(pArg->procTrueProgress)
        return (pArg->procTrueProgress)(nNum, nDenom, pArg->lTrueData);
    else
        return 0;
}


int GetFileExImp_Normal(CMyComPtr<IInArchive> archiveHandler, HLOCAL *dest, const char* pOutFile, SPI_PROGRESS lpPrgressCallback, long lData, UINT32 iExtractFileIndex, UINT64 unpackSize, SolidFileCache *scCache)
{
    CExtractCallbackImp *extractCallbackSpec = new CExtractCallbackImp;
    CMyComPtr<IArchiveExtractCallback> extractCallback(extractCallbackSpec);

    ProgressArgument pa = {
        lpPrgressCallback,
        lData,
        0
    };

    FILE* fp = NULL;
    if (dest) {
        *dest = LocalAlloc(LMEM_FIXED, static_cast<size_t>(unpackSize));
        if (*dest == NULL) {
            return SPI_NO_MEMORY;
        }
        extractCallbackSpec->Init(archiveHandler, (char*)*dest, static_cast<UINT32>(unpackSize), NULL, iExtractFileIndex, scCache, ProgressFunc, reinterpret_cast<long>(&pa));
    } else {
        fp = fopen(pOutFile, "wb");
        if (fp == NULL) {
            return SPI_FILE_WRITE_ERROR;
        }
        extractCallbackSpec->Init(archiveHandler, NULL, static_cast<UINT32>(unpackSize), fp, iExtractFileIndex, scCache, ProgressFunc, reinterpret_cast<long>(&pa));
    }

    if(scCache) {
        UINT32 numItems;
        if (S_OK != archiveHandler->GetNumberOfItems(&numItems)) {
            return SPI_FILE_READ_ERROR;
        }
        std::vector<UINT32> v;
        scCache->GetExtractVector(v, iExtractFileIndex, 10, numItems);
        pa.hwnd = CreateDialog(g_hInstance, MAKEINTRESOURCE(IDD_PROGRESS), NULL, ProgressDlgProc);
        ProgressFunc(0, scCache->GetProgressDenom(0), reinterpret_cast<long>(&pa));
        HRESULT result = archiveHandler->Extract(&v[0], v.size(), false, extractCallback);
        for(UINT32 i=0;i<v.size();++i) scCache->Cached(v[i]);
        DestroyWindow(pa.hwnd);
    } else {
        HRESULT result = archiveHandler->Extract(&iExtractFileIndex, 1, false, extractCallback);
    }

    if (fp) {
        fclose(fp);
    }

    if (extractCallbackSpec->m_NumErrors != 0) {
        ::DeleteFile(pOutFile);
        return SPI_FILE_READ_ERROR;
    }

    return SPI_ALL_RIGHT;
}

int GetFileEx(char *filename, HLOCAL *dest, const char* pOutFile, fileInfo *pinfo,
            SPI_PROGRESS lpPrgressCallback, long lData)
{
    UString archiveName = MultiByteToUnicodeString(filename);

    NFind::CFileInfoW archiveFileInfo;
    if (!NFind::FindFile(archiveName, archiveFileInfo) || archiveFileInfo.IsDirectory()) {
        return SPI_FILE_READ_ERROR;
    }

    if (archiveFileInfo.IsDirectory()) {
        return SPI_FILE_READ_ERROR;
    }

    CCodecs *codecs = new CCodecs;
    CMyComPtr<
        #ifdef EXTERNAL_CODECS
        ICompressCodecsInfo
        #else
        IUnknown
        #endif
    > compressCodecsInfo = codecs;
    HRESULT result0 = codecs->Load();

    bool passwordEnabled = false;
    UString password;
    UString defaultItemName;

    CMyComPtr<IInArchive> archiveHandler;

    if (!MyOpenArchive(codecs, archiveName, archiveFileInfo, 
        &archiveHandler, 
        defaultItemName, passwordEnabled, password)) {
            return SPI_FILE_READ_ERROR;
    }

    // 解凍すべきファイルのインデックスを求める
    UINT32 iExtractFileIndex = pinfo->position;
    UINT64 unpackSize = 0;
    if (!GetUINT64Value(archiveHandler, iExtractFileIndex, kpidSize, unpackSize)) {
        return SPI_FILE_READ_ERROR;
    }
OutputDebugPrintf("GetFileEx %s %d %d", pinfo->filename, iExtractFileIndex, (UINT32)unpackSize);

    // 解凍
    if(!lstrcmp((char*)pinfo->method, "7zip_s")
        && ((pinfo->method[7] == 'R' && g_nSolidEnableRar)
            || (pinfo->method[7] == '7' && g_nSolidEnable7z))) {
        SolidFileCache scCache = SolidCache::GetFileCache(filename);
        if(scCache.IsCached(iExtractFileIndex)) { // Cached
            return GetFileExImp_Cached(dest, pOutFile, iExtractFileIndex, unpackSize, scCache);
        } else { // with Cache
            return GetFileExImp_Normal(archiveHandler, dest, pOutFile, lpPrgressCallback, lData, iExtractFileIndex, unpackSize, &scCache);
        }
    } else { // Normal
        return GetFileExImp_Normal(archiveHandler, dest, pOutFile, lpPrgressCallback, lData, iExtractFileIndex, unpackSize, NULL);
    }
}

int GetFileWEx(wchar_t *filename, HLOCAL *dest, const wchar_t* pOutFile, fileInfoW *pinfo,
            SPI_PROGRESS lpPrgressCallback, long lData)
{
    UString archiveName = filename;

    NFind::CFileInfoW archiveFileInfo;
    if (!NFind::FindFile(archiveName, archiveFileInfo) || archiveFileInfo.IsDirectory()) {
        return SPI_FILE_READ_ERROR;
    }

    if (archiveFileInfo.IsDirectory()) {
        return SPI_FILE_READ_ERROR;
    }

    CCodecs *codecs = new CCodecs;
    CMyComPtr<
        #ifdef EXTERNAL_CODECS
        ICompressCodecsInfo
        #else
        IUnknown
        #endif
    > compressCodecsInfo = codecs;
    HRESULT result0 = codecs->Load();

    bool passwordEnabled = false;
    UString password;
    UString defaultItemName;

    CMyComPtr<IInArchive> archiveHandler;

    if (!MyOpenArchive(codecs, archiveName, archiveFileInfo, 
        &archiveHandler, 
        defaultItemName, passwordEnabled, password)) {
            return SPI_FILE_READ_ERROR;
    }

    // 解凍すべきファイルのインデックスを求める
    UINT32 iExtractFileIndex = pinfo->position;
    UINT64 unpackSize = 0;
    if (!GetUINT64Value(archiveHandler, iExtractFileIndex, kpidSize, unpackSize)) {
        return SPI_FILE_READ_ERROR;
    }

    // 解凍
    CExtractCallbackImp *extractCallbackSpec = new CExtractCallbackImp;
    CMyComPtr<IArchiveExtractCallback> extractCallback(extractCallbackSpec);

    FILE* fp = NULL;
    if (dest) {
        *dest = LocalAlloc(LMEM_FIXED, static_cast<size_t>(unpackSize));
        if (*dest == NULL) {
            return SPI_NO_MEMORY;
        }
        extractCallbackSpec->Init(archiveHandler, (char*)*dest, static_cast<UINT32>(unpackSize), NULL, iExtractFileIndex, &SolidCache::GetFileCache(std::string()), lpPrgressCallback, lData);
    } else {
        fp = _wfopen(pOutFile, L"wb");
        if (fp == NULL) {
            return SPI_FILE_WRITE_ERROR;
        }
        extractCallbackSpec->Init(archiveHandler, NULL, static_cast<UINT32>(unpackSize), fp, iExtractFileIndex, &SolidCache::GetFileCache(std::string()), lpPrgressCallback, lData);
    }

    HRESULT result = archiveHandler->Extract(&iExtractFileIndex, 1, false, extractCallback);

    if (fp) {
        fclose(fp);
    }

    if (extractCallbackSpec->m_NumErrors != 0) {
        ::DeleteFileW(pOutFile);
        return SPI_FILE_READ_ERROR;
    }

    return SPI_ALL_RIGHT;
}

int ExtractSolidArchiveEx(LPCWSTR filename, SPI_OnWriteCallback pCallback)
{
    // 解凍すべきファイルのインデックスを求める
    std::vector<fileInfoW> vFileInfos;
    int nRet = GetArchiveInfoWEx_impl(filename, vFileInfos);
    if (nRet != SPI_ALL_RIGHT) {
        return nRet;
    }
    if (vFileInfos.empty()) {
        return SPI_ALL_RIGHT;
    }
    std::vector<UINT> vIndecies(vFileInfos.size());
    std::map<UINT, const fileInfoW*> IndexToFileInfoMap;
    for (size_t i = 0; i < vFileInfos.size(); ++i) {
        vIndecies[i] = vFileInfos[i].position;
        IndexToFileInfoMap[vFileInfos[i].position] = &vFileInfos[i];
    }

    UString archiveName = filename;

    NFind::CFileInfoW archiveFileInfo;
    if (!NFind::FindFile(archiveName, archiveFileInfo) || archiveFileInfo.IsDirectory()) {
        return SPI_FILE_READ_ERROR;
    }

    if (archiveFileInfo.IsDirectory()) {
        return SPI_FILE_READ_ERROR;
    }

    CCodecs *codecs = new CCodecs;
    CMyComPtr<
        #ifdef EXTERNAL_CODECS
        ICompressCodecsInfo
        #else
        IUnknown
        #endif
    > compressCodecsInfo = codecs;
    HRESULT result0 = codecs->Load();

    bool passwordEnabled = false;
    UString password;
    UString defaultItemName;

    CMyComPtr<IInArchive> archiveHandler;

    if (!MyOpenArchive(codecs, archiveName, archiveFileInfo, 
        &archiveHandler, 
        defaultItemName, passwordEnabled, password)) {
            return SPI_FILE_READ_ERROR;
    }

    // 解凍
    CSolidArchiveExtractCallbackImp *extractCallbackSpec = new CSolidArchiveExtractCallbackImp;
    CMyComPtr<IArchiveExtractCallback> extractCallback(extractCallbackSpec);

    extractCallbackSpec->Init(archiveHandler, pCallback, &IndexToFileInfoMap);

    HRESULT result = archiveHandler->Extract(&vIndecies[0], vIndecies.size(), false, extractCallback);

    if (extractCallbackSpec->m_NumErrors != 0) {
        return SPI_FILE_READ_ERROR;
    }

    return SPI_ALL_RIGHT;
}
