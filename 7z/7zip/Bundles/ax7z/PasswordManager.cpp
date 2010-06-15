//PasswordManager.cpp
#include "PasswordManager.h"
#include "../../../Common/StringConvert.h"
#include "resource.h"

PasswordManager& PasswordManager::Get()
{
	static PasswordManager pm;
	return pm;
}

const UString& PasswordManager::GetPassword(bool bFilename)
{
	extern HINSTANCE g_hInstance;
	m_bFilename = bFilename;
	if(IsRetry()) {
		m_bPassword = false;
	}
	if(!m_bPassword && !m_bSkip && !m_bSkipArc) {
		DialogBoxParam(g_hInstance, MAKEINTRESOURCE(IDD_PASSWORD), NULL, (DLGPROC)PasswordDlgProc, reinterpret_cast<LPARAM>(static_cast<void*>(this)));
		m_bError = false;
	}
	if(!m_bPassword) {
		m_usPassword = L"";
	}
	m_bPasswordUsed = true;
	return m_usPassword;
}

bool PasswordManager::IsRetry() const
{
	return m_bPasswordUsed && m_bError && !m_bSkip && !m_bSkipArc;
}

bool PasswordManager::IsValid() const
{
	return !m_bArchiveChanged && !m_bError && !m_bSkip && !m_bSkipArc;
}

void PasswordManager::NotifyArchive(const UString& usArchive)
{
	if(m_usArchive != usArchive) {
		m_bPasswordUsed = false;
		m_bArchiveChanged = true;
		m_bError = false;
		m_bSkip = false;
		m_bSkipArc = false;
		m_usArchive = usArchive;
	}
}

void PasswordManager::NotfiyEndFile()
{
	m_bPasswordUsed = false;
	m_bError = false;
	m_bSkip = false;
}

void PasswordManager::NotifyError()
{
	m_bError = true;
}

void PasswordManager::ClearError()
{
	m_bError = false;
}

void PasswordManager::Reset()
{
	m_bPassword = false;
}

INT_PTR CALLBACK PasswordManager::PasswordDlgProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch(uMsg)
	{
	case WM_INITDIALOG:
	{
		SetWindowLongPtr(hwnd, DWLP_USER, lParam);
		PasswordManager* p = static_cast<PasswordManager*>(reinterpret_cast<void*>(GetWindowLongPtr(hwnd, DWLP_USER)));
		if(p->m_bFilename) EnableWindow(GetDlgItem(hwnd, IDC_BUTTON_SKIP), FALSE);
		if(p->IsRetry()) SetWindowText(GetDlgItem(hwnd, IDC_STATIC_GUIDE), "Extract error, maybe caused by wrong password,\nretype password:");
		HMENU hSysMenu = GetSystemMenu(hwnd, FALSE);
		InsertMenu(hSysMenu, SC_CLOSE, MF_STRING | (p->m_bTopMost ? MF_CHECKED : 0), IDM_TOPMOST, TEXT("TopMost"));
		InsertMenu(hSysMenu, SC_CLOSE, MF_SEPARATOR, 0, 0);
		return TRUE;
	}
	case WM_INITMENUPOPUP:
	{
		HMENU hSysMenu = GetSystemMenu(hwnd, FALSE);
		if(hSysMenu == static_cast<HANDLE>(reinterpret_cast<void*>(wParam))) {
			PasswordManager* p = static_cast<PasswordManager*>(reinterpret_cast<void*>(GetWindowLongPtr(hwnd, DWLP_USER)));
			ModifyMenu(hSysMenu, IDM_TOPMOST, MF_BYCOMMAND | MF_STRING | (p->m_bTopMost ? MF_CHECKED : 0), IDM_TOPMOST, TEXT("TopMost"));
		}
		return TRUE;
	}
	case WM_SYSCOMMAND:
		if((wParam & 0xFFF0) == IDM_TOPMOST) {
			PasswordManager* p = static_cast<PasswordManager*>(reinterpret_cast<void*>(GetWindowLongPtr(hwnd, DWLP_USER)));
			p->m_bTopMost = !p->m_bTopMost;
			if(p->m_bTopMost) {
				SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
			} else {
				SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
			}
			return TRUE;
		}
		return FALSE;
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
			p->m_bArchiveChanged = false;
			EndDialog(hwnd, TRUE);
			break;
		}
		case IDC_BUTTON_SKIP:
		{
			PasswordManager* p = static_cast<PasswordManager*>(reinterpret_cast<void*>(GetWindowLongPtr(hwnd, DWLP_USER)));
			p->m_bSkip = true;
			p->m_bPassword = false;
			EndDialog(hwnd, FALSE);
			break;
		}
		case IDC_BUTTON_SKIPARC:
		{
			PasswordManager* p = static_cast<PasswordManager*>(reinterpret_cast<void*>(GetWindowLongPtr(hwnd, DWLP_USER)));
			p->m_bSkipArc = true;
			p->m_bPassword = false;
			EndDialog(hwnd, FALSE);
			break;
		}
		default:
			return FALSE;
		}
	default:
		return FALSE;
	}
	return FALSE; // not reached
}