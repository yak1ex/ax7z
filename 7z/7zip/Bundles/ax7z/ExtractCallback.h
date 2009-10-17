// ExtractCallback.h

#pragma once

#ifndef __EXTRACTCALLBACK_H
#define __EXTRACTCALLBACK_H

//#include "Common/String.h"
#include "../../Common/FileStreams.h"
#include "../../IPassword.h"
#include "../../Archive/IArchive.h"
#include "../../UI/Common/ZipRegistry.h"
#include <stdio.h>

#include "entryFuncs.h"

class CExtractCallbackImp: 
  public IArchiveExtractCallback,
  public ICryptoGetTextPassword,
  public CMyUnknownImp
{
public:
  MY_UNKNOWN_IMP1(ICryptoGetTextPassword)

  // IProgress
  STDMETHOD(SetTotal)(UINT64 size);
  STDMETHOD(SetCompleted)(const UINT64 *completeValue);

  // IExtractCallback200
  STDMETHOD(GetStream)(UINT32 index, ISequentialOutStream **outStream, 
      INT32 askExtractMode);
  STDMETHOD(PrepareOperation)(INT32 askExtractMode);
  STDMETHOD(SetOperationResult)(INT32 resultEOperationResult);

  // ICryptoGetTextPassword
  STDMETHOD(CryptoGetTextPassword)(BSTR *password);

private:
//  CMyComPtr<IInArchive> m_ArchiveHandler;
  IInArchive* m_ArchiveHandler;

  bool IsEncrypted(UINT32 index);
public:
  void Init(IInArchive *archive, char** pBuf, UINT32 nBufSize, FILE* fp, UINT32 index);

  UINT64 m_NumErrors;
  static INT_PTR CALLBACK PasswordDlgProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
private:
  char** m_pBuf;
  FILE* m_fp;
  UINT32 m_nBufSize;
  UINT32 m_nIndex;

  bool m_fPassword;
  UString m_usPassword;
};

#endif
