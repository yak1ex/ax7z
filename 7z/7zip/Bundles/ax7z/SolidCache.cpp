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
	OutputDebugPrintf("SolidCache::Append %s %d %d bytes", sArchive.c_str(), index, size);
	if(m_scd.Exists(sArchive.c_str(), index)) {
		m_scd.Append(sArchive.c_str(), index, data, size);
		OutputDebugPrintf("SolidCache::Append:disk %s %d %d bytes", sArchive.c_str(), index, size);
	} else {
		m_scm.GetFileCache(sArchive).Append(index, data, size);
		OutputDebugPrintf("SolidCache::Append:memory %s %d %d bytes", sArchive.c_str(), index, size);
		if(m_scm.GetMaxMemory() >= 0 && (m_scm.GetSize() + size)/1024/1024 > m_scm.GetMaxMemory()) {
// TODO: Need to select which entries are purged to disk
// TODO: Reduce size
			m_scd.Append(sArchive.c_str(), index, m_scm.GetFileCache(sArchive).GetContent(index), m_scm.GetFileCache(sArchive).GetCurSize(index));
			m_scm.GetFileCache(sArchive).Remove(index);
			OutputDebugPrintf("SolidCache::Append:memory2disk %s %d %d", sArchive.c_str(), index, m_scm.GetFileCache(sArchive).GetCurSize(index));
		}
	}
}

int SolidCacheMemory::GetSize() const
{
	int nResult = 0;
	for(std::map<std::string, SolidFileCacheMemory::FileCache>::const_iterator it1 = m_mTable.begin(); it1 != m_mTable.end(); ++it1) {
		for(SolidFileCacheMemory::FileCache::const_iterator it2 = it1->second.begin(); it2 != it1->second.end(); ++it2) {
			nResult += it2->second.vBuffer.size();
		}
	}
	return nResult;
}