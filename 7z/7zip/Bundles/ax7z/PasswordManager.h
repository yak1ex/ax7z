//PasswordManager.h

#pragma once

#ifndef PASSWORDMANAGER_H
#define PASSWORDMANAGER_H

#include "../../../Common/MyString.h"

class PasswordManager
{
public:
	static PasswordManager& Get();
	bool IsRetry() const;
	bool IsDefined() const { return m_bPassword; }
	bool IsValid() const;
	const UString& GetPassword(bool bFilename);
	void NotifyArchive(const UString& usArchive);
	void NotfiyEndFile();
	void NotifyError();
	void ClearError();
	void Reset();
private:
	static INT_PTR CALLBACK PasswordDlgProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
	PasswordManager() : m_bPassword(false), m_bPasswordUsed(false), m_bArchiveChanged(false), m_bError(false), m_bSkip(false), m_bSkipArc(false) {}
	bool m_bPassword;
	bool m_bPasswordUsed;
	bool m_bArchiveChanged;
	bool m_bError;
	bool m_bFilename;
	bool m_bSkip;
	bool m_bSkipArc;
	UString m_usPassword;
	UString m_usArchive;
};

#endif
