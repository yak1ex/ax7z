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

static void OutputDebugPrintf(char* format, ...)
{
	va_list ap;
	va_start(ap, format);
	char buf[2048];
	wvsprintf(buf, format, ap);
	OutputDebugString(buf);
	va_end(ap);
}

class SolidCache
{
private:
	struct Entry
	{
		bool fCached;
		std::vector<unsigned char> vBuffer;
		Entry():fCached(false) {}
	};
	typedef std::map<unsigned int, Entry> FileCache;
	static std::map<std::string, FileCache> table;
	SolidCache(const std::string& filename) : m_cache(table[filename])
	{
	}
	FileCache& m_cache;
	UINT32 m_nMaxNum;
public:
	static SolidCache GetFileCache(const std::string& filename)
	{
		return SolidCache(filename);
	}
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


#endif
