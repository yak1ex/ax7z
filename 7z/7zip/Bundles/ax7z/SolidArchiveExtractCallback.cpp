// SolidArchiveExtractCallback.h
#include <windows.h>
#include "SolidArchiveExtractCallback.h"
#include "Common/StringConvert.h"
#include "Windows/FileDir.h"
#include "Windows/PropVariant.h"
#include "Windows/PropVariantConversions.h"
#include "resource.h"
#include <assert.h>

using namespace NWindows;

void CSolidArchiveExtractCallbackImp::Init(IInArchive *archive, SPI_OnWriteCallback pCallback, const std::map<UINT, const fileInfoW*>* pIndexToFileInfoMap)
{
  m_NumErrors = 0;
  m_ArchiveHandler = archive;
  m_pCallback = pCallback;
  m_pIndexToFileInfoMap = pIndexToFileInfoMap;
  m_bAbort = false;
  m_fPassword = false;
}

bool CSolidArchiveExtractCallbackImp::IsEncrypted(UINT32 index)
{
  NCOM::CPropVariant propVariant;
  if(m_ArchiveHandler->GetProperty(index, kpidEncrypted, &propVariant) != S_OK)
    return false;
  if (propVariant.vt != VT_BOOL)
    return false;
  return VARIANT_BOOLToBool(propVariant.boolVal);
}
  
STDMETHODIMP CSolidArchiveExtractCallbackImp::SetTotal(UINT64 size)
{
  return S_OK;
}

STDMETHODIMP CSolidArchiveExtractCallbackImp::SetCompleted(const UINT64 *completeValue)
{
  return S_OK;
}

class CCallbackStream:
  public ISequentialOutStream,
  public CMyUnknownImp
{
public:
    CCallbackStream() : m_pCallback(NULL), m_nProcessed(0) {}
    void Init(SPI_OnWriteCallback pCallback, const fileInfoW* pFileInfo, CSolidArchiveExtractCallbackImp* pExtractCallback) {
        m_pCallback = pCallback;
        m_pFileInfo = pFileInfo;
        m_pExtractCallback = pExtractCallback;
    }
  MY_UNKNOWN_IMP

  STDMETHOD(Write)(const void *data, UINT32 size, UINT32 *processedSize);
  STDMETHOD(WritePart)(const void *data, UINT32 size, UINT32 *processedSize);
protected:
    SPI_OnWriteCallback m_pCallback;
    UINT32 m_nProcessed;
    const fileInfoW* m_pFileInfo;
    CSolidArchiveExtractCallbackImp* m_pExtractCallback;
};

STDMETHODIMP CCallbackStream::Write(const void *data, UINT32 size, UINT32 *processedSize)
{
    UCHAR bStop = 0;
    m_pCallback(data, size, m_nProcessed, m_pFileInfo, &bStop);
    m_nProcessed += size;

    *processedSize = size;
    if (bStop == 0) {
        return S_OK;
    } else {
        m_pExtractCallback->SetAbort();
        return E_ABORT;
    }
}

STDMETHODIMP CCallbackStream::WritePart(const void *data, UINT32 size, UINT32 *processedSize)
{
    return Write(data, size, processedSize);
}

STDMETHODIMP CSolidArchiveExtractCallbackImp::GetStream(UINT32 index,
    ISequentialOutStream **outStream, INT32 askExtractMode)
{
    if (m_bAbort) {
        return E_ABORT;
    }
    CCallbackStream* pRealStream = new CCallbackStream;
    pRealStream->AddRef();
    std::map<UINT, const fileInfoW*>::const_iterator it = m_pIndexToFileInfoMap->find(index);
    if (it == m_pIndexToFileInfoMap->end()) {
        return E_ABORT;
    }
    pRealStream->Init(m_pCallback, it->second, this);
    *outStream = pRealStream;
    return S_OK;
}

STDMETHODIMP CSolidArchiveExtractCallbackImp::PrepareOperation(INT32 askExtractMode)
{
  return S_OK;
}

STDMETHODIMP CSolidArchiveExtractCallbackImp::SetOperationResult(INT32 resultEOperationResult)
{
  switch(resultEOperationResult)
  {
    case NArchive::NExtract::NOperationResult::kOK:
    {
      break;
    }
    default:
    {
      m_NumErrors++;
    }
  }
  return S_OK;
}

INT_PTR CALLBACK CSolidArchiveExtractCallbackImp::PasswordDlgProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  switch(uMsg)
  {
  case WM_INITDIALOG:
    SetWindowLongPtr(hwnd, DWLP_USER, lParam);
    return TRUE;
  case WM_COMMAND:
    switch(LOWORD(wParam)) {
    case IDOK:
    {
      CSolidArchiveExtractCallbackImp* p = static_cast<CSolidArchiveExtractCallbackImp*>(reinterpret_cast<void*>(GetWindowLongPtr(hwnd, DWLP_USER)));
      char buf[4096+1];
      GetDlgItemText(hwnd, IDC_PASSWORD_EDIT, buf, sizeof(buf));
	  AString oemPassword = buf;
      p->m_usPassword = MultiByteToUnicodeString(oemPassword, CP_OEMCP);
	  p->m_fPassword = true;
      EndDialog(hwnd, TRUE);
      break;
	}
    case IDCANCEL:
      EndDialog(hwnd, FALSE);
      break;
	default:
      return FALSE;
	}
  default:
    return FALSE;
  }
  return FALSE; // not reached
}

STDMETHODIMP CSolidArchiveExtractCallbackImp::CryptoGetTextPassword(BSTR *password)
{
  extern HINSTANCE g_hInstance;
  if (!m_fPassword)
  {
    DialogBoxParam(g_hInstance, MAKEINTRESOURCE(IDD_PASSWORD), NULL, (DLGPROC)PasswordDlgProc, reinterpret_cast<LPARAM>(static_cast<void*>(this)));
//    AString oemPassword = g_StdIn.ScanStringUntilNewLine();
//    m_fPassword = MultiByteToUnicodeString(oemPassword, CP_OEMCP); 
//    m_fPassword = true;
  }
  CMyComBSTR tempName(m_usPassword);
  *password = tempName.Detach();

  return S_OK;
}
  
