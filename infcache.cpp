#include "infcache.h"

InfoCache::InfoCache()
{
	nowno = 0;
	for (int i=0; i<INFOCACHE_MAX; i++) {
		arcinfo[i].hinfo = NULL;
	}
}

InfoCache::~InfoCache()
{
	for (int i=0; i<INFOCACHE_MAX; i++) {
		if (arcinfo[i].hinfo) LocalFree(arcinfo[i].hinfo);
	}
}

void InfoCache::Clear(void)
{
	cs.Enter();
	for (int i=0; i<INFOCACHE_MAX; i++) {
		if (arcinfo[i].hinfo) {
			LocalFree(arcinfo[i].hinfo);
			arcinfo[i].hinfo = NULL;
		}
	}
	nowno = 0;
	cs.Leave();
}

//�L���b�V���ǉ�
//filepath:�t�@�C���p�X
//ph:�n���h���ւ̃|�C���^
//INFOCACHE_MAX��������Â��̂͏���
void InfoCache::Add(char *filepath, HLOCAL *ph)
{
	cs.Enter();
	if (arcinfo[nowno].hinfo) LocalFree(arcinfo[nowno].hinfo);
	arcinfo[nowno].hinfo = *ph;
	lstrcpy(arcinfo[nowno].path, filepath);
	nowno = (nowno+1)%INFOCACHE_MAX;
	cs.Leave();
}

//�L���b�V���ɂ���΃n���h����Ԃ�
bool InfoCache::GetCache(char *filepath, HLOCAL *ph)
{
	bool ret = false;
	int no = nowno-1;
	if (no < 0) no = INFOCACHE_MAX -1;
	for (int i=0; i<INFOCACHE_MAX; i++) {
		if (arcinfo[no].hinfo == NULL) break;
		if (lstrcmpi(arcinfo[no].path, filepath) == 0) {
			*ph = arcinfo[no].hinfo;
			ret = true;
			break;
		}
		no--;
		if (no < 0) no = INFOCACHE_MAX -1;
	}
	return ret;
}

//�L���b�V���ɂ���΃R�s�[
//ph:�A�[�J�C�u�����󂯎��n���h���ւ̃|�C���^
//pinfo:�A�[�J�C�u�̃t�@�C�������󂯎��|�C���^
//		���炩���� pinfo �� filename �� position ���Z�b�g���Ă����B
//		�L���b�V��������� filename(position) �̈�v�������Ԃ��B
//�L���b�V���ɂȂ���΁ASPI_NO_FUNCTION ���Ԃ�B
//�L���b�V���ɂ���� SPI_ALL_RIGHT ���Ԃ�B
//�A�[�J�C�u���̓L���b�V���ɂ��邪�Afilename(position) ����v���Ȃ��ꍇ��
//SPI_NOT_SUPPORT ���Ԃ�B�G���[�̏ꍇ�̓G���[�R�[�h���Ԃ�B
int InfoCache::Dupli(char *filepath, HLOCAL *ph, fileInfo *pinfo)
{
	cs.Enter();
	HLOCAL hinfo;
	int ret = GetCache(filepath, &hinfo);

if (ret) {
	ret = SPI_ALL_RIGHT;
	if (ph != NULL) {
		UINT size = LocalSize(hinfo);
		/* �o�͗p�̃������̊��蓖�� */
		*ph = LocalAlloc(LMEM_FIXED, size);
		if (*ph == NULL) {
			ret = SPI_NO_MEMORY;
		} else {
			memcpy(*ph, (void*)hinfo, size);
		}
	} else {
		fileInfo *ptmp = (fileInfo *)hinfo;
		if (pinfo->filename[0] != '\0') {
			for (;;) {
				if (ptmp->method[0] == '\0') {
					ret = SPI_NOT_SUPPORT;
					break;
				}
				// complete path relative to archive root
				char path[sizeof(ptmp->path)+sizeof(ptmp->filename)];
				strcpy(path, ptmp->path);
				size_t len = strlen(path);
				if(len && path[len-1] != '/' && path[len-1] != '\\') // need delimiter
					strcat(path, "\\");
				strcat(path, ptmp->filename);
				if (lstrcmpi(path, pinfo->filename) == 0) break;
				ptmp++;
			}
		} else {
			for (;;) {
				if (ptmp->method[0] == '\0') {
					ret = SPI_NOT_SUPPORT;
					break;
				}
				if (ptmp->position == pinfo->position) break;
				ptmp++;
			}
		}
		if (ret == SPI_ALL_RIGHT) *pinfo = *ptmp;
	}
} else {
	ret = SPI_NO_FUNCTION;
}

	cs.Leave();
	return ret;
}

