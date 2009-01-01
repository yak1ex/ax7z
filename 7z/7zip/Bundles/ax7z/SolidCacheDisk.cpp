#include <iostream>
#define NOMINMAX
#include <windows.h>
#undef NOMINMAX
#include <sys/types.h>
#include <sys/stat.h>
#include "sqlite3/sqlite3.h"
#include "sqlite3/sqlite3helper.h"
#include "SolidCache.h"

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
	sqlite3_close(m_db);
}

void SolidCacheDisk::InitDB_()
{
	sqlite3_exec(m_db, "PRAGMA journal_mode = OFF", NULL, NULL, NULL);
	sqlite3_exec(m_db, "PRAGMA synchronous = OFF", NULL, NULL, NULL); // very effective
	sqlite3_exec(m_db, "PRAGMA temp_store = MEMORY", NULL, NULL, NULL);
	Statement stmt(m_db, "pragma user_version");
	if(stmt() && stmt.get_int(0) > 0) {
		switch(stmt.get_int(0)) {
		case 1:
			OutputDebugPrintf("SolidCacheDisk::InitDB: version 1 exists, need to update");
			sqlite3_exec(m_db, "ALTER TABLE archive ADD COLUMN mtime INTEGER", NULL, NULL, NULL);
			sqlite3_exec(m_db, "ALTER TABLE archive ADD COLUMN size INTEGER", NULL, NULL, NULL);
			{
				Statement stmt2(m_db, "SELECT rowid FROM entry WHERE completed <> 1");
				while(stmt2()) {
					OutputDebugPrintf("SolidCacheDisk::InitDB(): DeleteFile %s", GetFileName(stmt2.get_int64(0)).c_str());
					DeleteFile(GetFileName(stmt2.get_int64(0)).c_str());
				}
			}
			sqlite3_exec(m_db, "DELETE FROM entry WHERE completed <> 1", NULL, NULL, NULL);
			sqlite3_exec(m_db, "UPDATE entry SET completed = 0", NULL, NULL, NULL);
			sqlite3_exec(m_db, "PRAGMA user_version = 2", NULL, NULL, NULL);
			break;
		case 2:
			OutputDebugPrintf("SolidCacheDisk::InitDB: version 2 exists, skip");
			break;
		default:
			OutputDebugPrintf("SolidCacheDisk::InitDB: Unknown version %d", stmt.get_int(0));
			break;
		}
	} else {
		OutputDebugPrintf("SolidCacheDisk::InitDB: Create version 2");
		sqlite3_exec(m_db, "PRAGMA user_version = 2", NULL, NULL, NULL);
		sqlite3_exec(m_db, "CREATE TABLE archive (idx INTEGER PRIMARY KEY AUTOINCREMENT, path TEXT, atime INTEGER, mtime INTEGER, size INTEGER)", NULL, NULL, NULL);
		sqlite3_exec(m_db, "CREATE TABLE entry (aidx INTERGER, idx INTEGER, size INTEGER, completed INTEGER, CONSTRAINT pkey PRIMARY KEY (aidx, idx) )", NULL, NULL, NULL);
	}
}

void SolidCacheDisk::CheckDB_()
{
	Statement stmt(m_db, "SELECT idx, path, mtime, size FROM archive");
	while(stmt()) {
		if(stmt.get_int64(2) == 0 && stmt.get_int64(3) == 0) continue;
		__stat64 st;
		if(!_stat64(stmt.get_text(1), &st) && st.st_mtime == stmt.get_int64(2) && st.st_size == stmt.get_int64(3))
			continue;
		Statement stmt2(m_db, "SELECT rowid FROM entry WHERE aidx = ?");
		stmt2.bind(1, stmt.get_int(0));
		while(stmt2()) {
			OutputDebugPrintf("SolidCacheDisk::CheckDB(): DeleteFile %s", GetFileName(stmt2.get_int64(0)).c_str());
			DeleteFile(GetFileName(stmt2.get_int64(0)).c_str());
		}
		Statement(m_db, "DELETE FROM entry WHERE aidx = ?").bind(1, stmt.get_int(0))();
		OutputDebugPrintf("SolidCacheDisk::CheckDB invalid %s %u actualtime %" UINT64_S "u dbmtime %" UINT64_S "u actualsize %" UINT64_S "u dbsize %" UINT64_S "u", stmt.get_text(1), stmt.get_int(0), st.st_mtime, stmt.get_int64(2), st.st_size, stmt.get_int64(3));
	}
	PurgeUnreferenced_();
}

