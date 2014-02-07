#include <iostream>
#define NOMINMAX
#include <windows.h>
#undef NOMINMAX
#include <sys/types.h>
#include <sys/stat.h>

#include "sqlite3/sqlite3.h"
#include "sqlite3/sqlite3helper.h"
#include "SolidCache.h"

#include "7z/Common/MyString.h"
#include "7z/Common/StringConvert.h"

using yak::sqlite::Statement;

void dump_table(sqlite3 *db, char *name)
{
	std::cout << '[' << name << ']' << std::endl;
	char buf[2048];
	sqlite3_snprintf(sizeof(buf), buf, "select * from %s", name);
	Statement stmt(db, buf);
	while(stmt()) {
		int nColumn = stmt.get_count();
		for(int i=0;i<nColumn;++i) {
			std::cout << stmt.get_name(i) << ':';
			switch(stmt.get_type(i)) {
			case SQLITE_INTEGER:
				std::cout << stmt.get_int(i);
				break;
			case SQLITE_FLOAT:
				std::cout << stmt.get_double(i);
				break;
			case SQLITE_TEXT:
				std::cout << stmt.get_text(i);
				break;
			case SQLITE_BLOB:
				std::cout << '(' << stmt.get_bytes(i) << ')';
				break;
			case SQLITE_NULL:
				std::cout << "NULL";
				break;
			}
			std::cout << '\t';
		}
		std::cout << std::endl;
	}
}

SolidCacheDisk::~SolidCacheDisk()
{
	m_db.close();
}

bool SolidCacheDisk::OpenDB_()
{
	try {
		std::string sDB = GetCacheFolder() + "ax7z_s.db";
		UString us(GetUnicodeString(sDB.c_str()));
		int nLen = WideCharToMultiByte(CP_UTF8, 0, us.GetBuffer(0), -1, 0, 0, 0, 0);
		std::vector<char> vBuf(nLen);
		WideCharToMultiByte(CP_UTF8, 0, us.GetBuffer(0), -1, &vBuf[0], vBuf.size(), 0, 0);
		m_db.reopen(&vBuf[0]);
	} catch(yak::sqlite::sqlite_error &e) {
		return false;
	}
	return true;
}

void SolidCacheDisk::InitDB_()
{
	Statement stmt_enc(m_db, "pragma encoding");
	if(stmt_enc()) {
		std::string sEnc(stmt_enc.get_text(0));
		if(sEnc != "UTF-8") {
			OutputDebugPrintf("SolidCacheDisk::InitDB: encoding is not UTF-8, recreating DB\n");
			stmt_enc.finalize();
			std::string sDB = GetCacheFolder() + "ax7z_s.db";
			try {
				m_db.close();
			} catch (yak::sqlite::sqlite_error &e) {
				OutputDebugPrintf("SolidCache::InitDB: sqlite3_close for %s failed by %s\n", sDB.c_str(), sqlite3_errmsg(m_db));
			}
			if(!DeleteFile(sDB.c_str())) {
				OutputDebugPrintf("SolidCache::InitDB: deleting %s failed %d\n", sDB.c_str(), GetLastError());
			}
			if(!OpenDB_()) {
				OutputDebugPrintf("SolidCache::InitDB: sqlite3_open for %s failed by %s\n", sDB.c_str(), sqlite3_errmsg(m_db));
			}
			sqlite3_exec(m_db, "PRAGMA encoding = \"UTF-8\"", NULL, NULL, NULL);
		}
	}
	m_db.exec("PRAGMA journal_mode = OFF");
	m_db.exec("PRAGMA synchronous = OFF"); // very effective
	m_db.exec("PRAGMA temp_store = MEMORY");
	Statement stmt(m_db, "pragma user_version");
	if(stmt() && stmt.get_int(0) > 0) {
		switch(stmt.get_int(0)) {
		case 1:
			OutputDebugPrintf("SolidCacheDisk::InitDB: version 1 exists, need to update\n");
			m_db.exec("ALTER TABLE archive ADD COLUMN mtime INTEGER");
			m_db.exec("ALTER TABLE archive ADD COLUMN size INTEGER");
			{
				Statement stmt2(m_db, "SELECT rowid FROM entry WHERE completed <> 1");
				while(stmt2()) {
					OutputDebugPrintf("SolidCacheDisk::InitDB(): DeleteFile %s\n", GetFileName(stmt2.get_int64(0)).c_str());
					DeleteFile(GetFileName(stmt2.get_int64(0)).c_str());
				}
			}
			m_db.exec("DELETE FROM entry WHERE completed <> 1");
			m_db.exec("UPDATE entry SET completed = 0");
			m_db.exec("PRAGMA user_version = 2");
			break;
		case 2:
			OutputDebugPrintf("SolidCacheDisk::InitDB: version 2 exists, skip\n");
			break;
		default:
			OutputDebugPrintf("SolidCacheDisk::InitDB: Unknown version %d\n", stmt.get_int(0));
			break;
		}
	} else {
		OutputDebugPrintf("SolidCacheDisk::InitDB: Create version 2\n");
		m_db.exec("PRAGMA user_version = 2");
		m_db.exec("CREATE TABLE archive (idx INTEGER PRIMARY KEY AUTOINCREMENT, path TEXT, atime INTEGER, mtime INTEGER, size INTEGER)");
		m_db.exec("CREATE TABLE entry (aidx INTERGER, idx INTEGER, size INTEGER, completed INTEGER, CONSTRAINT pkey PRIMARY KEY (aidx, idx) )");
	}
}

