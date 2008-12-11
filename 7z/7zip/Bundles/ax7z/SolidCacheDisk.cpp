#include <iostream>
#define NOMINMAX
#include <windows.h>
#undef NOMINMAX
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

void SolidCacheDisk::InitDB()
{
	sqlite3_exec(m_db, "pragma journal_mode = off", NULL, NULL, NULL);
	sqlite3_exec(m_db, "PRAGMA synchronous = OFF", NULL, NULL, NULL); // very effective
	sqlite3_exec(m_db, "PRAGMA temp_store = MEMORY", NULL, NULL, NULL);
	Statement stmt(m_db, "pragma user_version");
	if(stmt() && stmt.get_int(0) > 0) {
		switch(stmt.get_int(0)) {
		case 1:
			OutputDebugPrintf("SolidCacheDisk::InitDB: version 1");
			break;
		default:
			OutputDebugPrintf("SolidCacheDisk::InitDB: Unknown version %d", stmt.get_int(0));
			break;
		}
	} else {
		OutputDebugPrintf("SolidCacheDisk::InitDB: Create version 1");
		sqlite3_exec(m_db, "pragma user_version = 1", NULL, NULL, NULL);
		sqlite3_exec(m_db, "create table archive (idx INTEGER PRIMARY KEY AUTOINCREMENT, path TEXT, atime INTEGER)", NULL, NULL, NULL);
		sqlite3_exec(m_db, "create table entry (aidx INTERGER, idx INTEGER, size INTEGER, completed INTEGER, constraint pkey PRIMARY KEY (aidx, idx) )", NULL, NULL, NULL);
	}
}

std::string SolidCacheDisk::GetFileName(__int64 id) const
{
	char buf[32768];
	wsprintf(buf, "%s%08X%08X.tmp", m_sCacheFolder.c_str(), static_cast<unsigned int>(id >> 32), static_cast<unsigned int>(id & 0xFFFFFFFF));
	return std::string(buf);
}

bool SolidCacheDisk::ExistsArchive(const char* archive)
{
	Statement stmt(m_db, "select count(*) from archive where path = ?");
	stmt.bind(1, archive);
	return stmt() && stmt.get_int(0) > 0;
}

void SolidCacheDisk::AddArchive(const char* archive)
{
	Statement(m_db, "insert into archive (path, atime) values (?, 0)").bind(1, archive)();
}

int SolidCacheDisk::GetArchiveIdx(const char* archive)
{
	Statement stmt(m_db, "select idx from archive where path = ?");
	stmt.bind(1, archive)();
	return stmt.get_int(0);
}

bool SolidCacheDisk::ExistsEntry(int aidx, int idx)
{
	Statement stmt(m_db, "select count(*) from entry where aidx = ? and idx = ?");
	stmt.bind(1, aidx).bind(2, idx);
	return stmt() && stmt.get_int(0) > 0;
}

void SolidCacheDisk::AppendEntry(int aidx, int idx, const void* data, int size)
{
// TODO: transaction
	Statement stmt(m_db, "select size, completed, rowid from entry where aidx = ? and idx = ?");
	stmt.bind(1, aidx).bind(2, idx)();
	if(stmt.get_int(1) == 0) {
		int old_size = stmt.get_int(0);
		FILE *fp = fopen(GetFileName(stmt.get_int64(2)).c_str(), "a+b");
		fwrite(data, size, 1, fp);
		fclose(fp);

		stmt.reprepare(m_db, "update entry set size = ? where aidx = ? and idx = ?")
		    .bind(1, old_size + size).bind(2, aidx).bind(3, idx)();
	}
}

void SolidCacheDisk::AddEntry(int aidx, int idx, const void *data, int size)
{
// TODO: transaction
	Statement(m_db, "insert into entry (aidx, idx, size, completed) values (?, ?, ?, 0)")
	    .bind(1, aidx).bind(2, idx).bind(3, size)();
	Statement stmt(m_db, "select size, completed, rowid from entry where aidx = ? and idx = ?");
	stmt.bind(1, aidx).bind(2, idx)();
	FILE *fp = fopen(GetFileName(stmt.get_int64(2)).c_str(), "wb");
	fwrite(data, size, 1, fp);
	fclose(fp);
}

