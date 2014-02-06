#ifndef SQLITE3HELPER_DECL_H
#define SQLITE3HELPER_DECL_H

#include <stdexcept>
#include <stdint.h>

struct sqlite3;
struct sqlite3_stmt;

namespace yak {
namespace sqlite {

struct sqlite_error : std::runtime_error
{
	sqlite_error(const char* msg) : std::runtime_error(msg) {}
};

// Tiny wrapper to sqlite3
class Database
{
private:
	sqlite3* m_db;
	Database& operator=(const Database&);
	Database(const Database&);
public:
	Database();
	Database(const char* dbfile);
	Database& reopen(const char* dbfile);
	~Database();
	void close();
	const Database& exec(const char* sql) const;
	const char* errmsg() const;
	operator sqlite3*();
	operator const sqlite3*() const;
};

// Tiny wrapper to sqlite3_stmt
class Statement
{
private:
	sqlite3_stmt *m_stmt;
	Statement& operator=(const Statement&);
	Statement(const Statement&);
public:
	Statement();
	Statement(sqlite3 *db, const char* sql);
	Statement& reprepare(sqlite3 *db, const char* sql);
	const Statement& reset() const;
	Statement& finalize();
	~Statement();
	const Statement& bind(int idx, int value) const;
	const Statement& bind(int idx, unsigned int value) const;
	const Statement& bind(int idx, int64_t value) const;
	const Statement& bind(int idx, const char* value) const;
	const Statement& bind(int idx, const void* value, int size, void (*func)(void*) = reinterpret_cast<void(*)(void*)>(-1) /* SQLITE_TRANSIENT */) const;
	bool operator()() const;
	int get_int(int idx) const;
	int64_t get_int64(int idx) const;
	double get_double(int idx) const;
	const char* get_text(int idx) const;
	const void* get_blob(int idx) const;
	int get_bytes(int idx) const;
	const char* get_name(int idx) const;
	int get_type(int idx) const;
	int get_count() const;
};

} // namespace sqlite
} // namespace yak

#endif