void SolidCacheDisk::CheckDB_()
{
	Statement stmt(m_db, "SELECT idx, path, mtime, size FROM archive");
	while(stmt()) {
		if(stmt.get_int64(2) == 0 && stmt.get_int64(3) == 0) continue;
		struct __stat64 st;
		if(!_stat64(stmt.get_text(1), &st) && st.st_mtime == stmt.get_int64(2) && st.st_size == stmt.get_int64(3))
			continue;
		Statement stmt2(m_db, "SELECT rowid FROM entry WHERE aidx = ?");
		stmt2.bind(1, stmt.get_int(0));
		while(stmt2()) {
			OutputDebugPrintf("SolidCacheDisk::CheckDB(): DeleteFile %s\n", GetFileName(stmt2.get_int64(0)).c_str());
			DeleteFile(GetFileName(stmt2.get_int64(0)).c_str());
		}
		Statement(m_db, "DELETE FROM entry WHERE aidx = ?").bind(1, stmt.get_int(0))();
		OutputDebugPrintf("SolidCacheDisk::CheckDB invalid %s %u actualtime %" UINT64_S "u dbmtime %" UINT64_S "u actualsize %" UINT64_S "u dbsize %" UINT64_S "u\n", stmt.get_text(1), stmt.get_int(0), st.st_mtime, stmt.get_int64(2), st.st_size, stmt.get_int64(3));
	}
	PurgeUnreferenced_();
}

std::string SolidCacheDisk::GetFileName(__int64 id) const
{
	char buf[32768];
	wsprintf(buf, "%sax7z_s%08X%08X.tmp", m_sCacheFolder.c_str(), static_cast<unsigned int>(id >> 32), static_cast<unsigned int>(id & 0xFFFFFFFF));
	return std::string(buf);
}

bool SolidCacheDisk::ExistsArchive_(const char* archive) const
{
	struct __stat64 st;
	if(!_stat64(archive, &st)) {
		Statement stmt(m_db, "SELECT COUNT(*) FROM archive WHERE path = ? AND mtime = ? AND size = ?");
		stmt.bind(1, archive).bind(2, st.st_mtime).bind(3, st.st_size);
		bool success = stmt();
		OutputDebugPrintf("SolidCacheDisk::ExistsArchive_(): archive %s : %d [%d]\n", archive, int(success), stmt.get_int(0));
		return success && stmt.get_int(0) > 0;
	}
// TODO: proper error handling
	OutputDebugPrintf("ERROR on SolidCacheDisk::ExistArchive_ archive %s\n", archive);
	return false;
}

