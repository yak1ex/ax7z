#include <iostream>
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
			std::cout << std::endl;
		}
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

void add_archive(sqlite3 *db, const char* archive, int time)
{
	Statement(db, "insert into archive (path, atime) values (?, ?)").bind(1, archive).bind(2, time)();
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
		add_archive(db, archive, 0);
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

int main(void)
{
    sqlite3 *db;
    sqlite3_open("test.db", &db);
    init_db(db);
    append(db, "hoge.rar", 0, "TEXT", 4);
    append(db, "hoge.rar", 0, "TEXT", 4);
    mark(db, "hoge.rar", 0);
    append(db, "hoge.rar", 0, "TEXT", 4);
    append(db, "hoge.rar", 1, "FOO", 3);
    append(db, "hoge2.rar", 0, "TEXT", 4);
    dump_table(db, "archive");
    dump_table(db, "entry");
    sqlite3_close(db);
    return 0;
}

