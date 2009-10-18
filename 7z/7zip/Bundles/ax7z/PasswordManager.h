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
	const UString& GetPassword(bool bRetry);
	void NotifyArchive(const UString& usArchive);
	void NotfiyEndFile();
private:
	static INT_PTR CALLBACK PasswordDlgProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
	PasswordManager() : m_bPassword(false) {}
	bool m_bPassword;
	UString m_usPassword;
	UString m_usArchive;
};

#endif