void SolidCacheDisk::AddArchive_(const char* archive)
{
	struct __stat64 st;
	if(!_stat64(archive, &st)) {
		Statement(m_db, "INSERT INTO archive (path, atime, mtime, size) VALUES (?, 0, ?, ?)")
			.bind(1, archive).bind(2, st.st_mtime).bind(3, st.st_size)();
		OutputDebugPrintf("SolidCacheDisk::AddArchive archive %s mtime %" UINT64_S "u size %" UINT64_S "u\n", archive, st.st_mtime, st.st_size);
	} else {
// TODO: proper error handling
		OutputDebugPrintf("ERROR on SolidCacheDisk::AddArchive_ archive %s\n", archive);
	}
}

unsigned int SolidCacheDisk::GetArchiveIdx_(const char* archive) const
{
	struct __stat64 st;
	if(!_stat64(archive, &st)) {
		Statement stmt(m_db, "SELECT idx FROM archive WHERE path = ? AND mtime = ? AND size = ?");
		bool success = stmt.bind(1, archive).bind(2, st.st_mtime).bind(3, st.st_size)();
		OutputDebugPrintf("SolidCacheDisk::GetArchiveIdx_(): archive %s : %d [%d]\n", archive, int(success), stmt.get_int(0));
		return stmt.get_int(0);
	}
// TODO: proper error handling
	OutputDebugPrintf("ERROR on SolidCacheDisk::GetArchiveIdx_ archive %s\n", archive);
	return 0;
}

bool SolidCacheDisk::ExistsEntry_(unsigned int aidx, unsigned int idx) const
{
	Statement stmt(m_db, "SELECT COUNT(*) FROM entry WHERE aidx = ? AND idx = ?");
	stmt.bind(1, aidx).bind(2, idx);
	return stmt() && stmt.get_int(0) > 0;
}

void SolidCacheDisk::AppendEntry_(unsigned int aidx, unsigned int idx, const void* data, unsigned int size)
{
	Statement stmt(m_db, "SELECT size, completed, rowid FROM entry WHERE aidx = ? AND idx = ?");
	stmt.bind(1, aidx).bind(2, idx)();
	if(stmt.get_int(1) == GetCurrentThreadId()) {
		int old_size = stmt.get_int(0);
		FILE *fp = fopen(GetFileName(stmt.get_int64(2)).c_str(), "a+b");
		fwrite(data, size, 1, fp);
		fclose(fp);

		stmt.reprepare(m_db, "UPDATE entry SET size = ? WHERE aidx = ? AND idx = ?")
		    .bind(1, old_size + size).bind(2, aidx).bind(3, idx)();
	}
}

void SolidCacheDisk::AddEntry_(unsigned int aidx, unsigned int idx, const void *data, unsigned int size)
{
	Statement(m_db, "INSERT INTO entry (aidx, idx, size, completed) VALUES (?, ?, ?, ?)")
	    .bind(1, aidx).bind(2, idx).bind(3, size).bind(4, static_cast<int>(GetCurrentThreadId()))();
	Statement stmt(m_db, "SELECT size, completed, rowid FROM entry WHERE aidx = ? AND idx = ?");
	stmt.bind(1, aidx).bind(2, idx)();
	FILE *fp = fopen(GetFileName(stmt.get_int64(2)).c_str(), "wb");
	fwrite(data, size, 1, fp);
	fclose(fp);
}

