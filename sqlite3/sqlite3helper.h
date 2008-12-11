#ifndef SQLITE3HELPER_H
#define SQLITE3HELPER_H

#include "sqlite3.h"

namespace yak {
namespace sqlite {

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
	Statement& bind(int idx, unsigned int value)
	{
		sqlite3_bind_int64(m_stmt, idx, value);
		return *this;
	}
	Statement& bind(int idx, sqlite3_int64 value)
	{
		sqlite3_bind_int64(m_stmt, idx, value);
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
	sqlite3_int64 get_int64(int idx)
	{
		return sqlite3_column_int64(m_stmt, idx);
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

} // namespace sqlite
} // namespace yak

#endif
