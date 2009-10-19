// OpenCallback.h

#pragma once

#ifndef __OPENCALLBACK_H
#define __OPENCALLBACK_H

//#include "Common/String.h"
#include "Common/MyCom.h"
#include "Windows/FileFind.h"

#include "../../Archive/IArchive.h"
#include "../../IPassword.h"

class COpenCallbackImp2: 
  public IArchiveOpenCallback,
  public IArchiveOpenVolumeCallback,
  public ICryptoGetTextPassword,
  public CMyUnknownImp
{
public:
  MY_UNKNOWN_IMP2(
      IArchiveOpenVolumeCallback, 
      ICryptoGetTextPassword
      )

  STDMETHOD(SetTotal)(const UINT64 *files, const UINT64 *bytes);
  STDMETHOD(SetCompleted)(const UINT64 *files, const UINT64 *bytes);

  // IArchiveOpenVolumeCallback
  STDMETHOD(GetProperty)(PROPID propID, PROPVARIANT *value);
  STDMETHOD(GetStream)(const wchar_t *name, IInStream **inStream);

  // ICryptoGetTextPassword
  STDMETHOD(CryptoGetTextPassword)(BSTR *password);

private:
  UString _folderPrefix;
  NWindows::NFile::NFind::CFileInfoW _fileInfo;
public:
  COpenCallbackImp2() {}
  void LoadFileInfo(const UString &folderPrefix,  const UString &fileName)
  {
    _folderPrefix = folderPrefix;
    if (!NWindows::NFile::NFind::FindFile(_folderPrefix + fileName, _fileInfo))
      throw 1;
  }
};

#endif