void SolidCacheDisk::Append_(const char* archive, unsigned int idx, const void* data, unsigned int size)
{
	OutputDebugPrintf("SolidCacheDisk::Append: archive %s idx %u size %u\n", archive, idx, size);
	if(!ExistsArchive_(archive)) {
		AddArchive_(archive);
	}

	unsigned int aidx = GetArchiveIdx_(archive);
	if(GetMaxDisk() >= 0 && GetSize_() + size >= GetMaxDiskInBytes()) {
//		PurgeUnmarkedOther_(aidx);
		if(GetSize_() + size >= GetMaxDiskInBytes()) {
			ReduceSize_(std::max(GetPurgeDiskInBytes(), GetSize_() + size - GetMaxDiskInBytes()), aidx);
		}
	}
	if(!ExistsArchive_(archive)) {
		AddArchive_(archive);
		aidx = GetArchiveIdx_(archive);
		OutputDebugPrintf("SolidCacheDisk::Append: Should not reach: new aidx %d\n", aidx);
	}
	if(ExistsEntry_(aidx, idx)) {
		AppendEntry_(aidx, idx, data, size);
	} else {
		AddEntry_(aidx, idx, data, size);
	}
}

void SolidCacheDisk::Cached_(const char* archive, unsigned int idx)
{
	if(IsProcessing_(archive, idx)) {
		unsigned int aidx = GetArchiveIdx_(archive); 
		Statement(m_db, "UPDATE entry SET completed = 0 WHERE idx = ? AND aidx = ?")
		    .bind(1, idx).bind(2, aidx)();
	}
}

void SolidCacheDisk::CachedVector_(const char* archive, std::vector<unsigned int>& vIndex)
{
	unsigned int aidx = GetArchiveIdx_(archive); 
	Statement stmt(m_db, "UPDATE entry SET completed = 0 WHERE idx = ? AND aidx = ?");
	stmt.bind(2, aidx);
	std::vector<unsigned int>::iterator it = vIndex.begin(), itEnd = vIndex.end();
	for(; it != itEnd; ++it) {
		if(IsProcessing_(archive, *it)) {
			OutputDebugPrintf("SolidCacheDisk::CachedVector %s %u\n", archive, *it);
			stmt.bind(1, *it)();
			stmt.reset();
		}
	}
}

void SolidCacheDisk::PurgeUnreferenced_()
{
	OutputDebugPrintf("SolidCacheDisk::PurgeUnreferenced()\n");
	m_db.exec("DELETE FROM archive WHERE idx NOT IN (SELECT aidx FROM entry)");
}

void SolidCacheDisk::PurgeUnreferencedOther_(unsigned int aidx)
{
	OutputDebugPrintf("SolidCacheDisk::PurgeUnreferencedOther(): aidx %u\n", aidx);
	Statement(m_db, "DELETE FROM archive WHERE idx NOT IN (SELECT aidx FROM entry) AND idx <> ?").bind(1, aidx)();
}

void SolidCacheDisk::PurgeUnreferencedWithAIdx_(unsigned int aidx)
{
	OutputDebugPrintf("SolidCacheDisk::PurgeUnreferencedWithAIdx(): aidx %u\n", aidx);
	Statement(m_db, "DELETE FROM archive WHERE idx NOT IN (SELECT aidx FROM entry) AND idx = ?").bind(1, aidx)();
}

void SolidCacheDisk::PurgeUnmarked_(const char *archive)
{
	OutputDebugPrintf("SolidCacheDisk::PurgeUnmarked(): archive %s\n", archive);
	unsigned int aidx = SolidCacheDisk::GetArchiveIdx_(archive);
// TODO: transaction
// TODO: may need to restrict rows affected by current thread
	Statement stmt(m_db, "SELECT rowid FROM entry WHERE completed <> 0 AND aidx = ?");
	stmt.bind(1, aidx);
	while(stmt()) {
		OutputDebugPrintf("SolidCacheDisk::PurgeUnmarked(): DeleteFile %s\n", GetFileName(stmt.get_int64(0)).c_str());
		DeleteFile(GetFileName(stmt.get_int64(0)).c_str());
	}
	Statement(m_db, "DELETE FROM entry WHERE completed <> 0 AND aidx = ?")
	    .bind(1, aidx)();
	PurgeUnreferencedWithAIdx_(aidx);
}

