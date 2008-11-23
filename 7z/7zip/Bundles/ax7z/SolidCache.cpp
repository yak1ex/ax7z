#include "SolidCache.h"

std::map<std::string, SolidFileCache::FileCache> SolidCache::table;

SolidCache& SolidCache::GetInstance()
{
	static SolidCache scSingleton;
	return scSingleton;
}
