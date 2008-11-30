// SolidCache.h

#pragma once

#ifndef SOLIDCACHE_H
#define SOLIDCACHE_H

#include <windows.h>
#include <vector>
#include <map>
#include <string>
#include <algorithm>

#undef min
#undef max

#ifdef NDEBUG
#define OutputDebugPrintf (void)
#else
static void OutputDebugPrintf(char* format, ...)
{
	va_list ap;
	va_start(ap, format);
	char buf[2048];
	wvsprintf(buf, format, ap);
	OutputDebugString(buf);
	va_end(ap);
}
#endif

#if 0
class SolidFileCache
{
public:
	bool IsCached(unsigned int index) const;
	void Append(unsigned int index, const void* data, unsigned int size);
	void Cached(unsigned int index);
	void GetContent(unsigned int index, void* dest, unsigned int size) const;
	void OutputContent(unsigned int index, unsigned int size, FILE* fp) const;
	void GetExtractVector(std::vector<UINT32> &v, UINT32 index, UINT32 num, UINT32 max_num);
	int GetProgress(UINT32 index);
	int GetProgressDenom(UINT32 index);
};
#endif

struct sqlite3;

class SolidFileCacheDisk;

class SolidCacheDisk
{
private:
	SolidCacheDisk():m_nMaxLookAhead(-1),m_nMaxMemory(-1),m_nMaxDisk(-1),m_sCacheFolder(""),m_db(0) {}
	int m_nMaxLookAhead;
	int m_nMaxMemory;
	int m_nMaxDisk;
	std::string m_sCacheFolder;
	sqlite3* m_db;

	void InitDB();
	bool ExistsArchive(const char* archive);
	void AddArchive(const char* archive);
	int GetArchiveIdx(const char* archive);
	bool ExistsEntry(int aidx, int idx);
	void AppendEntry(int aidx, int idx, const void* data, int size);
	void AddEntry(int aidx, int idx, const void *data, int size);
	void PurgeUnreferenced();
	void PurgeUnmarked(const char *archive);
	void PurgeUnmarkedAll();
	int GetSize();
	void ReduceSizeWithArchive(const char* archive, int size);
	void ReduceSizeWithAIdx(int aidx, int size);
	void ReduceSize(int size);
	void AccessArchive(const char* archive);
public:
	~SolidCacheDisk();
	bool IsCached(const char* archive, unsigned int index) const;
	void Append(const char* archive, unsigned int index, const void* data, unsigned int size);
	void Cached(const char* archive, unsigned int index);
	void GetContent(const char* archive, unsigned int index, void* dest, unsigned int size) const;
	void OutputContent(const char* archive, unsigned int index, unsigned int size, FILE* fp) const;
	static SolidCacheDisk& GetInstance();
	static SolidFileCacheDisk GetFileCache(const std::string& filename);
	int GetMaxLookAhead() const { return m_nMaxLookAhead; }
	int SetMaxLookAhead(int nNew)
	{
		int nOld = m_nMaxLookAhead;
		m_nMaxLookAhead = nNew;
		return nOld;
	}
	int GetMaxMemory() const { return m_nMaxMemory; }
	int SetMaxMemory(int nNew)
	{
		int nOld = m_nMaxMemory;
		m_nMaxMemory = nNew;
		return nOld;
	}
	int GetMaxDisk() const { return m_nMaxDisk; }
	int SetMaxDisk(int nNew)
	{
		int nOld = m_nMaxDisk;
		m_nMaxDisk = nNew;
		return nOld;
	}
	const std::string& GetCacheFolder() const { return m_sCacheFolder; }
	std::string SetCacheFolder(std::string sNew);
};

class SolidFileCacheDisk
{
	friend class SolidCacheDisk;
private:
	SolidCacheDisk &m_scd;
	std::string m_sArchive;
	UINT32 m_nMaxNum;
	SolidFileCacheDisk(SolidCacheDisk &scd, const std::string &sArchive)
	    : m_scd(scd), m_sArchive(sArchive)
	{
	}
public:
	bool IsCached(unsigned int index) const
	{
		return m_scd.IsCached(m_sArchive.c_str(), index);
	}
	void Append(unsigned int index, const void* data, unsigned int size)
	{
		m_scd.Append(m_sArchive.c_str(), index, data, size);
	}
	void Cached(unsigned int index)
	{
		m_scd.Cached(m_sArchive.c_str(), index);
	}
	void GetContent(unsigned int index, void* dest, unsigned int size) const
	{
		m_scd.GetContent(m_sArchive.c_str(), index, dest, size);
	}
	void OutputContent(unsigned int index, unsigned int size, FILE* fp) const
	{
		m_scd.OutputContent(m_sArchive.c_str(), index, size, fp);
	}
	void GetExtractVector(std::vector<UINT32> &v, UINT32 index, UINT32 num, UINT32 max_num)
	{
		UINT32 start = 0;
		m_nMaxNum = max_num;
		UINT32 end = m_nMaxNum - 1;
		v.resize(end - start + 1);
		for(UINT32 i = start; i <= end; ++i) v[i - start] = i;
	}
	int GetProgress(UINT32 index)
	{
		return index;
	}
	int GetProgressDenom(UINT32 index)
	{
		return m_nMaxNum;
	}
};