void SolidCacheDisk::PurgeUnmarkedAll_()
{
// TODO: transaction
	Statement stmt(m_db, "SELECT rowid FROM entry WHERE completed <> 0");
	while(stmt()) {
		OutputDebugPrintf("SolidCacheDisk::PurgeUnmarkedAll(): DeleteFile %s\n", GetFileName(stmt.get_int64(0)).c_str());
		DeleteFile(GetFileName(stmt.get_int64(0)).c_str());
	}
	m_db.exec("DELETE FROM entry WHERE completed <> 0");
	PurgeUnreferenced_();
}

void SolidCacheDisk::PurgeUnmarkedOther_(unsigned int aidx)
{
// TODO: transaction
	Statement stmt(m_db, "SELECT rowid FROM entry WHERE aidx <> ? AND completed <> 0");
	stmt.bind(1,aidx);
	while(stmt()) {
		OutputDebugPrintf("SolidCacheDisk::PurgeUnmarkedOther(): DeleteFile %s\n", GetFileName(stmt.get_int64(0)).c_str());
		DeleteFile(GetFileName(stmt.get_int64(0)).c_str());
	}
	Statement(m_db, "DELETE FROM entry WHERE aidx <> ? AND completed <> 0").bind(1,aidx)();
	PurgeUnreferencedOther_(aidx);
}

boost::uint64_t SolidCacheDisk::GetSize_() const
{
	Statement stmt(m_db, "select sum(size) from entry");
	stmt();
	return stmt.get_int64(0);
}

void SolidCacheDisk::ReduceSizeWithAIdx_(unsigned int aidx, boost::uint64_t size)
{
// TODO: transaction
	OutputDebugPrintf("SolidCacheDisk::ReduceSizeWithAidx: aidx %u size %" UINT64_S "u\n", aidx, size);
	boost::uint64_t total = 0;
	int idx = -1;
	Statement stmt(m_db, "SELECT idx, size, rowid FROM entry WHERE aidx = ? AND completed = 0 ORDER BY idx");
	stmt.bind(1, aidx);
	while(stmt()) {
		idx = stmt.get_int(0);
		total += stmt.get_int64(1);
		OutputDebugPrintf("SolidCacheDisk::ReduceSizeWithAIdx(): DeleteFile %s\n", GetFileName(stmt.get_int64(2)).c_str());
		DeleteFile(GetFileName(stmt.get_int64(2)).c_str());
		if(total >= size) break;
	}

	stmt.reprepare(m_db, "DELETE FROM entry WHERE completed = 0 AND aidx = ? AND idx <= ?");
	stmt.bind(1, aidx).bind(2, idx)();
	PurgeUnreferencedWithAIdx_(aidx);
}

void SolidCacheDisk::ReduceSize_(boost::uint64_t size, unsigned int exclude_aidx)
{
	OutputDebugPrintf("SolidCacheDisk::ReduceSize: size %" UINT64_S "u exclude_aidx %u\n", size, exclude_aidx);
	boost::uint64_t cur_size = GetSize_();
	while(size > 0 && cur_size != 0) {
		Statement stmt(m_db, "SELECT archive.idx FROM archive, entry WHERE archive.idx <> ? AND archive.idx = entry.aidx AND entry.completed = 0 GROUP BY archive.idx HAVING count(entry.idx) > 0 ORDER BY atime LIMIT 1");
		stmt.bind(1, exclude_aidx);
		if(stmt()) {
			unsigned int aidx = stmt.get_int(0);
			ReduceSizeWithAIdx_(aidx, size);
			boost::uint64_t new_size = GetSize_();
			OutputDebugPrintf("SolidCacheDisk::ReduceSize: size %" UINT64_S "u cur_size %" UINT64_S "u new_size %" UINT64_S "u\n", size, cur_size, new_size);
			if(cur_size - new_size < size) size -= (cur_size - new_size);
			else size = 0;
			cur_size = new_size;
		} else break;
	}
}