void SolidCacheDisk::Append(const char* archive, unsigned int idx, const void* data, unsigned int size)
{
	OutputDebugPrintf("SolidCacheDisk::Append: archive %s idx %d size %d", archive, idx, size);
	if(!ExistsArchive(archive)) {
		AddArchive(archive);
	}

	int aidx = GetArchiveIdx(archive);
	if(m_nMaxDisk >= 0 && (GetSize() + size)/1024/1024 >= m_nMaxDisk) {
// TODO: enable configuration for delete size at a time
		PurgeUnmarkedOther(aidx);
		if(m_nMaxDisk >= 0 && (GetSize() + size)/1024/1024 >= m_nMaxDisk) {
			ReduceSize(std::max(10U * 1024 * 1024, GetSize() + size - m_nMaxDisk * 1024 * 1024), aidx);
		}
	}
	if(ExistsEntry(aidx, idx)) {
		AppendEntry(aidx, idx, data, size);
	} else {
		AddEntry(aidx, idx, data, size);
	}
}

void SolidCacheDisk::Cached(const char* archive, unsigned int idx)
{
	if(!IsCached(archive, idx)) {
		Statement(m_db, "update entry set completed = 1 where idx = ? and aidx = (select idx from archive where path = ?)")
		    .bind(1, idx).bind(2, archive)();
	}
}

void SolidCacheDisk::CachedVector(const char* archive, std::vector<unsigned int>& vIndex)
{
	int aidx = GetArchiveIdx(archive); 
	Statement stmt(m_db, "update entry set completed = 1 where idx = ? and aidx = ?");
	stmt.bind(2, aidx);
	std::vector<unsigned int>::iterator it = vIndex.begin(), itEnd = vIndex.end();
	for(; it != itEnd; ++it) {
		if(!IsCached(archive, *it)) {
			OutputDebugPrintf("SolidCacheDisk::CachedVector %s %d", archive, *it);
			stmt.bind(1, *it)();
			stmt.reset();
		}
	}
}

void SolidCacheDisk::PurgeUnreferenced()
{
	sqlite3_exec(m_db, "delete from archive where idx not in (select aidx from entry)", NULL, NULL, NULL);
}

void SolidCacheDisk::PurgeUnmarked(const char *archive)
{
// TODO: transaction
	Statement stmt(m_db, "select rowid from entry where completed = 0 and aidx = (select idx from archive where path = ?)");
	stmt.bind(1, archive);
	while(stmt()) {
		DeleteFile(GetFileName(stmt.get_int64(0)).c_str());
	}
	Statement(m_db, "delete from entry where completed = 0 and aidx = (select idx from archive where path = ?)")
	    .bind(1, archive)();
	PurgeUnreferenced();
}

void SolidCacheDisk::PurgeUnmarkedAll()
{
// TODO: transaction
	Statement stmt(m_db, "select rowid from entry where completed = 0");
	while(stmt()) {
		DeleteFile(GetFileName(stmt.get_int64(0)).c_str());
	}
	sqlite3_exec(m_db, "delete from entry where completed = 0", NULL, NULL, NULL);
	PurgeUnreferenced();
}

void SolidCacheDisk::PurgeUnmarkedOther(int aidx)
{
// TODO: transaction
	Statement stmt(m_db, "select rowid from entry where aidx <> ? and completed = 0");
	stmt.bind(1,aidx);
	while(stmt()) {
		DeleteFile(GetFileName(stmt.get_int64(0)).c_str());
	}
	Statement(m_db, "delete from entry where aidx <> ? and completed = 0").bind(1,aidx)();
	PurgeUnreferenced();
}

int SolidCacheDisk::GetSize() const
{
	Statement stmt(m_db, "select sum(size) from entry");
	stmt();
	return stmt.get_int(0);
}

void SolidCacheDisk::ReduceSizeWithAIdx(int aidx, int size)
{
// TODO: transaction
	OutputDebugPrintf("ReduceSizeWithAidx: aidx %d size %d", aidx, size);
	unsigned int total = 0;
	int idx = -1;
	Statement stmt(m_db, "select idx, size, rowid from entry where aidx = ? and completed = 1 order by idx");
	stmt.bind(1, aidx);
	while(stmt()) {
		idx = stmt.get_int(0);
		total += stmt.get_int(1);
		DeleteFile(GetFileName(stmt.get_int64(2)).c_str());
		if(total >= size) break;
	}

	stmt.reprepare(m_db, "delete from entry where completed = 1 and aidx = ? and idx <= ?");
	stmt.bind(1, aidx).bind(2, idx)();
	PurgeUnreferenced();
}