std::string SolidCacheDisk::GetFileName(__int64 id) const
{
	char buf[32768];
	wsprintf(buf, "%sax7z%08X%08X.tmp", m_sCacheFolder.c_str(), static_cast<unsigned int>(id >> 32), static_cast<unsigned int>(id & 0xFFFFFFFF));
	return std::string(buf);
}

bool SolidCacheDisk::ExistsArchive_(const char* archive)
{
	Statement stmt(m_db, "SELECT COUNT(*) FROM archive WHERE path = ?");
	stmt.bind(1, archive);
	return stmt() && stmt.get_int(0) > 0;
}

void SolidCacheDisk::AddArchive_(const char* archive)
{
	__stat64 st;
// TODO: error check
	_stat64(archive, &st);
	Statement(m_db, "INSERT INTO archive (path, atime, mtime, size) VALUES (?, 0, ?, ?)")
		.bind(1, archive).bind(2, st.st_mtime).bind(3, st.st_size)();
	OutputDebugPrintf("SolidCacheDisk::AddArchive archive %s mtime %" UINT64_S "u size %" UINT64_S "u", archive, st.st_mtime, st.st_size);
}

unsigned int SolidCacheDisk::GetArchiveIdx_(const char* archive)
{
	Statement stmt(m_db, "SELECT idx FROM archive WHERE path = ?");
	stmt.bind(1, archive)();
	return stmt.get_int(0);
}

bool SolidCacheDisk::ExistsEntry_(unsigned int aidx, unsigned int idx)
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
	OutputDebugPrintf("SolidCacheDisk::Append: archive %s idx %u size %u", archive, idx, size);
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
		OutputDebugPrintf("SolidCacheDisk::Append: Should not reach: new aidx %d", aidx);
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
		Statement(m_db, "UPDATE entry SET completed = 0 WHERE idx = ? AND aidx = (SELECT idx FROM archive WHERE path = ?)")
		    .bind(1, idx).bind(2, archive)();
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
			OutputDebugPrintf("SolidCacheDisk::CachedVector %s %u", archive, *it);
			stmt.bind(1, *it)();
			stmt.reset();
		}
	}
}

void SolidCacheDisk::PurgeUnreferenced_()
{
	OutputDebugPrintf("SolidCacheDisk::PurgeUnreferenced()");
	sqlite3_exec(m_db, "DELETE FROM archive WHERE idx NOT IN (SELECT aidx FROM entry)", NULL, NULL, NULL);
}

void SolidCacheDisk::PurgeUnreferencedOther_(unsigned int aidx)
{
	OutputDebugPrintf("SolidCacheDisk::PurgeUnreferencedOther(): aidx %u", aidx);
	Statement(m_db, "DELETE FROM archive WHERE idx NOT IN (SELECT aidx FROM entry) AND idx <> ?").bind(1, aidx)();
}

void SolidCacheDisk::PurgeUnreferencedWithAIdx_(unsigned int aidx)
{
	OutputDebugPrintf("SolidCacheDisk::PurgeUnreferencedWithAIdx(): aidx %u", aidx);
	Statement(m_db, "DELETE FROM archive WHERE idx NOT IN (SELECT aidx FROM entry) AND idx = ?").bind(1, aidx)();
}