void SolidCacheDisk::AccessArchive_(const char* archive)
{
	OutputDebugPrintf("SolidCacheDisk::AccessArchive(): archive: %s\n", archive);
	unsigned int aidx = GetArchiveIdx_(archive); 
	std::time_t atime;
	std::time(&atime);
	Statement(m_db, "UPDATE archive SET atime = ? WHERE idx = ?")
	    .bind(1, (int)atime).bind(2, aidx)();
}

bool SolidCacheDisk::IsCached_(const char* archive, unsigned int idx) const
{
	OutputDebugPrintf("SolidCacheDisk::IsCached(): archive: %s idx %d\n", archive, idx);
	if(!ExistsArchive_(archive)) return false;
	unsigned int aidx = GetArchiveIdx_(archive); 
	Statement stmt(m_db, "SELECT COUNT(*) FROM entry WHERE aidx = ? AND idx = ? AND completed = 0");
	stmt.bind(1, aidx).bind(2, idx);
	return stmt() && stmt.get_int(0) > 0;
}

bool SolidCacheDisk::Exists_(const char* archive, unsigned int idx) const
{
	OutputDebugPrintf("SolidCacheDisk::Exists(): archive: %s idx %d\n", archive, idx);
	if(!ExistsArchive_(archive)) return false;
	unsigned int aidx = GetArchiveIdx_(archive); 
	Statement stmt(m_db, "SELECT COUNT(*) FROM entry WHERE aidx = ? AND idx = ?");
	stmt.bind(1, aidx).bind(2, idx);
	return stmt() && stmt.get_int(0) > 0;
}

bool SolidCacheDisk::IsProcessing_(const char *archive, unsigned int idx) const
{
	OutputDebugPrintf("SolidCacheDisk::IsProcessing(): archive: %s idx %d\n", archive, idx);
	unsigned int aidx = GetArchiveIdx_(archive); 
	Statement stmt(m_db, "SELECT COUNT(*) FROM entry WHERE aidx = ? AND idx = ? AND completed <> 0");
	stmt.bind(1, aidx).bind(2, idx);
	return stmt() && stmt.get_int(0) > 0;
}