class SolidFileCacheMemory
{
	friend class SolidCacheMemory;
private:
	struct Entry
	{
		bool fCached;
		std::vector<unsigned char> vBuffer;
		Entry():fCached(false) {}
	};
	typedef std::map<unsigned int, Entry> FileCache;
	SolidFileCacheMemory(FileCache& cache) : m_cache(cache)
	{
	}
	FileCache& m_cache;
	UINT32 m_nMaxNum;
public:
	bool IsCached(unsigned int index) const
	{
		return m_cache.count(index) > 0 && m_cache[index].fCached;
	}
	void Append(unsigned int index, const void* data, unsigned int size)
	{
		m_cache[index].vBuffer.insert(m_cache[index].vBuffer.end(), 
			static_cast<const unsigned char*>(data), 
			static_cast<const unsigned char*>(data)+size);
		OutputDebugPrintf("SolidCache::Append: %d %p %d %d, %02X %02X %02X %02X", index, data, size, m_cache[index].vBuffer.size(), m_cache[index].vBuffer[0], m_cache[index].vBuffer[1], m_cache[index].vBuffer[2], m_cache[index].vBuffer[3]);
	}
	void Cached(unsigned int index)
	{
		m_cache[index].fCached = true;
//		OutputDebugPrintf("SolidCache::Cached: %d", index);
	}
	void GetContent(unsigned int index, void* dest, unsigned int size) const
	{
		OutputDebugPrintf("SolidCache::GetContent: %d %p %d %d %02X %02X %02X %02X", index, dest, size, m_cache[index].vBuffer.size(), m_cache[index].vBuffer[0], m_cache[index].vBuffer[1], m_cache[index].vBuffer[2], m_cache[index].vBuffer[3]);
		// TODO: error check
		CopyMemory(dest, &m_cache[index].vBuffer[0], std::min(size, m_cache[index].vBuffer.size()));
	}
	void OutputContent(unsigned int index, unsigned int size, FILE* fp) const
	{
		OutputDebugPrintf("SolidCache::OutputContent: %d %p %d %d %02X %02X %02X %02X", index, fp, size, m_cache[index].vBuffer.size(), m_cache[index].vBuffer[0], m_cache[index].vBuffer[1], m_cache[index].vBuffer[2], m_cache[index].vBuffer[3]);
		fwrite(&m_cache[index].vBuffer[0], std::min(size, m_cache[index].vBuffer.size()), 1, fp);
	}
	void GetExtractVector(std::vector<UINT32> &v, UINT32 index, UINT32 num, UINT32 max_num)
	{
// Need to co-operate with Append() / Cached()
//		UINT32 start = m_cache.size() ? std::min((--m_cache.end())->first, index) : 0;
//		UINT32 end = std::min(std::max(start + num, index), max_num - 1);
		UINT32 start = 0;
		m_nMaxNum = max_num;
		UINT32 end = m_nMaxNum - 1;
		v.resize(end - start + 1);
		for(UINT32 i = start; i <= end; ++i) v[i - start] = i;
	}
	int GetProgress(UINT32 index)
	{
		return index;
	}
	int GetProgressDenom(UINT32 index)
	{
		return m_nMaxNum;
	}
};

class SolidCacheMemory
{
private:
	SolidCacheMemory():m_nMaxLookAhead(-1),m_nMaxMemory(-1),m_nMaxDisk(-1),m_sCacheFolder("") {}
	static std::map<std::string, SolidFileCacheMemory::FileCache> table;
	int m_nMaxLookAhead;
	int m_nMaxMemory;
	int m_nMaxDisk;
	std::string m_sCacheFolder;
public:
	static SolidCacheMemory& GetInstance();
	static SolidFileCacheMemory GetFileCache(const std::string& filename)
	{
		return SolidFileCacheMemory(table[filename]);
	}
	int GetMaxLookAhead() const { return m_nMaxLookAhead; }
	int SetMaxLookAhead(int nNew)
	{
		int nOld = m_nMaxLookAhead;
		m_nMaxLookAhead = nNew;
		return nOld;
	}
	int GetMaxMemory() const { return m_nMaxMemory; }
	int SetMaxMemory(int nNew)
	{
		int nOld = m_nMaxMemory;
		m_nMaxMemory = nNew;
		return nOld;
	}
	int GetMaxDisk() const { return m_nMaxDisk; }
	int SetMaxDisk(int nNew)
	{
		int nOld = m_nMaxDisk;
		m_nMaxDisk = nNew;
		return nOld;
	}
	const std::string& GetCacheFolder() const { return m_sCacheFolder; }
	std::string SetCacheFolder(std::string sNew)
	{
		std::swap(sNew, m_sCacheFolder);
		return sNew;
	}
};

typedef SolidFileCacheDisk SolidFileCache;
typedef SolidCacheDisk SolidCache;

#endif
