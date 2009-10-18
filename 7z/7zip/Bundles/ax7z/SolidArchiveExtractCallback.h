// SolidArchiveExtractCallback.h

#pragma once

#ifndef __SOLIDARCHIVEEXTRACTCALLBACK_H
#define __SOLIDARCHIVEEXTRACTCALLBACK_H

//#include "Common/String.h"
#include "../../Common/FileStreams.h"
#include "../../IPassword.h"
#include "../../Archive/IArchive.h"
#include "../../UI/Common/ZipRegistry.h"
#include <stdio.h>
#include "entryFuncs.h"
#include <map>

class CSolidArchiveExtractCallbackImp: 
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

  void SetAbort() { m_bAbort = true; }

private:
//  CMyComPtr<IInArchive> m_ArchiveHandler;
  IInArchive* m_ArchiveHandler;

  bool IsEncrypted(UINT32 index);
public:
  void Init(IInArchive *archive, SPI_OnWriteCallback pCallback, const std::map<UINT, const fileInfoW*>* pIndexToFileInfoMap);

public:
  UINT64 m_NumErrors;
private:
  SPI_OnWriteCallback m_pCallback;
  bool m_bAbort;
  const std::map<UINT, const fileInfoW*>* m_pIndexToFileInfoMap;
};

#endif
