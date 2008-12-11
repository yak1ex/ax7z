// SolidCache.h

#pragma once

#ifndef SOLIDCACHE_H
#define SOLIDCACHE_H

#define NOMINMAX
#include <windows.h>
#undef NOMINMAX
#include <vector>
#include <map>
#include <string>
#include <algorithm>
#include <ctime>
#include <cassert>

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

struct sqlite3;

class SolidFileCacheDisk;

class SolidCacheDisk
{
private:
	int m_nMaxDisk;
	std::string m_sCacheFolder;
	sqlite3* m_db;

	void InitDB();
	std::string GetFileName(__int64 id) const;
	bool ExistsArchive(const char* archive);
	void AddArchive(const char* archive);
	int GetArchiveIdx(const char* archive);
	bool ExistsEntry(int aidx, int idx);
	void AppendEntry(int aidx, int idx, const void* data, int size);
	void AddEntry(int aidx, int idx, const void *data, int size);
	void PurgeUnreferenced();
	void PurgeUnmarkedAll();
	void PurgeUnmarkedOther(int aidx);
	int GetSize() const;
	void ReduceSizeWithAIdx(int aidx, int size);
	void ReduceSize(int size, int exclude_aidx);
	void AccessArchive(const char* archive);
public:
	SolidCacheDisk():m_nMaxDisk(-1),m_sCacheFolder(""),m_db(0) {}
	~SolidCacheDisk();
	bool IsCached(const char* archive, unsigned int index) const;
	bool Exists(const char* archive, unsigned int index) const;
	void Append(const char* archive, unsigned int index, const void* data, unsigned int size);
	void Cached(const char* archive, unsigned int index);
	void CachedVector(const char* archive, std::vector<unsigned int>& vIndex);
	void PurgeUnmarked(const char *archive);
	void GetContent(const char* archive, unsigned int index, void* dest, unsigned int size) const;
	void OutputContent(const char* archive, unsigned int index, unsigned int size, FILE* fp) const;
	static SolidCacheDisk& GetInstance();
	static SolidFileCacheDisk GetFileCache(const std::string& filename);
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
	bool Exists(unsigned int index) const
	{
		return m_scd.Exists(m_sArchive.c_str(), index);
	}
	void Append(unsigned int index, const void* data, unsigned int size)
	{
		m_scd.Append(m_sArchive.c_str(), index, data, size);
	}
	void Cached(unsigned int index)
	{
		m_scd.Cached(m_sArchive.c_str(), index);
	}
	void CachedVector(std::vector<unsigned int>& vIndex)
	{
		m_scd.CachedVector(m_sArchive.c_str(), vIndex);
	}
	void PurgeUnmarked()
	{
		m_scd.PurgeUnmarked(m_sArchive.c_str());
	}
	void GetContent(unsigned int index, void* dest, unsigned int size) const
	{
		m_scd.GetContent(m_sArchive.c_str(), index, dest, size);
	}
	void OutputContent(unsigned int index, unsigned int size, FILE* fp) const
	{
		m_scd.OutputContent(m_sArchive.c_str(), index, size, fp);
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
	SolidFileCacheMemory(FileCache& cache, std::time_t &atime) : m_cache(cache), m_atime(atime)
	{
	}
	void AccessArchive();
	FileCache& m_cache;
	std::time_t m_atime;
public:
	bool IsCached(unsigned int index) const
	{
		return m_cache.count(index) > 0 && m_cache[index].fCached;
	}
	bool Exists(unsigned int index) const
	{
		return m_cache.count(index) > 0;
	}
	void Append(unsigned int index, const void* data, unsigned int size)
	{
		m_cache[index].vBuffer.insert(m_cache[index].vBuffer.end(), 
			static_cast<const unsigned char*>(data), 
			static_cast<const unsigned char*>(data)+size);
		OutputDebugPrintf("SolidCacheMemory::Append: %d %p %d %d, %02X %02X %02X %02X", index, data, size, m_cache[index].vBuffer.size(), m_cache[index].vBuffer[0], m_cache[index].vBuffer[1], m_cache[index].vBuffer[2], m_cache[index].vBuffer[3]);
	}
	void Cached(unsigned int index)
	{
		m_cache[index].fCached = true;
//		OutputDebugPrintf("SolidCache::Cached: %d", index);
	}
	void CachedVector(std::vector<unsigned int>& vIndex)
	{
		std::vector<unsigned int>::iterator it = vIndex.begin(), itEnd = vIndex.end();
		for(; it != itEnd; ++it) {
			if(m_cache.count(*it)) {
				m_cache[*it].fCached = true;
			}
		}
	}
	void Remove(unsigned int index)
	{
		m_cache.erase(index);
	}
	unsigned int ReduceSize(unsigned int uiSize, void(*fCallback)(void*, const std::string&, unsigned int, void*, unsigned int, bool), void* pArg);
	unsigned int GetCount() const
	{
		return m_cache.size();
	}
	int GetCurSize(unsigned int index) const
	{
		return m_cache[index].vBuffer.size();
	}
	const void* GetContent(unsigned int index) const
	{
		return &m_cache[index].vBuffer[0];
	}
	void GetContent(unsigned int index, void* dest, unsigned int size) const
	{
		OutputDebugPrintf("SolidCacheMemory::GetContent: %d %p %d %d %02X %02X %02X %02X", index, dest, size, m_cache[index].vBuffer.size(), m_cache[index].vBuffer[0], m_cache[index].vBuffer[1], m_cache[index].vBuffer[2], m_cache[index].vBuffer[3]);
		// TODO: error check
		CopyMemory(dest, &m_cache[index].vBuffer[0], std::min(size, m_cache[index].vBuffer.size()));
	}
	void OutputContent(unsigned int index, unsigned int size, FILE* fp) const
	{
		OutputDebugPrintf("SolidCacheMemory::OutputContent: %d %p %d %d %02X %02X %02X %02X", index, fp, size, m_cache[index].vBuffer.size(), m_cache[index].vBuffer[0], m_cache[index].vBuffer[1], m_cache[index].vBuffer[2], m_cache[index].vBuffer[3]);
		fwrite(&m_cache[index].vBuffer[0], std::min(size, m_cache[index].vBuffer.size()), 1, fp);
	}
};

class SolidCacheMemory
{
private:
	std::map<std::string, SolidFileCacheMemory::FileCache> m_mTable;
	std::map<std::string, std::time_t> m_mAccess;
	int m_nMaxMemory;
	void AccessArchive(const char* archive);
	std::string GetLRUArchive() const;
public:
	SolidCacheMemory():m_nMaxMemory(-1) {}
	SolidFileCacheMemory GetFileCache(const std::string& sArchive)
	{
		return SolidFileCacheMemory(m_mTable[sArchive], m_mAccess[sArchive]);
	}
	int GetMaxMemory() const { return m_nMaxMemory; }
	int SetMaxMemory(int nNew)
	{
		int nOld = m_nMaxMemory;
		m_nMaxMemory = nNew;
		return nOld;
	}
	int GetSize() const;
	unsigned int ReduceSize(unsigned int uiSize, void(*)(void*, const std::string&, unsigned int, void*, unsigned int, bool), void* pArg);
};

class SolidFileCache;

class SolidCache
{
private:
	int m_nMaxLookAhead;
	SolidCacheDisk m_scd;
	SolidCacheMemory m_scm;
	SolidCache() : m_nMaxLookAhead(-1) {}
public:
	static SolidCache& GetInstance();
	static SolidFileCache GetFileCache(const std::string& filename);

