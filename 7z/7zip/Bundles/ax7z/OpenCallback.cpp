// OpenCallback.cpp
#include <windows.h>
#include "OpenCallback.h"

#include "Common/StdOutStream.h"
#include "Common/StdInStream.h"
#include "Common/StringConvert.h"

#include "../../Common/FileStreams.h"

#include "Windows/PropVariant.h"
#include "resource.h"

STDMETHODIMP COpenCallbackImp2::SetTotal(const UINT64 *files, const UINT64 *bytes)
{
    return S_OK;
}

STDMETHODIMP COpenCallbackImp2::SetCompleted(const UINT64 *files, const UINT64 *bytes)
{
    return S_OK;
}

STDMETHODIMP COpenCallbackImp2::GetProperty(PROPID propID, PROPVARIANT *value)
{
    NWindows::NCOM::CPropVariant propVariant;
    switch(propID)
    {
    case kpidName:
        propVariant = _fileInfo.Name;
        break;
    case kpidIsFolder:
        propVariant = _fileInfo.IsDirectory();
        break;
    case kpidSize:
        propVariant = _fileInfo.Size;
        break;
    case kpidAttributes:
        propVariant = (UINT32)_fileInfo.Attributes;
        break;
    case kpidLastAccessTime:
        propVariant = _fileInfo.LastAccessTime;
        break;
    case kpidCreationTime:
        propVariant = _fileInfo.CreationTime;
        break;
    case kpidLastWriteTime:
        propVariant = _fileInfo.LastWriteTime;
        break;
    }
    propVariant.Detach(value);
    return S_OK;
}

STDMETHODIMP COpenCallbackImp2::GetStream(const wchar_t *name, 
                                         IInStream **inStream)
{
    *inStream = NULL;
    UString fullPath = _folderPrefix + name;
    if (!NWindows::NFile::NFind::FindFile(fullPath, _fileInfo))
        return S_FALSE;
    if (_fileInfo.IsDirectory())
        return S_FALSE;
    CInFileStream *inFile = new CInFileStream;
    CMyComPtr<IInStream> inStreamTemp = inFile;
    if (!inFile->Open(fullPath))
        return ::GetLastError();
    *inStream = inStreamTemp.Detach();
    return S_OK;
}

INT_PTR CALLBACK COpenCallbackImp2::PasswordDlgProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
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
      COpenCallbackImp2* p = static_cast<COpenCallbackImp2*>(reinterpret_cast<void*>(GetWindowLongPtr(hwnd, DWLP_USER)));
      char buf[4096+1];
      GetDlgItemText(hwnd, IDC_PASSWORD_EDIT, buf, sizeof(buf));
	  AString oemPassword = buf;
      p->Password = MultiByteToUnicodeString(oemPassword, CP_OEMCP);
	  p->PasswordIsDefined = true;
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

STDMETHODIMP COpenCallbackImp2::CryptoGetTextPassword(BSTR *password)
{
    extern HINSTANCE g_hInstance;
    extern UString g_usPassword;
    extern bool g_fPassword;
    extern UString g_usPasswordCachedFile;

    if (!PasswordIsDefined)
    {
        if (g_fPassword)
        {
            Password = g_usPassword;
            PasswordIsDefined = true;
        } else {
            DialogBoxParam(g_hInstance, MAKEINTRESOURCE(IDD_PASSWORD), NULL, (DLGPROC)PasswordDlgProc, reinterpret_cast<LPARAM>(static_cast<void*>(this)));
//          AString oemPassword = g_StdIn.ScanStringUntilNewLine();
//          Password = MultiByteToUnicodeString(oemPassword, CP_OEMCP); 
//          PasswordIsDefined = true;
        }
    }
    if (Password)
    {
        g_usPassword = Password;
        g_fPassword = true;
    }
    CMyComBSTR temp(Password);
    *password = temp.Detach();

    return S_OK;
}

