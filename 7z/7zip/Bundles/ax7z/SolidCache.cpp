#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sstream>
#include "SolidCache.h"

SolidCache& SolidCache::GetInstance()
{
	static SolidCache sc;
	return sc;
}

SolidFileCache SolidCache::GetFileCache(const std::string& filename)
{
	return SolidFileCache(GetInstance(), filename);
}

void SolidCache::Append(const std::string& sArchive, unsigned int index, const void* data, unsigned int size)
{
	OutputDebugPrintf("SolidCache::Append %s %u %u bytes\n", sArchive.c_str(), index, size);
	if(m_scd.Exists(sArchive.c_str(), index) || GetMaxMemory() <= 0) {
		m_scd.Append(sArchive.c_str(), index, data, size);
		OutputDebugPrintf("SolidCache::Append:disk %s %u %u bytes\n", sArchive.c_str(), index, size);
	} else {
		m_scm.GetFileCache(sArchive).Append(index, data, size);
		OutputDebugPrintf("SolidCache::Append:memory %s %u %u bytes\n", sArchive.c_str(), index, size);
		if(m_scm.GetMaxMemory() > 0 && m_scm.GetSize() > m_scm.GetMaxMemoryInBytes()) {
			OutputDebugPrintf("SolidCache::Append:purge_memory2disk %s %u %u maxmem: %" UINT64_S "u curmem: %" UINT64_S "u\n", sArchive.c_str(), index, m_scm.GetFileCache(sArchive).GetCurSize(index), m_scm.GetMaxMemoryInBytes(), m_scm.GetSize());
			m_scm.ReduceSize(std::min(m_scm.GetSize(), std::max(m_scm.GetPurgeMemoryInBytes(), m_scm.GetSize() - m_scm.GetMaxMemoryInBytes())), SolidCacheDisk::ReduceSizeCallback, &m_scd, sArchive);
		}
	}
}

void SolidFileCacheMemory::AccessArchive_() const
{
	std::time(&m_atime);
}

struct Argument2
{
	std::string sArchive;
	void *pArg;
};

boost::uint64_t SolidFileCacheMemory::ReduceSize_(boost::uint64_t uiSize, void(*fCallback)(void*, const std::string&, unsigned int, void*, unsigned int, bool), void* pArg)
{
	Argument2 *pArg2 = static_cast<Argument2*>(pArg);
	while(uiSize > 0 && m_cache.size() > 0) {
		OutputDebugPrintf("SolidFileCacheMemory::ReduceSize %" UINT64_S "u\n", uiSize);
		unsigned int index = m_cache.begin()->first;
		fCallback(pArg2->pArg, pArg2->sArchive, index, &m_cache[index].vBuffer[0], m_cache[index].vBuffer.size(), m_cache[index].fCached);
		if(uiSize < m_cache[index].vBuffer.size()) uiSize = 0;
		else uiSize -= m_cache[index].vBuffer.size();
		m_cache.erase(index);
	}
	return uiSize;
}

std::string SolidCacheMemory::MakeKey(const std::string &sArchive) const
{
	std::string sKey;
	std::stringstream ss(sKey);
	__stat64 st;
// TODO: error check
	_stat64(sArchive.c_str(), &st);
	ss << sArchive << ':' << st.st_mtime << ':' << st.st_size;
	return ss.str();
}

void SolidCacheMemory::AccessArchive_(const char* archive)
{
	std::time_t atime;
	std::time(&atime);
	m_mAccess[MakeKey(archive)] = atime;
}

std::string SolidCacheMemory::GetLRUArchive_(const std::string& sExcludeArchive) const
{
	std::string sArchive = m_mAccess.begin()->first;
	std::time_t atime = m_mAccess.begin()->second;
	for(std::map<std::string, std::time_t>::const_iterator it = m_mAccess.begin(); it != m_mAccess.end(); ++it) {
		if(sArchive == sExcludeArchive || (it->first != sExcludeArchive && atime > it->second)) {
			sArchive = it->first;
			atime = it->second;
		}
	}
	return sArchive;
}

boost::uint64_t SolidCacheMemory::GetSize_() const
{
	boost::uint64_t nResult = 0;
	for(std::map<std::string, SolidFileCacheMemory::FileCache>::const_iterator it1 = m_mTable.begin(); it1 != m_mTable.end(); ++it1) {
		for(SolidFileCacheMemory::FileCache::const_iterator it2 = it1->second.begin(); it2 != it1->second.end(); ++it2) {
			nResult += it2->second.vBuffer.size();
		}
	}
	return nResult;
}

boost::uint64_t SolidCacheMemory::ReduceSize_(boost::uint64_t uiSize, void(*fCallback)(void*, const std::string&, unsigned int, void*, unsigned int, bool), void* pArg, const std::string& sExcludeArchive)
{
	OutputDebugPrintf("SolidCacheMemory::ReduceSize %" UINT64_S "u %s\n", uiSize, sExcludeArchive.c_str());
	while(uiSize > 0) {
		OutputDebugPrintf("SolidCacheMemory::ReduceSize trace %" UINT64_S "u %s\n", uiSize, sExcludeArchive.c_str());
		if(m_mTable.size() > 0) {
			std::string sArchive = GetLRUArchive_(sExcludeArchive);
			SolidFileCacheMemory scm = GetFileCache_(sArchive);
			if(scm.GetCount_() > 0) {
				Argument2 arg2 = { RestoreKey(sArchive), pArg };
				OutputDebugPrintf("SolidCacheMemory::ReduceSize calling scm.ReduceSize_()\n");
				uiSize = scm.ReduceSize_(uiSize, fCallback, &arg2);
			} else {
				m_mTable.erase(sArchive);
				m_mAccess.erase(sArchive);
			}
		}
	}
	return uiSize;
}
