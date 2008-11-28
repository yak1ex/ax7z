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

void dump_table(sqlite3 *db, char *name)
{
	sqlite3_stmt *stmt;
	char buf[2048];
	sqlite3_snprintf(sizeof(buf), buf, "select * from %s", name);
	std::cout << sqlite3_prepare_v2(db, buf, -1, &stmt, NULL) << std::endl;
//	sqlite3_bind_text(stmt, 1, name, -1, SQLITE_TRANSIENT);
	if(stmt) {
		while(sqlite3_step(stmt) == SQLITE_ROW) {
			int nColumn = sqlite3_column_count(stmt);
			for(int i=0;i<nColumn;++i) {
				std::cout << sqlite3_column_name(stmt, i) << ':';
				switch(sqlite3_column_type(stmt, i)) {
				case SQLITE_INTEGER:
					std::cout << sqlite3_column_int64(stmt, i);
					break;
				case SQLITE_FLOAT:
					std::cout << sqlite3_column_double(stmt, i);
					break;
				case SQLITE_TEXT:
					std::cout << sqlite3_column_text(stmt, i);
					break;
				case SQLITE_BLOB:
					std::cout << '(' << sqlite3_column_bytes(stmt, i) << ')';
					break;
				case SQLITE_NULL:
					std::cout << "NULL";
					break;
				}
				std::cout << std::endl;
			}
		}
	}
}

int count(sqlite3 *db, char *sql)
{
	sqlite3_stmt *stmt;
	sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
	int result = -1;
	if(stmt) {
		if(sqlite3_step(stmt) == SQLITE_ROW) {
			result = sqlite3_column_int(stmt, 0);
		}
	}
	sqlite3_finalize(stmt);
	return result;
}

void init_db(sqlite3 *db)
{
	sqlite3_exec(db, "create table archive (idx INTEGER PRIMARY KEY AUTOINCREMENT, path TEXT, atime INTEGER)", NULL, NULL, NULL);
	sqlite3_exec(db, "create table entry (aidx INTERGER, idx INTEGER, data BLOB, completed INTEGER)", NULL, NULL, NULL);
}

bool exists_archive(sqlite3 *db, const char* archive)
{
	sqlite3_stmt *stmt;
	sqlite3_prepare_v2(db, "select count(*) from archive where path = ?", -1, &stmt, NULL);
	sqlite3_bind_text(stmt, 1, archive, -1, SQLITE_TRANSIENT);
	bool fResult = sqlite3_step(stmt) == SQLITE_ROW && sqlite3_column_int(stmt, 0) > 0;
	sqlite3_finalize(stmt);
	return fResult;
}

void add_archive(sqlite3 *db, const char* archive, int time)
{
	sqlite3_stmt *stmt;
	sqlite3_prepare_v2(db, "insert into archive (path, atime) values (?, ?)", -1, &stmt, NULL);
	sqlite3_bind_text(stmt, 1, archive, -1, SQLITE_TRANSIENT);
	sqlite3_bind_int(stmt, 2, time);
	sqlite3_step(stmt);
	sqlite3_finalize(stmt);
}

int get_archive_idx(sqlite3 *db, const char* archive)
{
	sqlite3_stmt *stmt;
	sqlite3_prepare_v2(db, "select idx from archive where path = ?", -1, &stmt, NULL);
	sqlite3_bind_text(stmt, 1, archive, -1, SQLITE_TRANSIENT);
// TODO: assert
	sqlite3_step(stmt);
	int aidx = sqlite3_column_int(stmt, 0);
	sqlite3_finalize(stmt);
	return aidx;
}

bool exists_entry(sqlite3 *db, int aidx, int idx)
{
	sqlite3_stmt *stmt;
	sqlite3_prepare_v2(db, "select count(*) from entry where aidx = ? and idx = ?", -1, &stmt, NULL);
	sqlite3_bind_int(stmt, 1, aidx);
	sqlite3_bind_int(stmt, 2, idx);
	bool fResult = sqlite3_step(stmt) == SQLITE_ROW && sqlite3_column_int(stmt, 0) > 0;
	sqlite3_finalize(stmt);
	return fResult;
}

void append_entry(sqlite3 *db, int aidx, int idx, const void* data, int size)
{
	sqlite3_stmt *stmt;
	sqlite3_prepare_v2(db, "select data, completed from entry where aidx = ? and idx = ?", -1, &stmt, NULL);
	sqlite3_bind_int(stmt, 1, aidx);
	sqlite3_bind_int(stmt, 2, idx);
	sqlite3_step(stmt);
	if(sqlite3_column_int(stmt, 1) == 0) {
		int old_size = sqlite3_column_bytes(stmt, 0);
		char *store = static_cast<char*>(sqlite3_malloc(old_size + size));
		memcpy(store, sqlite3_column_blob(stmt, 0), old_size);
		memcpy(store + old_size, data, size);
		sqlite3_finalize(stmt);

		sqlite3_prepare_v2(db, "update entry set data = ? where aidx = ? and idx = ?", -1, &stmt, NULL);
		sqlite3_bind_blob(stmt, 1, store, old_size + size, sqlite3_free);
		sqlite3_bind_int(stmt, 2, aidx);
		sqlite3_bind_int(stmt, 3, idx);
		sqlite3_step(stmt);
		sqlite3_finalize(stmt);
	} else {
		sqlite3_finalize(stmt);
	}
}

void add_entry(sqlite3 *db, int aidx, int idx, const void *data, int size)
{
	sqlite3_stmt *stmt;
	sqlite3_prepare_v2(db, "insert into entry (aidx, idx, data, completed) values (?, ?, ?, 0)", -1, &stmt, NULL);
	sqlite3_bind_int(stmt, 1, aidx);
	sqlite3_bind_int(stmt, 2, idx);
	sqlite3_bind_blob(stmt, 3, data, size, SQLITE_TRANSIENT);
	sqlite3_step(stmt);
	sqlite3_finalize(stmt);
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
	sqlite3_stmt *stmt;
	sqlite3_prepare_v2(db, "update entry set completed = 1 where idx = ? and aidx = (select idx from archive where path = ?)", -1, &stmt, NULL);
	sqlite3_bind_int(stmt, 1, idx);
	sqlite3_bind_text(stmt, 2, archive, -1, SQLITE_TRANSIENT);
	sqlite3_step(stmt);
	sqlite3_finalize(stmt);
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

