//�N���e�B�J���Z�N�V�����N���X
//
//�R���X�g���N�^�A�f�X�g���N�^��
//�N���e�B�J���Z�N�V�����̏������A�j�����s���B
//
//�N���e�B�J���Z�N�V�����ɓ���Ƃ���Enter()���ĂсA
//�o��Ƃ���Leave()���ĂԂ��ƁB

#ifndef critsect_h
#define critsect_h

#include <windows.h>

class CriticalSection
{
public:
	CriticalSection()
	{
		InitializeCriticalSection(&crit_sect);
	};
	~CriticalSection()
	{
		DeleteCriticalSection(&crit_sect);
	};
	void Enter()
	{
		EnterCriticalSection(&crit_sect);
	};
	void Leave()
	{
		LeaveCriticalSection(&crit_sect);
	};
private:
	CRITICAL_SECTION crit_sect;
};

#endif