void SolidCacheDisk::PurgeUnmarked_(const char *archive)
{
	OutputDebugPrintf("SolidCacheDisk::PurgeUnmarked(): archive %s", archive);
	unsigned int aidx = SolidCacheDisk::GetArchiveIdx_(archive);
// TODO: transaction
// TODO: may need to restrict rows affected by current thread
	Statement stmt(m_db, "SELECT rowid FROM entry WHERE completed <> 0 AND aidx = ?");
	stmt.bind(1, aidx);
	while(stmt()) {
		OutputDebugPrintf("SolidCacheDisk::PurgeUnmarked(): DeleteFile %s", GetFileName(stmt.get_int64(0)).c_str());
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
		OutputDebugPrintf("SolidCacheDisk::PurgeUnmarkedAll(): DeleteFile %s", GetFileName(stmt.get_int64(0)).c_str());
		DeleteFile(GetFileName(stmt.get_int64(0)).c_str());
	}
	sqlite3_exec(m_db, "DELETE FROM entry WHERE completed <> 0", NULL, NULL, NULL);
	PurgeUnreferenced_();
}

void SolidCacheDisk::PurgeUnmarkedOther_(unsigned int aidx)
{
// TODO: transaction
	Statement stmt(m_db, "SELECT rowid FROM entry WHERE aidx <> ? AND completed <> 0");
	stmt.bind(1,aidx);
	while(stmt()) {
		OutputDebugPrintf("SolidCacheDisk::PurgeUnmarkedOther(): DeleteFile %s", GetFileName(stmt.get_int64(0)).c_str());
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
	OutputDebugPrintf("SolidCacheDisk::ReduceSizeWithAidx: aidx %u size %" UINT64_S "u", aidx, size);
	boost::uint64_t total = 0;
	int idx = -1;
	Statement stmt(m_db, "SELECT idx, size, rowid FROM entry WHERE aidx = ? AND completed = 0 ORDER BY idx");
	stmt.bind(1, aidx);
	while(stmt()) {
		idx = stmt.get_int(0);
		total += stmt.get_int64(1);
		OutputDebugPrintf("SolidCacheDisk::ReduceSizeWithAIdx(): DeleteFile %s", GetFileName(stmt.get_int64(2)).c_str());
		DeleteFile(GetFileName(stmt.get_int64(2)).c_str());
		if(total >= size) break;
	}

	stmt.reprepare(m_db, "DELETE FROM entry WHERE completed = 0 AND aidx = ? AND idx <= ?");
	stmt.bind(1, aidx).bind(2, idx)();
	PurgeUnreferencedWithAIdx_(aidx);
}

void SolidCacheDisk::ReduceSize_(boost::uint64_t size, unsigned int exclude_aidx)
{
	OutputDebugPrintf("SolidCacheDisk::ReduceSize: size %" UINT64_S "u exclude_aidx %u", size, exclude_aidx);
	boost::uint64_t cur_size = GetSize_();
	while(size > 0 && cur_size != 0) {
		Statement stmt(m_db, "SELECT archive.idx FROM archive, entry WHERE archive.idx <> ? AND archive.idx = entry.aidx AND entry.completed = 0 GROUP BY archive.idx HAVING count(entry.idx) > 0 ORDER BY atime LIMIT 1");
		stmt.bind(1, exclude_aidx);
		if(stmt()) {
			unsigned int aidx = stmt.get_int(0);
			ReduceSizeWithAIdx_(aidx, size);
			boost::uint64_t new_size = GetSize_();
			OutputDebugPrintf("SolidCacheDisk::ReduceSize: size %" UINT64_S "u cur_size %" UINT64_S "u new_size %" UINT64_S "u", size, cur_size, new_size);
			if(cur_size - new_size < size) size -= (cur_size - new_size);
			else size = 0;
			cur_size = new_size;
		} else break;
	}
}

void SolidCacheDisk::AccessArchive_(const char* archive)
{
	std::time_t atime;
	std::time(&atime);
	Statement(m_db, "UPDATE archive SET atime = ? WHERE path = ?")
	    .bind(1, (int)atime).bind(2, archive)();
}

bool SolidCacheDisk::IsCached_(const char* archive, unsigned int idx) const
{
	Statement stmt(m_db, "SELECT COUNT(*) FROM archive, entry WHERE entry.aidx = archive.idx AND archive.path = ? AND entry.idx = ? AND completed = 0");
	stmt.bind(1, archive).bind(2, idx);
	return stmt() && stmt.get_int(0) > 0;
}

bool SolidCacheDisk::Exists_(const char* archive, unsigned int idx) const
{
	Statement stmt(m_db, "SELECT COUNT(*) FROM archive, entry WHERE archive.idx = entry.aidx AND archive.path = ? AND entry.idx = ?");
	stmt.bind(1, archive).bind(2, idx);
	return stmt() && stmt.get_int(0) > 0;
}

bool SolidCacheDisk::IsProcessing_(const char *archive, unsigned int idx) const
{
	Statement stmt(m_db, "SELECT COUNT(*) FROM archive, entry WHERE entry.aidx = archive.idx AND archive.path = ? AND entry.idx = ? AND completed <> 0");
	stmt.bind(1, archive).bind(2, idx);
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
	Statement stmt(m_db, "SELECT entry.rowid, entry.size FROM archive, entry WHERE archive.idx = entry.aidx AND archive.path = ? AND entry.idx = ?");
	stmt.bind(1, archive).bind(2, index)();
	unsigned int uiSize = std::min<unsigned int>(size, stmt.get_int(1));
	MMap mm(GetFileName(stmt.get_int64(0)), uiSize);
	CopyMemory(dest, mm, uiSize);
	OutputDebugPrintf("SolidCacheDisk::GetContent: %s %u %p reqsize: %u dbsize: %u", archive, index, dest, size, stmt.get_int(1));
}

void SolidCacheDisk::OutputContent_(const char *archive, unsigned int index, unsigned int size, FILE* fp) const
{
	Statement stmt(m_db, "SELECT entry.rowid, entry.size FROM archive, entry WHERE archive.idx = entry.aidx AND archive.path = ? AND entry.idx = ?");
	stmt.bind(1, archive).bind(2, index)();
	unsigned int uiSize = std::min<unsigned int>(size, stmt.get_int(1));
	MMap mm(GetFileName(stmt.get_int64(0)), uiSize);
	fwrite(mm, uiSize, 1, fp);
	OutputDebugPrintf("SolidCacheDisk::OutputContent: %s %u %p reqsize: %u dbsize: %u", archive, index, fp, size, stmt.get_int(1));
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
	std::string sDB = GetCacheFolder() + "ax7z.db";
	std::string s;

	sqlite3_close(m_db);
	sDB = GetCacheFolder() + "ax7z.db";
	DeleteFile(sDB.c_str());
	s = GetCacheFolder() + "ax7z????????????????.tmp";
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
	if(sqlite3_open(sDB.c_str(), &m_db) != SQLITE_OK) {
		OutputDebugPrintf("SolidCache::Clear: sqlite3_open for %s failed by %s", sDB.c_str(), sqlite3_errmsg(m_db));
	}
	InitDB_();
}

std::string SolidCacheDisk::SetCacheFolder(std::string sNew)
{
	boost::shared_lock<SolidCache::Mutex> guard(SolidCache::GetMutex());

	if(sNew != m_sCacheFolder) {
	    sqlite3_close(m_db);
		std::string sNewDB = sNew + "ax7z.db";
		if(m_sCacheFolder != "") {
			std::string sOldDB = m_sCacheFolder + "ax7z.db";
			MoveFileEx(sOldDB.c_str(), sNewDB.c_str(), MOVEFILE_COPY_ALLOWED | MOVEFILE_REPLACE_EXISTING);
			std::string s = m_sCacheFolder + "ax7z????????????????.tmp";
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
		if(sqlite3_open(sNewDB.c_str(), &m_db) != SQLITE_OK) {
			OutputDebugPrintf("SetCacheFolder: sqlite3_open for %s failed by %s", sNewDB.c_str(), sqlite3_errmsg(m_db));
		}
		InitDB_();
		CheckDB_();
	}
	return sNew;
}