//////////////////////////////////////////
// ���C�h������
//////////////////////////////////////////
InfoCacheW::InfoCacheW()
{
	nowno = 0;
	for (int i=0; i<INFOCACHE_MAX; i++) {
		arcinfo[i].hinfo = NULL;
	}
}

InfoCacheW::~InfoCacheW()
{
	for (int i=0; i<INFOCACHE_MAX; i++) {
		if (arcinfo[i].hinfo) LocalFree(arcinfo[i].hinfo);
	}
}

void InfoCacheW::Clear(void)
{
	cs.Enter();
	for (int i=0; i<INFOCACHE_MAX; i++) {
		if (arcinfo[i].hinfo) {
			LocalFree(arcinfo[i].hinfo);
			arcinfo[i].hinfo = NULL;
		}
	}
	nowno = 0;
	cs.Leave();
}

//�L���b�V���ǉ�
//filepath:�t�@�C���p�X
//ph:�n���h���ւ̃|�C���^
//INFOCACHE_MAX��������Â��̂͏���
void InfoCacheW::Add(wchar_t *filepath, HLOCAL *ph)
{
	cs.Enter();
	if (arcinfo[nowno].hinfo) LocalFree(arcinfo[nowno].hinfo);
	arcinfo[nowno].hinfo = *ph;
	wcscpy(arcinfo[nowno].path, filepath);
	nowno = (nowno+1)%INFOCACHE_MAX;
	cs.Leave();
}

//�L���b�V���ɂ���΃n���h����Ԃ�
bool InfoCacheW::GetCache(wchar_t *filepath, HLOCAL *ph)
{
	bool ret = false;
	int no = nowno-1;
	if (no < 0) no = INFOCACHE_MAX -1;
	for (int i=0; i<INFOCACHE_MAX; i++) {
		if (arcinfo[no].hinfo == NULL) break;
		if (wcsicmp(arcinfo[no].path, filepath) == 0) {
			*ph = arcinfo[no].hinfo;
			ret = true;
			break;
		}
		no--;
		if (no < 0) no = INFOCACHE_MAX -1;
	}
	return ret;
}

//�L���b�V���ɂ���΃R�s�[
//ph:�A�[�J�C�u�����󂯎��n���h���ւ̃|�C���^
//pinfo:�A�[�J�C�u�̃t�@�C�������󂯎��|�C���^
//		���炩���� pinfo �� filename �� position ���Z�b�g���Ă����B
//		�L���b�V��������� filename(position) �̈�v�������Ԃ��B
//�L���b�V���ɂȂ���΁ASPI_NO_FUNCTION ���Ԃ�B
//�L���b�V���ɂ���� SPI_ALL_RIGHT ���Ԃ�B
//�A�[�J�C�u���̓L���b�V���ɂ��邪�Afilename(position) ����v���Ȃ��ꍇ��
//SPI_NOT_SUPPORT ���Ԃ�B�G���[�̏ꍇ�̓G���[�R�[�h���Ԃ�B
int InfoCacheW::Dupli(wchar_t *filepath, HLOCAL *ph, fileInfoW *pinfo)
{
	cs.Enter();
	HLOCAL hinfo;
	int ret = GetCache(filepath, &hinfo);

	if (ret) {
		ret = SPI_ALL_RIGHT;
		if (ph != NULL) {
			UINT size = LocalSize(hinfo);
			/* �o�͗p�̃������̊��蓖�� */
			*ph = LocalAlloc(LMEM_FIXED, size);
			if (*ph == NULL) {
				ret = SPI_NO_MEMORY;
			} else {
				memcpy(*ph, (void*)hinfo, size);
			}
		} else {
			fileInfoW *ptmp = (fileInfoW *)hinfo;
			if (pinfo->filename[0] != L'\0') {
				for (;;) {
					if (ptmp->method[0] == '\0') {
						ret = SPI_NOT_SUPPORT;
						break;
					}
					// complete path relative to archive root
					wchar_t path[sizeof(ptmp->path)+sizeof(ptmp->filename)];
					wcscpy(path, ptmp->path);
					size_t len = wcslen(path);
					if(len && path[len-1] != L'/' && path[len-1] != L'\\') // need delimiter
						wcscat(path, L"\\");
					wcscat(path, ptmp->filename);
					if (wcsicmp(path, pinfo->filename) == 0) break;
					ptmp++;
				}
			} else {
				for (;;) {
					if (ptmp->method[0] == '\0') {
						ret = SPI_NOT_SUPPORT;
						break;
					}
					if (ptmp->position == pinfo->position) break;
					ptmp++;
				}
			}
			if (ret == SPI_ALL_RIGHT) *pinfo = *ptmp;
		}
	} else {
		ret = SPI_NO_FUNCTION;
	}

	cs.Leave();
	return ret;
}