void SolidCacheDisk::ReduceSize(int size, int exclude_aidx)
{
	OutputDebugPrintf("ReduceSize: size %d exclude_aidx %d", size, exclude_aidx);
	int cur_size = GetSize();
	while(size > 0 && cur_size != 0) {
		Statement stmt(m_db, "select archive.idx from archive, entry where archive.idx <> ? and archive.idx = entry.aidx and entry.completed = 1 group by archive.idx having count(entry.idx) > 0 order by atime limit 1");
		stmt.bind(1, exclude_aidx);
		if(stmt()) {
			int aidx = stmt.get_int(0);
			ReduceSizeWithAIdx(aidx, size);
			int new_size = GetSize();
			size -= (cur_size - new_size);
			cur_size = new_size;
		} else break;
	}
}

void SolidCacheDisk::AccessArchive(const char* archive)
{
	std::time_t atime;
	std::time(&atime);
	Statement(m_db, "update archive set atime = ? where path = ?")
	    .bind(1, (int)atime).bind(2, archive)();
}

bool SolidCacheDisk::IsCached(const char* archive, unsigned int idx) const
{
	Statement stmt(m_db, "select count(*) from archive, entry where entry.aidx = archive.idx and archive.path = ? and entry.idx = ? and completed = 1");
	stmt.bind(1, archive).bind(2, idx);
	return stmt() && stmt.get_int(0) > 0;
}

bool SolidCacheDisk::Exists(const char* archive, unsigned int idx) const
{
	Statement stmt(m_db, "select count(*) from archive, entry where archive.idx = entry.aidx and archive.path = ? and entry.idx = ?");
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

void SolidCacheDisk::GetContent(const char *archive, unsigned int index, void* dest, unsigned int size) const
{
	Statement stmt(m_db, "select entry.rowid, entry.size from archive, entry where archive.idx = entry.aidx and archive.path = ? and entry.idx = ?");
	stmt.bind(1, archive).bind(2, index)();
	unsigned int uiSize = std::min<unsigned int>(size, stmt.get_int(1));
	MMap mm(GetFileName(stmt.get_int64(0)), uiSize);
	CopyMemory(dest, mm, uiSize);
	OutputDebugPrintf("SolidCacheDisk::GetContent: %s %u %p %d %d", archive, index, dest, size, stmt.get_int(1));
}

void SolidCacheDisk::OutputContent(const char *archive, unsigned int index, unsigned int size, FILE* fp) const
{
	Statement stmt(m_db, "select entry.rowid, entry.size from archive, entry where archive.idx = entry.aidx and archive.path = ? and entry.idx = ?");
	stmt.bind(1, archive).bind(2, index)();
	unsigned int uiSize = std::min<unsigned int>(size, stmt.get_int(1));
	MMap mm(GetFileName(stmt.get_int64(0)), uiSize);
	OutputDebugPrintf("SolidCacheDisk::OutputContent:4 %p", mm);
	fwrite(mm, uiSize, 1, fp);
	OutputDebugPrintf("SolidCacheDisk::OutputContent: %s %u %p %d %d", archive, index, fp, size, stmt.get_int(1));
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

std::string SolidCacheDisk::SetCacheFolder(std::string sNew)
{
	if(sNew != m_sCacheFolder) {
	    sqlite3_close(m_db);
	    std::string sOldDB = m_sCacheFolder + "ax7z.db";
	    std::string sNewDB = sNew + "ax7z.db";
	    MoveFileEx(sOldDB.c_str(), sNewDB.c_str(), MOVEFILE_COPY_ALLOWED | MOVEFILE_REPLACE_EXISTING);
		std::swap(sNew, m_sCacheFolder);
		if(sqlite3_open(sNewDB.c_str(), &m_db) != SQLITE_OK) {
			OutputDebugPrintf("SetCacheFolder: sqlite3_open for %s failed by %s", sNewDB.c_str(), sqlite3_errmsg(m_db));
		}
		InitDB();
	}
	return sNew;
}
