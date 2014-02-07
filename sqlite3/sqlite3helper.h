#ifndef SQLITE3HELPER_H
#define SQLITE3HELPER_H

#include "sqlite3helper_decl.h"
#include "sqlite3.h"

namespace yak {
namespace sqlite {

// TODO: Move support

// Tiny wrapper to sqlite3
#define CHECK(exp) do { if((exp) != SQLITE_OK) { throw sqlite_error(this->errmsg()); } } while(0)
inline Database::Database() : m_db(0) {}
inline Database::Database(const char* dbfile)
{
	CHECK(sqlite3_open(dbfile, &m_db));
}
inline Database& Database::reopen(const char* dbfile)
{
	// TODO: check return value
	CHECK(sqlite3_close(m_db));
	CHECK(sqlite3_open(dbfile, &m_db));
	return *this;
}
inline Database::~Database()
{
	sqlite3_close(m_db); // no throw
}
inline void Database::close()
{
	CHECK(sqlite3_close(m_db));
	m_db = 0;
}
inline const Database& Database::exec(const char* sql) const
{
	CHECK(sqlite3_exec(m_db, sql, 0, 0, 0));
	return *this;
}
inline const char* Database::errmsg() const
{
	return sqlite3_errmsg(m_db);
}
inline Database::operator sqlite3*() const { return m_db; }
#undef CHECK

// Tiny wrapper to sqlite3_stmt
inline Statement::Statement() : m_stmt(0) {}
inline Statement::Statement(sqlite3 *db, const char* sql)
{
	sqlite3_prepare_v2(db, sql, -1, &m_stmt, NULL);
}
inline Statement& Statement::reprepare(sqlite3 *db, const char* sql)
{
	sqlite3_finalize(m_stmt);
	sqlite3_prepare_v2(db, sql, -1, &m_stmt, NULL);
	return *this;
}
inline const Statement& Statement::reset() const
{
	if(!m_stmt) throw std::logic_error("not prepared");
	sqlite3_reset(m_stmt);
	return *this;
}
inline Statement& Statement::finalize()
{
	sqlite3_finalize(m_stmt);
	m_stmt = 0;
	return *this;
}
inline Statement::~Statement()
{
	sqlite3_finalize(m_stmt);
}
inline const Statement& Statement::bind(int idx, int value) const
{
	if(!m_stmt) throw std::logic_error("not prepared");
	sqlite3_bind_int(m_stmt, idx, value);
	return *this;
}
inline const Statement& Statement::bind(int idx, unsigned int value) const
{
	if(!m_stmt) throw std::logic_error("not prepared");
	sqlite3_bind_int64(m_stmt, idx, value);
	return *this;
}
inline const Statement& Statement::bind(int idx, int64_t value) const
{
	if(!m_stmt) throw std::logic_error("not prepared");
	sqlite3_bind_int64(m_stmt, idx, value);
	return *this;
}
inline const Statement& Statement::bind(int idx, const char* value) const
{
	if(!m_stmt) throw std::logic_error("not prepared");
	sqlite3_bind_text(m_stmt, idx, value, -1, SQLITE_TRANSIENT);
	return *this;
}
inline const Statement& Statement::bind(int idx, const void* value, int size, void (*func)(void*)) const
{
	if(!m_stmt) throw std::logic_error("not prepared");
	sqlite3_bind_blob(m_stmt, idx, value, size, func);
	return *this;
}
inline bool Statement::operator()() const
{
	if(!m_stmt) throw std::logic_error("not prepared");
// TODO: catch error on prepare()
	return sqlite3_step(m_stmt) == SQLITE_ROW;
}
inline int Statement::get_int(int idx) const
{
	return sqlite3_column_int(m_stmt, idx);
}
inline int64_t Statement::get_int64(int idx) const
{
	return sqlite3_column_int64(m_stmt, idx);
}
inline double Statement::get_double(int idx) const
{
	return sqlite3_column_double(m_stmt, idx);
}
inline const char* Statement::get_text(int idx) const
{
	return static_cast<const char*>(static_cast<const void*>(sqlite3_column_text(m_stmt, idx)));
}
inline const void* Statement::get_blob(int idx) const
{
	return sqlite3_column_blob(m_stmt, idx);
}
inline int Statement::get_bytes(int idx) const
{
	return sqlite3_column_bytes(m_stmt, idx);
}
inline const char* Statement::get_name(int idx) const
{
	return sqlite3_column_name(m_stmt, idx);
}
inline int Statement::get_type(int idx) const
{
	return sqlite3_column_type(m_stmt, idx);
}
inline int Statement::get_count() const
{
	return sqlite3_column_count(m_stmt);
}

} // namespace sqlite
} // namespace yak

#endif
