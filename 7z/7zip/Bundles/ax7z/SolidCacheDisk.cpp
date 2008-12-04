#include <iostream>
#include <ctime>
#include "sqlite3/sqlite3.h"
#include "sqlite3/sqlite3helper.h"
#include "SolidCache.h"

#if 0
[append]
if(cur_in_disk) {
    append_to_disk();
} else {
    if(cur_size + size > MaxMemory) {
        purge_to_file();
        append_to_disk();
    } else {
        append_to_memory();
        if(GetMemoryUsage()+ size > MaxMemory)
            purge_memory();
    }
}

[mark]
if(cur_in_disk) {
    mark_disk();
} else if(cur_in_memory) {
    mark_memory();
}

[isCached]
cur_in_disk && marked || cur_in_memory && marked
#endif

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
// TODO: version checking
	sqlite3_exec(m_db, "create table archive (idx INTEGER PRIMARY KEY AUTOINCREMENT, path TEXT, atime INTEGER)", NULL, NULL, NULL);
	sqlite3_exec(m_db, "create table entry (aidx INTERGER, idx INTEGER, data BLOB, completed INTEGER, constraint pkey PRIMARY KEY (aidx, idx) )", NULL, NULL, NULL);
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
	Statement stmt(m_db, "select data, completed from entry where aidx = ? and idx = ?");
	stmt.bind(1, aidx).bind(2, idx)();
	if(stmt.get_int(1) == 0) {
		int old_size = stmt.get_bytes(0);
		char *store = static_cast<char*>(sqlite3_malloc(old_size + size));
		memcpy(store, stmt.get_blob(0), old_size);
		memcpy(store + old_size, data, size);

		stmt.reprepare(m_db, "update entry set data = ? where aidx = ? and idx = ?")
		    .bind(1, store, old_size + size, sqlite3_free).bind(2, aidx).bind(3, idx)();
	}
}

void SolidCacheDisk::AddEntry(int aidx, int idx, const void *data, int size)
{
	Statement(m_db, "insert into entry (aidx, idx, data, completed) values (?, ?, ?, 0)")
	    .bind(1, aidx).bind(2, idx).bind(3, data, size)();
}

void SolidCacheDisk::Append(const char* archive, unsigned int idx, const void* data, unsigned int size)
{
	OutputDebugPrintf("Append: archive %s idx %d size %d", archive, idx, size);
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
	Statement(m_db, "update entry set completed = 1 where idx = ? and aidx = (select idx from archive where path = ?)")
	    .bind(1, idx).bind(2, archive)();
}

void SolidCacheDisk::PurgeUnreferenced()
{
	sqlite3_exec(m_db, "delete from archive where idx not in (select aidx from entry)", NULL, NULL, NULL);
}

void SolidCacheDisk::PurgeUnmarked(const char *archive)
{
	Statement(m_db, "delete from entry where completed = 0 and aidx = (select idx from archive where path = ?)")
	    .bind(1, archive)();
	PurgeUnreferenced();
}

void SolidCacheDisk::PurgeUnmarkedAll()
{
	sqlite3_exec(m_db, "delete from entry where completed = 0", NULL, NULL, NULL);
	PurgeUnreferenced();
}

void SolidCacheDisk::PurgeUnmarkedOther(int aidx)
{
	Statement(m_db, "delete from entry where aidx <> ? and completed = 0").bind(1,aidx)();
	PurgeUnreferenced();
}

int SolidCacheDisk::GetSize() const
{
	Statement stmt(m_db, "select sum(length(data)) from entry");
	stmt();
	return stmt.get_int(0);
}

void SolidCacheDisk::ReduceSizeWithArchive(const char* archive, int size)
{
	Statement stmt(m_db,
	    "delete from entry where aidx = (select idx from archive where path = ?) and "
	        "idx <= (select idx from "
	            "(select t1.*,sum(length(t2.data)) cutsize from entry t1, entry t2 "
	                "where t1.aidx = "
	                    "(select idx from archive where path = ?1) "
	                    "and t1.completed = 1 and t2.completed = 1 "
	                    "and t2.aidx = t1.aidx and t2.idx <= t1.idx group by t1.idx) "
	            "order by cutsize - ?2 < 0, abs(cutsize - ?2) limit 1)");
	stmt.bind(1, archive).bind(2, size)();
	PurgeUnreferenced();
}

void SolidCacheDisk::ReduceSizeWithAIdx(int aidx, int size)
{
	OutputDebugPrintf("ReduceSizeWithAidx: aidx %d size %d", aidx, size);
	unsigned int total = 0;
	int idx = -1;
	Statement stmt(m_db, "select idx, length(data) from entry where aidx = ? and completed = 1 order by idx");
	stmt.bind(1, aidx);
	while(stmt()) {
		idx = stmt.get_int(0);
		total += stmt.get_int(1);
		if(total >= size) break;
	}
	stmt.reprepare(m_db, "delete from entry where completed = 1 and idx <= ?");
	stmt.bind(1, idx)();
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

void SolidCacheDisk::GetContent(const char *archive, unsigned int index, void* dest, unsigned int size) const
{
	Statement stmt(m_db, "select data from archive, entry where archive.idx = entry.aidx and archive.path = ? and entry.idx = ?");
	stmt.bind(1, archive).bind(2, index)();
	CopyMemory(dest, stmt.get_blob(0), std::min<unsigned int>(size, stmt.get_bytes(0)));
}

void SolidCacheDisk::OutputContent(const char *archive, unsigned int index, unsigned int size, FILE* fp) const
{
	Statement stmt(m_db, "select data from archive, entry where archive.idx = entry.aidx and archive.path = ? and entry.idx = ?");
	stmt.bind(1, archive).bind(2, index)();
	fwrite(stmt.get_blob(0), std::min<unsigned int>(size, stmt.get_bytes(0)), 1, fp);
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
