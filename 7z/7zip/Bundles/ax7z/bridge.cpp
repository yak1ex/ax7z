/*
7-zip decode engine - susie bridge
*/

#include <windows.h>
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

HINSTANCE g_hInstance = 0;

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

static bool IsSolid(IInArchive* archive, const NFind::CFileInfoW& archiverInfo, UINT32 numItems)
{
    // rar の場合は kpidSolidが設定されているようなのでそちらを使用
    NCOM::CPropVariant propVariant;
    if (S_OK == archive->GetArchiveProperty(kpidSolid, &propVariant)) {
        if (propVariant.vt == VT_BOOL) {
            return propVariant.bVal ? true : false;
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
                return true;
			}
			bFlag = TRUE;
		}
    }
    return false;
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

char buf[2048];

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
    bool bSolid = IsSolid(archiveHandler, archiveFileInfo, numItems);

wsprintf(buf, "GetArchiveInfoEx: %d %d", numItems, bSolid);
OutputDebugString(buf);

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
        if (bSolid) {
    		lstrcpy((char*)pinfo->method, "7zip_s");
        } else {
    		lstrcpy((char*)pinfo->method, "7zip");
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
wsprintf(buf, "GetArchiveInfoEx: %d %s", i, s);
OutputDebugString(buf);

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
    bool bSolid = IsSolid(archiveHandler, archiveFileInfo, numItems);

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
        if (bSolid) {
    		lstrcpy((char*)pinfo->method, "7zip_s");
        } else {
    		lstrcpy((char*)pinfo->method, "7zip");
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

int GetFileEx(char *filename, HLOCAL *dest, const char* pOutFile, fileInfo *pinfo,
			SPI_PROGRESS lpPrgressCallback, long lData)
{
OutputDebugString("GetFileEx");
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
char buf[2048];
wsprintf(buf, "GetFileEx: %d %s %d", iExtractFileIndex, pinfo->filename, (UINT32)unpackSize);
OutputDebugString(buf);

    // 解凍
    CExtractCallbackImp *extractCallbackSpec = new CExtractCallbackImp;
    CMyComPtr<IArchiveExtractCallback> extractCallback(extractCallbackSpec);

	SolidCache scCache = SolidCache::GetFileCache(filename);
	bool fCached = !lstrcmp((char*)pinfo->method, "7zip_s") && scCache.IsCached(iExtractFileIndex);
    FILE* fp = NULL;
    if (dest) {
        *dest = LocalAlloc(LMEM_FIXED, static_cast<size_t>(unpackSize));
        if (*dest == NULL) {
            return SPI_NO_MEMORY;
        }

if(fCached) {
//wsprintf(buf, "GetFileEx: cached memory %d %p", table[filename][iExtractFileIndex].size(), &table[filename][iExtractFileIndex][0]);
//OutputDebugString(buf);
scCache.GetContent(iExtractFileIndex, *dest, unpackSize);
} else {
OutputDebugString("GetFileEx: uncached for memory");
	extractCallbackSpec->Init(archiveHandler, (char*)*dest, static_cast<UINT32>(unpackSize), NULL, iExtractFileIndex, &scCache);
}
    } else {
        fp = fopen(pOutFile, "wb");
        if (fp == NULL) {
            return SPI_FILE_WRITE_ERROR;
        }
if(fCached) {
//wsprintf(buf, "GetFileEx: cached memory %d %p", table[filename][iExtractFileIndex].size(), &table[filename][iExtractFileIndex][0]);
//OutputDebugString(buf);
scCache.OutputContent(iExtractFileIndex, unpackSize, fp);
} else {
OutputDebugString("GetFileEx: uncached for file");
	extractCallbackSpec->Init(archiveHandler, NULL, static_cast<UINT32>(unpackSize), fp, iExtractFileIndex, &scCache);
}
	}

if(!fCached) {
if(!lstrcmp((char*)pinfo->method, "7zip_s")) {
std::vector<UINT32> v(iExtractFileIndex+1);
for(int i=0;i<v.size();++i) v[i]=i;
HRESULT result = archiveHandler->Extract(&v[0], v.size(), false, extractCallback);
for(int i=0;i<v.size();++i) scCache.Cached(i);
} else {
    HRESULT result = archiveHandler->Extract(&iExtractFileIndex, 1, false, extractCallback);
}
}
    if (fp) {
        fclose(fp);
    }

    if (!fCached && extractCallbackSpec->m_NumErrors != 0) {
OutputDebugString("GetFileEx: error");
        ::DeleteFile(pOutFile);
        return SPI_FILE_READ_ERROR;
    }

OutputDebugString("GetFileEx: success");
	return SPI_ALL_RIGHT;
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
		extractCallbackSpec->Init(archiveHandler, (char*)*dest, static_cast<UINT32>(unpackSize), NULL, iExtractFileIndex, &SolidCache::GetFileCache(std::string()));
    } else {
        fp = _wfopen(pOutFile, L"wb");
        if (fp == NULL) {
            return SPI_FILE_WRITE_ERROR;
        }
		extractCallbackSpec->Init(archiveHandler, NULL, static_cast<UINT32>(unpackSize), fp, iExtractFileIndex, &SolidCache::GetFileCache(std::string()));
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