	bool IsCached(const std::string& sArchive, unsigned int index) /* const */
	{
		return m_scm.GetFileCache(sArchive).IsCached(index) || m_scd.IsCached(sArchive.c_str(), index);
	}
	void Append(const std::string& sArchvie, unsigned int index, const void* data, unsigned int size);
	void Cached(const std::string& sArchive, unsigned int index)
	{
		OutputDebugPrintf("SolidCache::Cached %s %d", sArchive.c_str(), index);
		if(m_scm.GetFileCache(sArchive).Exists(index)) {
			OutputDebugPrintf("SolidCache::Cached:memory %s %d", sArchive.c_str(), index);
			m_scm.GetFileCache(sArchive).Cached(index);
		} else if(m_scd.Exists(sArchive.c_str(), index)) {
			OutputDebugPrintf("SolidCache::Cached:disk %s %d", sArchive.c_str(), index);
			m_scd.Cached(sArchive.c_str(), index);
		} else {
			assert("SolidCache::Cached: Not reached");
		}
	}
	void CachedVector(const std::string& sArchive, std::vector<unsigned int>& vIndex)
	{
		m_scm.GetFileCache(sArchive).CachedVector(vIndex);
		m_scd.CachedVector(sArchive.c_str(), vIndex);
	}
	void PurgeUnmarked(const std::string& sArchive)
	{
// TODO: Implement for memory
//		m_scm.GetFileCache(sArchive);
		m_scd.PurgeUnmarked(sArchive.c_str());
	}
	void GetContent(const std::string& sArchive, unsigned int index, void* dest, unsigned int size) /* const */
	{
		OutputDebugPrintf("SolidCache::GetContent %s %d %d bytes", sArchive.c_str(), index, size);
		if(m_scm.GetFileCache(sArchive).IsCached(index)) {
			m_scm.GetFileCache(sArchive).GetContent(index, dest, size);
		} else if(m_scd.IsCached(sArchive.c_str(), index)) {
			m_scd.GetContent(sArchive.c_str(), index, dest, size);
		} else {
			assert("SolidCache::GetContent: Not reached");
		}
	}
	void OutputContent(const std::string& sArchive, unsigned int index, unsigned int size, FILE* fp) /* const */
	{
		OutputDebugPrintf("SolidCache::OutputContent %s %d %d bytes", sArchive.c_str(), index, size);
		if(m_scm.GetFileCache(sArchive).IsCached(index)) {
			m_scm.GetFileCache(sArchive).OutputContent(index, size, fp);
		} else if(m_scd.IsCached(sArchive.c_str(), index)) {
			m_scd.OutputContent(sArchive.c_str(), index, size, fp);
		} else {
			assert("SolidCache::OutputContent: Not reached");
		}
	}

