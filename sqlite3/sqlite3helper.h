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
	Statement() : m_stmt(0) {}
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
	const Statement& reset() const
	{
		if(!m_stmt) throw std::logic_error("not prepared");
		sqlite3_reset(m_stmt);
		return *this;
	}
	Statement& finalize()
	{
		sqlite3_finalize(m_stmt);
		m_stmt = 0;
		return *this;
	}
	~Statement()
	{
		sqlite3_finalize(m_stmt);
	}
	const Statement& bind(int idx, int value) const
	{
		if(!m_stmt) throw std::logic_error("not prepared");
		sqlite3_bind_int(m_stmt, idx, value);
		return *this;
	}
	const Statement& bind(int idx, unsigned int value) const
	{
		if(!m_stmt) throw std::logic_error("not prepared");
		sqlite3_bind_int64(m_stmt, idx, value);
		return *this;
	}
	const Statement& bind(int idx, sqlite3_int64 value) const
	{
		if(!m_stmt) throw std::logic_error("not prepared");
		sqlite3_bind_int64(m_stmt, idx, value);
		return *this;
	}
	const Statement& bind(int idx, const char* value) const
	{
		if(!m_stmt) throw std::logic_error("not prepared");
		sqlite3_bind_text(m_stmt, idx, value, -1, SQLITE_TRANSIENT);
		return *this;
	}
	const Statement& bind(int idx, const void* value, int size, void (*func)(void*) = SQLITE_TRANSIENT) const
	{
		if(!m_stmt) throw std::logic_error("not prepared");
		sqlite3_bind_blob(m_stmt, idx, value, size, func);
		return *this;
	}
	bool operator()() const
	{
		if(!m_stmt) throw std::logic_error("not prepared");
// TODO: catch error on prepare()
		return sqlite3_step(m_stmt) == SQLITE_ROW;
	}
	int get_int(int idx) const
	{
		return sqlite3_column_int(m_stmt, idx);
	}
	sqlite3_int64 get_int64(int idx) const
	{
		return sqlite3_column_int64(m_stmt, idx);
	}
	double get_double(int idx) const
	{
		return sqlite3_column_double(m_stmt, idx);
	}
	const char* get_text(int idx) const
	{
		return static_cast<const char*>(static_cast<const void*>(sqlite3_column_text(m_stmt, idx)));
	}
	const void* get_blob(int idx) const
	{
		return sqlite3_column_blob(m_stmt, idx);
	}
	int get_bytes(int idx) const
	{
		return sqlite3_column_bytes(m_stmt, idx);
	}
	const char* get_name(int idx) const
	{
		return sqlite3_column_name(m_stmt, idx);
	}
	int get_type(int idx) const
	{
		return sqlite3_column_type(m_stmt, idx);
	}
	int get_count() const
	{
		return sqlite3_column_count(m_stmt);
	}
};

} // namespace sqlite
} // namespace yak

#endif
