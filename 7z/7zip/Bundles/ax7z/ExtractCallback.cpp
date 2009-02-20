// ExtractCallback.h
#define NOMINMAX
#include <windows.h>
#undef NOMINMAX
#include "ExtractCallback.h"
#include "Common/StringConvert.h"
#include "Windows/FileDir.h"
#include "Windows/PropVariant.h"
#include "Windows/PropVariantConversions.h"
#include "resource.h"
#include <assert.h>

using namespace NWindows;

void CExtractCallbackImp::Init(IInArchive *archive, char* pBuf, UINT32 nBufSize, FILE* fp, UINT32 index, SolidFileCache *cache, SPI_PROGRESS lpPrgressCallback, long lData)
{
  assert(!pBuf || !fp);
  m_NumErrors = 0;
  m_ArchiveHandler = archive;
  m_pBuf = pBuf;
  m_fp = fp;
  m_nBufSize = nBufSize;
  m_nIndex = index;
  m_cache = cache;
  m_lpPrgressCallback = lpPrgressCallback;
  m_lData = lData;
  m_fPassword = false;
}

bool CExtractCallbackImp::IsEncrypted(UINT32 index)
{
  NCOM::CPropVariant propVariant;
  if(m_ArchiveHandler->GetProperty(index, kpidEncrypted, &propVariant) != S_OK)
    return false;
  if (propVariant.vt != VT_BOOL)
    return false;
  return VARIANT_BOOLToBool(propVariant.boolVal);
}
  
STDMETHODIMP CExtractCallbackImp::SetTotal(UINT64 size)
{
  return S_OK;
}

STDMETHODIMP CExtractCallbackImp::SetCompleted(const UINT64 *completeValue)
{
  return S_OK;
}

class CMemOutStream:
  public ISequentialOutStream,
  public CMyUnknownImp
{
public:
    CMemOutStream() : m_iPos(0), m_pBuf(NULL), m_nBufSize(0) {}
    void Init(char* pBuf, UINT32 nBufSize, FILE* fp, bool bValid, SolidFileCache *cache, UINT32 nIndex)
	{
		m_pBuf = pBuf;
		m_fp = fp;
		m_nBufSize = nBufSize;
		m_bValid = bValid;
		m_cache = cache;
		m_index = nIndex;
	}
  MY_UNKNOWN_IMP

  STDMETHOD(Write)(const void *data, UINT32 size, UINT32 *processedSize);
  STDMETHOD(WritePart)(const void *data, UINT32 size, UINT32 *processedSize);
protected:
    UINT32 m_iPos;
    char* m_pBuf;
    FILE* m_fp;
    UINT32 m_nBufSize;
    bool m_bValid;
	SolidFileCache *m_cache;
	UINT32 m_index;
};

STDMETHODIMP CMemOutStream::Write(const void *data, UINT32 size, UINT32 *processedSize)
{
    UINT32 dummy;
    if (processedSize == NULL) {
        processedSize = &dummy;
    }
    
    if(m_cache && !m_cache->IsCached(m_index)) m_cache->Append(m_index, data, size);

    if (!m_bValid) {
        *processedSize = size;
        return S_OK;
    }

    if (m_nBufSize < m_iPos + size) {
        // 念のためチェック
        return S_FALSE;
    }

    if (m_pBuf) {
        memcpy(m_pBuf + m_iPos, data, size);
    } else {
        fwrite(data, size, 1, m_fp);
    }
    m_iPos += size;
    *processedSize = size;
    return S_OK;
}

STDMETHODIMP CMemOutStream::WritePart(const void *data, UINT32 size, UINT32 *processedSize)
{
    return Write(data, size, processedSize);
}

STDMETHODIMP CExtractCallbackImp::GetStream(UINT32 index,
    ISequentialOutStream **outStream, INT32 askExtractMode)
{
    CMemOutStream* pRealStream = new CMemOutStream;
    pRealStream->AddRef();
    pRealStream->Init(m_pBuf, m_nBufSize, m_fp, index == m_nIndex, m_cache, index);
    *outStream = pRealStream;
// TODO: support abort?
	if(m_lpPrgressCallback && m_cache)
		m_lpPrgressCallback(m_cache->GetProgress(index), m_cache->GetProgressDenom(index), m_lData);
    return S_OK;
}

STDMETHODIMP CExtractCallbackImp::PrepareOperation(INT32 askExtractMode)
{
  return S_OK;
}

STDMETHODIMP CExtractCallbackImp::SetOperationResult(INT32 resultEOperationResult)
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

INT_PTR CALLBACK CExtractCallbackImp::PasswordDlgProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
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
      CExtractCallbackImp* p = static_cast<CExtractCallbackImp*>(reinterpret_cast<void*>(GetWindowLongPtr(hwnd, DWLP_USER)));
      char buf[4096+1];
      GetDlgItemText(hwnd, IDC_PASSWORD_EDIT, buf, sizeof(buf));
OutputDebugPrintf("PasswordDlgProc: %s", buf);
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

STDMETHODIMP CExtractCallbackImp::CryptoGetTextPassword(BSTR *password)
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
  