class MMap
{
private:
	HANDLE hFile;
	HANDLE hView;
	LPVOID pMap;
public:
	MMap(const std::string& name, unsigned int size)
	{
		hFile = CreateFile(name.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
		hView = CreateFileMapping(hFile, NULL, PAGE_READONLY, 0, 0, NULL);
		pMap = MapViewOfFile(hView, FILE_MAP_READ, 0, 0, size);
//		OutputDebugPrintf("hFile %08X hView %08X pMap %08X", hFile, hView, pMap);
	}
	operator LPVOID() { return pMap; }
	~MMap()
	{
		UnmapViewOfFile(pMap);
		CloseHandle(hView);
		CloseHandle(hFile);
	}
};

void SolidCacheDisk::GetContent_(const char *archive, unsigned int index, void* dest, unsigned int size) const
{
	unsigned int aidx = GetArchiveIdx_(archive); 
	Statement stmt(m_db, "SELECT rowid, size FROM entry WHERE aidx = ? AND idx = ?");
	stmt.bind(1, aidx).bind(2, index)();
	unsigned int uiSize = std::min<unsigned int>(size, stmt.get_int(1));
	MMap mm(GetFileName(stmt.get_int64(0)), uiSize);
	CopyMemory(dest, mm, uiSize);
	OutputDebugPrintf("SolidCacheDisk::GetContent: %s %u %p reqsize: %u dbsize: %u\n", archive, index, dest, size, stmt.get_int(1));
}

void SolidCacheDisk::OutputContent_(const char *archive, unsigned int index, unsigned int size, FILE* fp) const
{
	unsigned int aidx = GetArchiveIdx_(archive); 
	Statement stmt(m_db, "SELECT rowid, size FROM entry WHERE aidx = ? AND idx = ?");
	stmt.bind(1, aidx).bind(2, index)();
	unsigned int uiSize = std::min<unsigned int>(size, stmt.get_int(1));
	MMap mm(GetFileName(stmt.get_int64(0)), uiSize);
	fwrite(mm, uiSize, 1, fp);
	OutputDebugPrintf("SolidCacheDisk::OutputContent: %s %u %p reqsize: %u dbsize: %u\n", archive, index, fp, size, stmt.get_int(1));
}

SolidCacheDisk& SolidCacheDisk::GetInstance()
{
	static SolidCacheDisk scSingleton;
	return scSingleton;
}

SolidFileCacheDisk SolidCacheDisk::GetFileCache(const std::string& filename)
{
	return SolidFileCacheDisk(GetInstance(), filename);
}

void SolidCacheDisk::Clear_()
{
	std::string sDB = GetCacheFolder() + "ax7z_s.db";
	std::string s;

	m_db.close();
	DeleteFile(sDB.c_str());
	s = GetCacheFolder() + "ax7z_s????????????????.tmp";
	WIN32_FIND_DATA find;
	HANDLE hFind;
	hFind = FindFirstFile(s.c_str(), &find);
	if(hFind != INVALID_HANDLE_VALUE) {
		do {
			s = GetCacheFolder() + find.cFileName;
			DeleteFile(s.c_str());
		} while(FindNextFile(hFind, &find));
		CloseHandle(hFind);
	}
	if(!OpenDB_()) {
		OutputDebugPrintf("SolidCache::Clear: sqlite3_open for %s failed by %s\n", sDB.c_str(), sqlite3_errmsg(m_db));
	}
	InitDB_();
}

std::string SolidCacheDisk::SetCacheFolder(std::string sNew)
{
	boost::shared_lock<SolidCache::Mutex> guard(SolidCache::GetMutex());

	{
		const char* temp = sNew.c_str();
		const char* temp2 = temp + sNew.size();
		if(*CharPrev(temp, temp2) != '\\') sNew += '\\';
	}
	if(sNew != m_sCacheFolder) {
	    m_db.close();
		std::string sNewDB = sNew + "ax7z_s.db";
		if(m_sCacheFolder != "") {
			std::string sOldDB = m_sCacheFolder + "ax7z_s.db";
			MoveFileEx(sOldDB.c_str(), sNewDB.c_str(), MOVEFILE_COPY_ALLOWED | MOVEFILE_REPLACE_EXISTING);
			std::string s = m_sCacheFolder + "ax7z_s????????????????.tmp";
			WIN32_FIND_DATA find;
			HANDLE hFind;
			hFind = FindFirstFile(s.c_str(), &find);
			if(hFind != INVALID_HANDLE_VALUE) {
				do {
					s = m_sCacheFolder + find.cFileName;
					std::string sNewData = sNew + find.cFileName;
					MoveFileEx(s.c_str(), sNewData.c_str(), MOVEFILE_COPY_ALLOWED | MOVEFILE_REPLACE_EXISTING);
				} while(FindNextFile(hFind, &find));
				CloseHandle(hFind);
			}
		}
		std::swap(sNew, m_sCacheFolder);
		if(!OpenDB_()) {
			OutputDebugPrintf("SetCacheFolder: sqlite3_open for %s failed by %s\n", sNewDB.c_str(), sqlite3_errmsg(m_db));
		}
		InitDB_();
		CheckDB_();
	}
	return sNew;
}

void SolidCacheDisk::ReduceSizeCallback(void *pArg, const std::string& sArchive, unsigned int index, void* data, unsigned int size, bool flag)
{
	SolidCacheDisk *pscd = static_cast<SolidCacheDisk*>(pArg);
	assert(!pscd->Exists_(sArchive.c_str(), index));
	pscd->Append_(sArchive.c_str(), index, data, size);
	if(flag) pscd->Cached_(sArchive.c_str(), index);
}
