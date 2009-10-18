//PasswordManager.cpp
#include "PasswordManager.h"
#include "../../../Common/StringConvert.h"
#include "resource.h"

PasswordManager& PasswordManager::Get()
{
	static PasswordManager pm;
	return pm;
}

const UString& PasswordManager::GetPassword(bool bRetry)
{
	extern HINSTANCE g_hInstance;
	if(!m_bPassword) {
		DialogBoxParam(g_hInstance, MAKEINTRESOURCE(IDD_PASSWORD), NULL, (DLGPROC)PasswordDlgProc, reinterpret_cast<LPARAM>(static_cast<void*>(this)));
	}
	if(!m_bPassword) {
		m_usPassword = L"";
	}
	return m_usPassword;
}

void PasswordManager::NotifyArchive(const UString& usArchive)
{
	if(m_usArchive != usArchive) {
		m_bPassword = false;
		m_usArchive = usArchive;
	}
}

INT_PTR CALLBACK PasswordManager::PasswordDlgProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
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
			PasswordManager* p = static_cast<PasswordManager*>(reinterpret_cast<void*>(GetWindowLongPtr(hwnd, DWLP_USER)));
			char buf[4096+1];
			GetDlgItemText(hwnd, IDC_PASSWORD_EDIT, buf, sizeof(buf));
			AString oemPassword = buf;
			p->m_usPassword = MultiByteToUnicodeString(oemPassword, CP_OEMCP);
			p->m_bPassword = true;
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