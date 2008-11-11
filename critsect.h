//クリティカルセクションクラス
//
//コンストラクタ、デストラクタで
//クリティカルセクションの初期化、破棄を行う。
//
//クリティカルセクションに入るときはEnter()を呼び、
//出るときはLeave()を呼ぶこと。

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