	int GetMaxLookAhead() const { return m_nMaxLookAhead; }
	int SetMaxLookAhead(int nNew)
	{
		int nOld = m_nMaxLookAhead;
		m_nMaxLookAhead = nNew;
		return nOld;
	}
	int GetMaxMemory() const { return m_scm.GetMaxMemory(); }
	int SetMaxMemory(int nNew) { return m_scm.SetMaxMemory(nNew); }
	int GetMaxDisk() const { return m_scd.GetMaxDisk(); }
	int SetMaxDisk(int nNew) { return m_scd.SetMaxDisk(nNew); }
	const std::string& GetCacheFolder() const { return m_scd.GetCacheFolder(); }
	std::string SetCacheFolder(std::string sNew) { return m_scd.SetCacheFolder(sNew); }
};

class SolidFileCache
{
	friend class SolidCache;
private:
	SolidCache &m_sc;
	std::string m_sArchive;
	UINT32 m_nMaxNum;
	SolidFileCache(SolidCache &sc, const std::string &sArchive) : m_sc(sc), m_sArchive(sArchive) {}
public:
	bool IsCached(unsigned int index) /* const */ { return m_sc.IsCached(m_sArchive, index); }
	void Append(unsigned int index, const void* data, unsigned int size)
	{
		m_sc.Append(m_sArchive, index, data, size);
	}
	void Cached(unsigned int index) { m_sc.Cached(m_sArchive, index); }
	void CachedVector(std::vector<unsigned int>& vIndex) { m_sc.CachedVector(m_sArchive, vIndex); }
	void PurgeUnmarked() { m_sc.PurgeUnmarked(m_sArchive); }
	void GetContent(unsigned int index, void* dest, unsigned int size) /* const */
	{
		m_sc.GetContent(m_sArchive, index, dest, size);
	}
	void OutputContent(unsigned int index, unsigned int size, FILE* fp) /* const */
	{
		m_sc.OutputContent(m_sArchive, index, size, fp);
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

#endif