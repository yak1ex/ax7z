#ifndef YAK_UTIL_WINDOWS_HPP
#define YAK_UTIL_WINDOWS_HPP

#include <windows.h>

#include <vector>
#include <string>
#include <stdexcept>

namespace yak {
namespace util {
namespace windows {

class VersionResource
{
public:
	VersionResource(const std::string &sPath) : pTrans(0)
	{
		const char* sz = sPath.c_str();
		DWORD dwDummy;
		DWORD dwSize = GetFileVersionInfoSize(sz, &dwDummy);
		if(!dwSize) throw std::runtime_error("File not found");
		buf.resize(dwSize);
		GetFileVersionInfo(sz, 0, dwSize, &buf[0]);
	}
	struct LANGANDCODEPAGE
	{
		WORD wLanguage;
		WORD wCodePage;
	};
	UINT GetLangAndCodePageCount() const
	{
		if(!pTrans) SetTranslation();
		return uiTrans / sizeof(LANGANDCODEPAGE);
	}
	const LANGANDCODEPAGE& GetLangAndCodePage(UINT idx) const
	{
		if(!pTrans) SetTranslation();
		return pTrans[idx];
	}
	enum VT_TYPE {
		COMMENTS, COMPANY_NAME, FILE_DESCRIPTION, FILE_VERSION,
		INTERNAL_NAME, LEGAL_COPYRIGHT, LEGAL_TRADEMARKS, ORIGINAL_FILENAME,
		PRODUCT_NAME, PRODUCT_VERSION, PRIVATE_BUILD, SPECIAL_BUID,
		EOT,
	};
	bool IsExisted(VT_TYPE type, UINT idx = 0) const
	{
		std::string key = Key(type, idx);
		LPVOID p; UINT ui;
		// Probably win32api declaration error
		return VerQueryValue(const_cast<BYTE*>(&buf[0]), const_cast<LPSTR>(key.c_str()), &p, &ui) ? true : false;
	}
	std::string GetValue(VT_TYPE type, UINT idx = 0) const
	{
		std::string key = Key(type, idx);
		LPVOID p; UINT ui;
		// Probably win32api declaration error
		if(VerQueryValue(const_cast<BYTE*>(&buf[0]), const_cast<LPSTR>(key.c_str()), &p, &ui)) {
			return std::string(static_cast<char*>(p));
		} else return std::string();
	}
	static const char* GetKeyName(VT_TYPE idx)
	{
		static const char* const szBlockName[] = {
			"Comments", "CompanyName", "FileDescription", "FileVersion",
			"InternalName", "LegalCopyright", "LegalTrademarks", "OriginalFilename",
			"ProductName", "ProductVersion", "PrivateBuild", "SpecialBuild",
		};
		const UINT num = sizeof(szBlockName) / sizeof(szBlockName[0]);
		if(idx < num) return szBlockName[idx];
		else return 0;
	}
private:
	void SetTranslation() const
	{
		LPVOID p;
		// Probably win32api declaration error
		VerQueryValue(const_cast<BYTE*>(&buf[0]), "\\VarFileInfo\\Translation", &p, &uiTrans);
		pTrans = static_cast<LANGANDCODEPAGE*>(p);
	}
	std::string Key(VT_TYPE type, UINT idx) const
	{
		if(!pTrans) SetTranslation();
		std::string buf2(4096, '\0');
		const char* key = GetKeyName(type);
		if(key) {
			std::size_t size = wsprintf(&buf2[0], "\\StringFileInfo\\%04x%04x\\%s", pTrans[idx].wLanguage, pTrans[idx].wCodePage, key);
			buf2.resize(size);
		} else buf2.resize(0);
		return buf2;
	}
	std::vector<BYTE> buf;
	mutable LANGANDCODEPAGE *pTrans;
	mutable UINT uiTrans;
};

} // namespace windows
} // namespace util
} // namespace yak

#endif
