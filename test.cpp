#include <iostream>
#include <ctime>
#include "sqlite3/sqlite3.h"

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

// Tiny wrapper to sqlite3_stmt
class Statement
{
private:
	sqlite3_stmt *m_stmt;
	Statement& operator=(const Statement&);
	Statement(const Statement&);
public:
	Statement(sqlite3 *db, const char* sql)
	{
		sqlite3_prepare_v2(db, sql, -1, &m_stmt, NULL);
	}
	Statement& reprepare(sqlite3 *db, const char* sql)
	{
		sqlite3_finalize(m_stmt);
		sqlite3_prepare_v2(db, sql, -1, &m_stmt, NULL);
		return *this;
	}
	~Statement()
	{
		sqlite3_finalize(m_stmt);
	}
	Statement& bind(int idx, int value)
	{
		sqlite3_bind_int(m_stmt, idx, value);
		return *this;
	}
	Statement& bind(int idx, const char* value)
	{
		sqlite3_bind_text(m_stmt, idx, value, -1, SQLITE_TRANSIENT);
		return *this;
	}
	Statement& bind(int idx, const void* value, int size, void (*func)(void*) = SQLITE_TRANSIENT)
	{
		sqlite3_bind_blob(m_stmt, idx, value, size, func);
		return *this;
	}
	bool operator()()
	{
// TODO: catch error on prepare()
		return sqlite3_step(m_stmt) == SQLITE_ROW;
	}
	int get_int(int idx)
	{
		return sqlite3_column_int(m_stmt, idx);
	}
	double get_double(int idx)
	{
		return sqlite3_column_double(m_stmt, idx);
	}
	const char* get_text(int idx)
	{
		return static_cast<const char*>(static_cast<const void*>(sqlite3_column_text(m_stmt, idx)));
	}
	const void* get_blob(int idx)
	{
		return sqlite3_column_blob(m_stmt, idx);
	}
	int get_bytes(int idx)
	{
		return sqlite3_column_bytes(m_stmt, idx);
	}
	const char* get_name(int idx)
	{
		return sqlite3_column_name(m_stmt, idx);
	}
	int get_type(int idx)
	{
		return sqlite3_column_type(m_stmt, idx);
	}
	int get_count()
	{
		return sqlite3_column_count(m_stmt);
	}
};

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

void init_db(sqlite3 *db)
{
	sqlite3_exec(db, "create table archive (idx INTEGER PRIMARY KEY AUTOINCREMENT, path TEXT, atime INTEGER)", NULL, NULL, NULL);
	sqlite3_exec(db, "create table entry (aidx INTERGER, idx INTEGER, data BLOB, completed INTEGER)", NULL, NULL, NULL);
}

bool exists_archive(sqlite3 *db, const char* archive)
{
	Statement stmt(db, "select count(*) from archive where path = ?");
	stmt.bind(1, archive);
	return stmt() && stmt.get_int(0) > 0;
}

void add_archive(sqlite3 *db, const char* archive)
{
	Statement(db, "insert into archive (path, atime) values (?, 0)").bind(1, archive)();
}

int get_archive_idx(sqlite3 *db, const char* archive)
{
	Statement stmt(db, "select idx from archive where path = ?");
	stmt.bind(1, archive)();
	return stmt.get_int(0);
}

bool exists_entry(sqlite3 *db, int aidx, int idx)
{
	Statement stmt(db, "select count(*) from entry where aidx = ? and idx = ?");
	stmt.bind(1, aidx).bind(2, idx);
	return stmt() && stmt.get_int(0) > 0;
}

void append_entry(sqlite3 *db, int aidx, int idx, const void* data, int size)
{
	Statement stmt(db, "select data, completed from entry where aidx = ? and idx = ?");
	stmt.bind(1, aidx).bind(2, idx)();
	if(stmt.get_int(1) == 0) {
		int old_size = stmt.get_bytes(0);
		char *store = static_cast<char*>(sqlite3_malloc(old_size + size));
		memcpy(store, stmt.get_blob(0), old_size);
		memcpy(store + old_size, data, size);

		stmt.reprepare(db, "update entry set data = ? where aidx = ? and idx = ?")
		    .bind(1, store, old_size + size, sqlite3_free).bind(2, aidx).bind(3, idx)();
	}
}

void add_entry(sqlite3 *db, int aidx, int idx, const void *data, int size)
{
	Statement(db, "insert into entry (aidx, idx, data, completed) values (?, ?, ?, 0)")
	    .bind(1, aidx).bind(2, idx).bind(3, data, size)();
}

void append(sqlite3 *db, const char* archive, int idx, const void *data, int size)
{
	if(!exists_archive(db, archive)) {
		add_archive(db, archive);
	}

	int aidx = get_archive_idx(db, archive);
	if(exists_entry(db, aidx, idx)) {
		append_entry(db, aidx, idx, data, size);
	} else {
		add_entry(db, aidx, idx, data, size);
	}
}

