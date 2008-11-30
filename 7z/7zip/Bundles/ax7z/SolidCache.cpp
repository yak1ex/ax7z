#include "SolidCache.h"

std::map<std::string, SolidFileCacheMemory::FileCache> SolidCacheMemory::table;

SolidCacheMemory& SolidCacheMemory::GetInstance()
{
	static SolidCacheMemory scSingleton;
	return scSingleton;
}
