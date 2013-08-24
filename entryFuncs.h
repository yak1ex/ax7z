#ifndef spi00am_h
#define spi00am_h

#define NOMINMAX
#include <windows.h>
#undef NOMINMAX

/*-------------------------------------------------------------------------*/
// ファイル情報構造体
/*-------------------------------------------------------------------------*/
#pragma pack(push)
#pragma pack(1) //構造体のメンバ境界を1バイトにする
typedef struct fileInfo
{
	unsigned char method[8];	/* 圧縮法の種類 */
	unsigned long position;		/* ファイル上での位置 */
	unsigned long compsize;		/* 圧縮されたサイズ */
	unsigned long filesize;		/* 元のファイルサイズ */
	long /*time_t*/ timestamp;	/* ファイルの更新日時 */
	char path[200];				/* 相対パス */
	char filename[200];			/* ファイルネーム */
	unsigned long crc;			/* CRC */
} fileInfo;
typedef struct fileInfoW
{
	unsigned char method[8];	/* 圧縮法の種類 */
	unsigned long position;		/* ファイル上での位置 */
	unsigned long compsize;		/* 圧縮されたサイズ */
	unsigned long filesize;		/* 元のファイルサイズ */
	long /*time_t*/ timestamp;	/* ファイルの更新日時 */
	wchar_t path[200];				/* 相対パス */
	wchar_t filename[200];			/* ファイルネーム */
	unsigned long crc;			/* CRC */
} fileInfoW;
#pragma pack(pop)

/*-------------------------------------------------------------------------*/
// エラーコード
/*-------------------------------------------------------------------------*/
#define SPI_NO_FUNCTION			-1	/* その機能はインプリメントされていない */
#define SPI_ALL_RIGHT			0	/* 正常終了 */
#define SPI_ABORT				1	/* コールバック関数が非0を返したので展開を中止した */
#define SPI_NOT_SUPPORT			2	/* 未知のフォーマット */
#define SPI_OUT_OF_ORDER		3	/* データが壊れている */
#define SPI_NO_MEMORY			4	/* メモリーが確保出来ない */
#define SPI_MEMORY_ERROR		5	/* メモリーエラー */
#define SPI_FILE_READ_ERROR		6	/* ファイルリードエラー */
#define	SPI_WINDOW_ERROR		7	/* 窓が開けない (非公開のエラーコード) */
#define SPI_OTHER_ERROR			8	/* 内部エラー */
#define	SPI_FILE_WRITE_ERROR	9	/* 書き込みエラー (非公開のエラーコード) */
#define	SPI_END_OF_FILE			10	/* ファイル終端 (非公開のエラーコード) */

/*-------------------------------------------------------------------------*/
// '00AM'関数のプロトタイプ宣言
/*-------------------------------------------------------------------------*/
// Wrong calling convention
typedef int (PASCAL *SPI_PROGRESS)(int nNum,int nDenom,long lData);
//typedef int (CALLBACK *SPI_PROGRESS)(int, int, long);
typedef void (CALLBACK *SPI_OnWriteCallback)(const void *data, UINT32 size, UINT32 processed, const fileInfoW* pFileInfo, UCHAR* pbStop);
extern "C" {
	int __declspec(dllexport) __stdcall GetPluginInfo
			(int infono, LPSTR buf, int buflen);
	int __declspec(dllexport) __stdcall IsSupported(LPSTR filename, DWORD dw);
	int __declspec(dllexport) __stdcall IsSupportedW(LPWSTR filename, DWORD dw);
	int __declspec(dllexport) __stdcall GetArchiveInfo(
			LPSTR buf, long len, unsigned int flag, HLOCAL *lphInf);
	int __declspec(dllexport) __stdcall GetArchiveInfoW(
			LPWSTR buf, long len, unsigned int flag, HLOCAL *lphInf);
	int __declspec(dllexport) __stdcall GetFileInfo(LPSTR buf,long len,
			LPSTR filename, unsigned int flag, fileInfo *lpInfo);
	int __declspec(dllexport) __stdcall GetFileInfoW(LPWSTR buf,long len,
			LPWSTR filename, unsigned int flag, fileInfoW *lpInfo);
	int __declspec(dllexport) __stdcall GetFile(LPSTR src,long len,
			   LPSTR dest, unsigned int flag,
			   SPI_PROGRESS prgressCallback, long lData);
	int __declspec(dllexport) __stdcall GetFileW(LPWSTR src,long len,
			   LPWSTR dest, unsigned int flag,
			   SPI_PROGRESS prgressCallback, long lData);
	int __declspec(dllexport) __stdcall ConfigurationDlg(HWND parent, int fnc);
	int __declspec(dllexport) __stdcall ExtractSolidArchive(LPCWSTR filename, SPI_OnWriteCallback pCallback);
}

int GetArchiveInfoEx(LPSTR filename, long len, HLOCAL *lphInf);
int GetFileEx(char *filename, HLOCAL *dest, const char* pOutFile, fileInfo *pinfo,
			SPI_PROGRESS lpPrgressCallback, long lData);
int GetArchiveInfoWEx(LPWSTR filename, long len, HLOCAL *lphInf);
int GetFileWEx(wchar_t *filename, HLOCAL *dest, const wchar_t* pOutFile, fileInfoW *pinfo,
			SPI_PROGRESS lpPrgressCallback, long lData);
int ExtractSolidArchiveEx(LPCWSTR filename, SPI_OnWriteCallback pCallback);
#endif