void mark(sqlite3 *db, const char* archive, int idx)
{
	Statement(db, "update entry set completed = 1 where idx = ? and aidx = (select idx from archive where path = ?)")
	    .bind(1, idx).bind(2, archive)();
}

void purge_unreferenced(sqlite3 *db)
{
	sqlite3_exec(db, "delete from archive where idx not in (select aidx from entry)", NULL, NULL, NULL);
}

void purge_unmarked(sqlite3 *db, const char *archive)
{
	Statement(db, "delete from entry where completed = 0 and aidx = (select idx from archive where path = ?)")
	    .bind(1, archive)();
	purge_unreferenced(db);
}

void purge_unmarked_all(sqlite3 *db)
{
	sqlite3_exec(db, "delete from entry where completed = 0", NULL, NULL, NULL);
	purge_unreferenced(db);
}

int get_size(sqlite3 *db)
{
	Statement stmt(db, "select sum(length(data)) from entry");
	stmt();
	return stmt.get_int(0);
}

void reduce_size_with_archive(sqlite3 *db, const char* archive, int size)
{
	Statement stmt(db,
	    "delete from entry where aidx = (select idx from archive where path = ?) and "
	        "idx <= (select idx from "
	            "(select t1.*,sum(length(t2.data)) cutsize from entry t1, entry t2 "
	                "where t1.aidx = "
	                    "(select idx from archive where path = ?1) "
	                    "and t2.aidx = t1.aidx and t2.idx <= t1.idx group by t1.idx) "
	            "order by cutsize - ?2 < 0, abs(cutsize - ?2) limit 1)");
	stmt.bind(1, archive).bind(2, size)();
	purge_unreferenced(db);
}

void reduce_size_with_aidx(sqlite3 *db, int aidx, int size)
{
	Statement stmt(db,
	    "delete from entry where aidx = ? and "
	        "idx <= (select idx from "
	            "(select t1.*,sum(length(t2.data)) cutsize from entry t1, entry t2 "
	                "where t1.aidx = ?1 "
	                    "and t2.aidx = t1.aidx and t2.idx <= t1.idx group by t1.idx) "
	            "order by cutsize - ?2 < 0, abs(cutsize - ?2) limit 1)");
	stmt.bind(1, aidx).bind(2, size)();
	purge_unreferenced(db);
}

void reduce_size(sqlite3 *db, int size)
{
	int cur_size = get_size(db);
	while(size > 0 && cur_size != 0) {
		Statement stmt(db, "select idx from archive order by atime limit 1");
		if(stmt()) {
			int aidx = stmt.get_int(0);
			reduce_size_with_aidx(db, aidx, size);
			int new_size = get_size(db);
			size -= (cur_size - new_size);
			cur_size = new_size;
		} else break;
	}
}

void access_archive(sqlite3 *db, const char* archive)
{
	std::time_t atime;
	std::time(&atime);
	Statement(db, "update archive set atime = ? where path = ?")
	    .bind(1, (int)atime).bind(2, archive)();
}

bool exists(sqlite3 *db, const char* archive, int idx)
{
	Statement stmt(db, "select count(*) from archive, entry where entry.aidx = archive.idx and archive.path = ? and entry.idx = ?");
	stmt.bind(1, archvie).bind(2, idx);
	return stmt() && stmt.get_int(0) > 0;
}

int main(void)
{
    sqlite3 *db;
    sqlite3_open("test.db", &db);
    init_db(db);
    access_archive(db, "hoge.rar");
    append(db, "hoge.rar", 0, "TEXTTEXT", 8);
    append(db, "hoge.rar", 0, "TEXTTEXT", 8);
    mark(db, "hoge.rar", 0);
    append(db, "hoge.rar", 0, "TEXT", 4);
    append(db, "hoge.rar", 1, "FOOFOO", 6);
    append(db, "hoge.rar", 1, "FOOFOO", 6);
    append(db, "hoge.rar", 2, "BARFOO", 6);
    append(db, "hoge.rar", 2, "BARFOO", 6);
    access_archive(db, "hoge2.rar");
    append(db, "hoge2.rar", 0, "TEXTTEXT", 8);
    append(db, "hoge2.rar", 0, "TEXTTEXT", 8);
    append(db, "hoge2.rar", 0, "TEXTTEXT", 8);
//    access_archive(db, "hoge.rar");
    dump_table(db, "archive");
    dump_table(db, "entry");
//    purge_unmarked(db, "hoge2.rar");
	reduce_size(db, 100);
    dump_table(db, "archive");
    dump_table(db, "entry");
    sqlite3_close(db);
    return 0;
}

