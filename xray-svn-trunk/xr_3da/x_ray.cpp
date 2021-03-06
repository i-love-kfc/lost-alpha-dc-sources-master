//-----------------------------------------------------------------------------
// File: x_ray.cpp
//
// Programmers:
//	Oles		- Oles Shishkovtsov
//	AlexMX		- Alexander Maksimchuk
//-----------------------------------------------------------------------------
#include "stdafx.h"
#include "igame_level.h"
#include "igame_persistent.h"

#include "dedicated_server_only.h"
#include "no_single.h"
#include "../xrNetServer/NET_AuthCheck.h"

#include "xr_input.h"
#include "xr_ioconsole.h"
#include "x_ray.h"
#include "std_classes.h"
#include "GameFont.h"
#include "resource.h"
#include "LightAnimLibrary.h"
#include "../xrcdb/ispatial.h"
#include "CopyProtection.h"
#include "Text_Console.h"
#include <process.h>
#include <locale.h>

#include "securom_api.h"
//---------------------------------------------------------------------
ENGINE_API CInifile* pGameIni		= NULL;
BOOL	g_bIntroFinished			= FALSE;
extern	void	Intro				( void* fn );
extern	void	Intro_DSHOW			( void* fn );
extern	int PASCAL IntroDSHOW_wnd	(HINSTANCE hInstC, HINSTANCE hInstP, LPSTR lpCmdLine, int nCmdShow);
//int		max_load_stage = 0;

// computing build id
XRCORE_API	LPCSTR	build_date;
XRCORE_API	u32		build_id;

#ifdef MASTER_GOLD
#	define NO_MULTI_INSTANCES
#endif // #ifdef MASTER_GOLD


static LPSTR month_id[12] = {
	"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"
};

static int days_in_month[12] = {
	31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
};

static int start_day	= 31;	// 31
static int start_month	= 1;	// January
static int start_year	= 1999;	// 1999

// binary hash, mainly for copy-protection

#ifndef DEDICATED_SERVER

#include "../xr_3da/xrGame/xrGameSpy/gamespy/md5c.c"
#include <ctype.h>

#define DEFAULT_MODULE_HASH "3CAABCFCFF6F3A810019C6A72180F166"
static char szEngineHash[33] = DEFAULT_MODULE_HASH;

PROTECT_API char * ComputeModuleHash( char * pszHash )
{
	//SECUROM_MARKER_HIGH_SECURITY_ON(3)

	char szModuleFileName[ MAX_PATH ];
	HANDLE hModuleHandle = NULL , hFileMapping = NULL;
	LPVOID lpvMapping = NULL;
	MEMORY_BASIC_INFORMATION MemoryBasicInformation;

	if ( ! GetModuleFileName( NULL , szModuleFileName , MAX_PATH ) )
		return pszHash;

	hModuleHandle = CreateFile( szModuleFileName , GENERIC_READ , FILE_SHARE_READ , NULL , OPEN_EXISTING , 0 , NULL );

	if ( hModuleHandle == INVALID_HANDLE_VALUE )
		return pszHash;

	hFileMapping = CreateFileMapping( hModuleHandle , NULL , PAGE_READONLY , 0 , 0 , NULL );

	if ( hFileMapping == NULL ) {
		CloseHandle( hModuleHandle );
		return pszHash;
	}

	lpvMapping = MapViewOfFile( hFileMapping , FILE_MAP_READ , 0 , 0 , 0 );

	if ( lpvMapping == NULL ) {
		CloseHandle( hFileMapping );
		CloseHandle( hModuleHandle );
		return pszHash;
	}

	ZeroMemory( &MemoryBasicInformation , sizeof( MEMORY_BASIC_INFORMATION ) );

	VirtualQuery( lpvMapping , &MemoryBasicInformation , sizeof( MEMORY_BASIC_INFORMATION ) );

	if ( MemoryBasicInformation.RegionSize ) {
		char szHash[33];
		MD5Digest( ( unsigned char *)lpvMapping , (unsigned int) MemoryBasicInformation.RegionSize , szHash );
		MD5Digest( ( unsigned char *)szHash , 32 , pszHash );
		for ( int i = 0 ; i < 32 ; ++i )
			pszHash[ i ] = (char)toupper( pszHash[ i ] );
	}

	UnmapViewOfFile( lpvMapping );
	CloseHandle( hFileMapping );
	CloseHandle( hModuleHandle );

	//SECUROM_MARKER_HIGH_SECURITY_OFF(3)

	return pszHash;
}
#endif // #ifdef NDEBUG

void compute_build_id	()
{
	build_date			= __DATE__;

	int					days;
	int					months = 0;
	int					years;
	string16			month;
	string256			buffer;
	xr_strcpy				(buffer,__DATE__);
	sscanf				(buffer,"%s %d %d",month,&days,&years);

	for (int i=0; i<12; i++) {
		if (_stricmp(month_id[i],month))
			continue;

		months			= i;
		break;
	}

	build_id			= (years - start_year)*365 + days - start_day;

	for (int i=0; i<months; ++i)
		build_id		+= days_in_month[i];

	for (int i=0; i<start_month-1; ++i)
		build_id		-= days_in_month[i];


}
//---------------------------------------------------------------------
// 2446363
// umbt@ukr.net
//////////////////////////////////////////////////////////////////////////
struct _SoundProcessor	: public pureFrame
{
	virtual void	_BCL	OnFrame	( )
	{
		//Msg							("------------- sound: %d [%3.2f,%3.2f,%3.2f]",u32(Device.dwFrame),VPUSH(Device.vCameraPosition));
		Device.Statistic->Sound.Begin();
		::Sound->update				(Device.vCameraPosition,Device.vCameraDirection,Device.vCameraTop);
		Device.Statistic->Sound.End	();
	}
}	SoundProcessor;

//////////////////////////////////////////////////////////////////////////
// global variables
ENGINE_API	CApplication*	pApp			= NULL;
static		HWND			logoWindow		= NULL;

			int				doLauncher		();
			void			doBenchmark		(LPCSTR name);
ENGINE_API	bool			g_bBenchmark	= false;
string512	g_sBenchmarkName;


ENGINE_API	string512		g_sLaunchOnExit_params;
ENGINE_API	string512		g_sLaunchOnExit_app;
ENGINE_API	string_path		g_sLaunchWorkingFolder;
// -------------------------------------------
// startup point
void InitEngine		()
{
	Engine.Initialize			( );
	while (!g_bIntroFinished)	Sleep	(100);
	Device.Initialize			( );
	CheckCopyProtection			( );
}

struct path_excluder_predicate
{
	explicit path_excluder_predicate(xr_auth_strings_t const * ignore) :
		m_ignore(ignore)
	{
	}
	bool xr_stdcall is_allow_include(LPCSTR path)
	{
		if (!m_ignore)
			return true;
		
		return allow_to_include_path(*m_ignore, path);
	}
	xr_auth_strings_t const *	m_ignore;
};

PROTECT_API void InitSettings	()
{
	#ifndef DEDICATED_SERVER
		Msg( "EH: %s\n" , ComputeModuleHash( szEngineHash ) );
	#endif // DEDICATED_SERVER

	string_path					fname; 
	FS.update_path				(fname,"$game_config$","system.ltx");
#ifdef DEBUG
	Msg							("Updated path to system.ltx is %s", fname);
#endif // #ifdef DEBUG
	pSettings					= new CInifile(fname,TRUE);
	CHECK_OR_EXIT				(0!=pSettings->section_count(), make_string("Cannot find file %s.\nReinstalling application may fix this problem.",fname));

	xr_auth_strings_t			tmp_ignore_pathes;
	xr_auth_strings_t			tmp_check_pathes;
	fill_auth_check_params		(tmp_ignore_pathes, tmp_check_pathes);
	
	path_excluder_predicate			tmp_excluder(&tmp_ignore_pathes);
	CInifile::allow_include_func_t	tmp_functor;
	tmp_functor.bind(&tmp_excluder, &path_excluder_predicate::is_allow_include);
	pSettingsAuth					= new CInifile(
		fname,
		TRUE,
		TRUE,
		FALSE,
		0,
		tmp_functor
	);

	FS.update_path				(fname,"$game_config$","game.ltx");
	pGameIni					= new CInifile(fname,TRUE);
	CHECK_OR_EXIT				(0!=pGameIni->section_count(), make_string("Cannot find file %s.\nReinstalling application may fix this problem.",fname));
}
PROTECT_API void InitConsole	()
{
	//SECUROM_MARKER_SECURITY_ON(5)

#ifdef DEDICATED_SERVER
	{
		Console						= new CTextConsole();		
	}
#else
	//	else
	{
		Console						= new CConsole();
	}
#endif
	Console->Initialize			( );

	xr_strcpy						(Console->ConfigFile,"user.ltx");
	if (strstr(Core.Params,"-ltx ")) {
		string64				c_name;
		sscanf					(strstr(Core.Params,"-ltx ")+5,"%[^ ] ",c_name);
		xr_strcpy					(Console->ConfigFile,c_name);
	}

	//SECUROM_MARKER_SECURITY_OFF(5)
}

PROTECT_API void InitInput		()
{
	BOOL bCaptureInput			= !strstr(Core.Params,"-i");

	pInput						= new CInput(bCaptureInput);
}
void destroyInput	()
{
	xr_delete					( pInput		);
}

PROTECT_API void InitSound1		()
{
	CSound_manager_interface::_create				(0);
}

PROTECT_API void InitSound2		()
{
	CSound_manager_interface::_create				(1);
}

void destroySound	()
{
	CSound_manager_interface::_destroy				( );
}

void destroySettings()
{
	CInifile** s				= (CInifile**)(&pSettings);
	xr_delete					( *s		);
	xr_delete					( pGameIni		);
}

void destroyConsole	()
{
	Console->Execute			("cfg_save");
	Console->Destroy			();
	xr_delete					(Console);
}

void destroyEngine	()
{
	Device.Destroy				( );
	Engine.Destroy				( );
}

void execUserScript				( )
{
	Console->Execute			("unbindall");
	Console->ExecuteScript		(Console->ConfigFile);
}

void slowdownthread	( void* )
{
//	Sleep		(30*1000);
	for (;;)	{
		if (Device.Statistic->fFPS<30) Sleep(1);
		if (Device.mt_bMustExit)	return;
		if (0==pSettings)			return;
		if (0==Console)				return;
		if (0==pInput)				return;
		if (0==pApp)				return;
	}
}
void CheckPrivilegySlowdown		( )
{
#ifdef DEBUG
	if	(strstr(Core.Params,"-slowdown"))	{
		thread_spawn(slowdownthread,"slowdown",0,0);
	}
	if	(strstr(Core.Params,"-slowdown2x"))	{
		thread_spawn(slowdownthread,"slowdown",0,0);
		thread_spawn(slowdownthread,"slowdown",0,0);
	}
#endif // DEBUG
}

void Startup()
{
	InitSound1		();
	execUserScript	();
	InitSound2		();

	// ...command line for auto start
	{
		LPCSTR	pStartup			= strstr				(Core.Params,"-start ");
		if (pStartup)				Console->Execute		(pStartup+1);
	}
	{
		LPCSTR	pStartup			= strstr				(Core.Params,"-load ");
		if (pStartup)				Console->Execute		(pStartup+1);
	}

	// Initialize APP
//#ifndef DEDICATED_SERVER
	ShowWindow( Device.m_hWnd , SW_SHOWNORMAL );
	Device.Create				( );
//#endif
	LALib.OnCreate				( );
	pApp						= new CApplication();
	g_pGamePersistent			= (IGame_Persistent*)	NEW_INSTANCE (CLSID_GAME_PERSISTANT);
	g_SpatialSpace				= new ISpatial_DB();
	g_SpatialSpacePhysic		= new ISpatial_DB();
	
	// Destroy LOGO
	DestroyWindow				(logoWindow);
	logoWindow					= NULL;

	// Main cycle
	CheckCopyProtection			( );
	Memory.mem_usage();
	Device.Run					( );

	// Destroy APP
	xr_delete					( g_SpatialSpacePhysic	);
	xr_delete					( g_SpatialSpace		);
	DEL_INSTANCE				( g_pGamePersistent		);
	xr_delete					( pApp					);
	Engine.Event.Dump			( );

	// Destroying
//.	destroySound();
	destroyInput();

	if(!g_bBenchmark)
		destroySettings();

	LALib.OnDestroy				( );
	
	if(!g_bBenchmark)
		destroyConsole();
	else
		Console->Destroy();

	destroySound();

	destroyEngine();
}

static BOOL CALLBACK logDlgProc( HWND hw, UINT msg, WPARAM wp, LPARAM lp )
{
	switch( msg ){
		case WM_DESTROY:
			break;
		case WM_CLOSE:
			DestroyWindow( hw );
			break;
		case WM_COMMAND:
			if( LOWORD(wp)==IDCANCEL )
				DestroyWindow( hw );
			break;
		default:
			return FALSE;
	}
	return TRUE;
}
/*
void	test_rtc	()
{
	CStatTimer		tMc,tM,tC,tD;
	u32				bytes=0;
	tMc.FrameStart	();
	tM.FrameStart	();
	tC.FrameStart	();
	tD.FrameStart	();
	::Random.seed	(0x12071980);
	for		(u32 test=0; test<10000; test++)
	{
		u32			in_size			= ::Random.randI(1024,256*1024);
		u32			out_size_max	= rtc_csize		(in_size);
		u8*			p_in			= xr_alloc<u8>	(in_size);
		u8*			p_in_tst		= xr_alloc<u8>	(in_size);
		u8*			p_out			= xr_alloc<u8>	(out_size_max);
		for (u32 git=0; git<in_size; git++)			p_in[git] = (u8)::Random.randI	(8);	// garbage
		bytes		+= in_size;

		tMc.Begin	();
		memcpy		(p_in_tst,p_in,in_size);
		tMc.End		();

		tM.Begin	();
		CopyMemory(p_in_tst,p_in,in_size);
		tM.End		();

		tC.Begin	();
		u32			out_size		= rtc_compress	(p_out,out_size_max,p_in,in_size);
		tC.End		();

		tD.Begin	();
		u32			in_size_tst		= rtc_decompress(p_in_tst,in_size,p_out,out_size);
		tD.End		();

		// sanity check
		R_ASSERT	(in_size == in_size_tst);
		for (u32 tit=0; tit<in_size; tit++)			R_ASSERT(p_in[tit] == p_in_tst[tit]);	// garbage

		xr_free		(p_out);
		xr_free		(p_in_tst);
		xr_free		(p_in);
	}
	tMc.FrameEnd	();	float rMc		= 1000.f*(float(bytes)/tMc.result)/(1024.f*1024.f);
	tM.FrameEnd		(); float rM		= 1000.f*(float(bytes)/tM.result)/(1024.f*1024.f);
	tC.FrameEnd		(); float rC		= 1000.f*(float(bytes)/tC.result)/(1024.f*1024.f);
	tD.FrameEnd		(); float rD		= 1000.f*(float(bytes)/tD.result)/(1024.f*1024.f);
	Msg				("* memcpy:        %5.2f M/s (%3.1f%%)",rMc,100.f*rMc/rMc);
	Msg				("* mm-memcpy:     %5.2f M/s (%3.1f%%)",rM,100.f*rM/rMc);
	Msg				("* compression:   %5.2f M/s (%3.1f%%)",rC,100.f*rC/rMc);
	Msg				("* decompression: %5.2f M/s (%3.1f%%)",rD,100.f*rD/rMc);
}
*/
extern void	testbed	(void);

// video
/*
static	HINSTANCE	g_hInstance		;
static	HINSTANCE	g_hPrevInstance	;
static	int			g_nCmdShow		;
void	__cdecl		intro_dshow_x	(void*)
{
	IntroDSHOW_wnd		(g_hInstance,g_hPrevInstance,"GameData\\Stalker_Intro.avi",g_nCmdShow);
	g_bIntroFinished	= TRUE	;
}
*/
#define dwStickyKeysStructSize sizeof( STICKYKEYS )
#define dwFilterKeysStructSize sizeof( FILTERKEYS )
#define dwToggleKeysStructSize sizeof( TOGGLEKEYS )

struct damn_keys_filter {
	BOOL bScreenSaverState;

	// Sticky & Filter & Toggle keys

	STICKYKEYS StickyKeysStruct;
	FILTERKEYS FilterKeysStruct;
	TOGGLEKEYS ToggleKeysStruct;

	DWORD dwStickyKeysFlags;
	DWORD dwFilterKeysFlags;
	DWORD dwToggleKeysFlags;

	damn_keys_filter	()
	{
		// Screen saver stuff

		bScreenSaverState = FALSE;

		// Saveing current state
		SystemParametersInfo( SPI_GETSCREENSAVEACTIVE , 0 , ( PVOID ) &bScreenSaverState , 0 );

		if ( bScreenSaverState )
			// Disable screensaver
			SystemParametersInfo( SPI_SETSCREENSAVEACTIVE , FALSE , NULL , 0 );

		dwStickyKeysFlags = 0;
		dwFilterKeysFlags = 0;
		dwToggleKeysFlags = 0;


		ZeroMemory( &StickyKeysStruct , dwStickyKeysStructSize );
		ZeroMemory( &FilterKeysStruct , dwFilterKeysStructSize );
		ZeroMemory( &ToggleKeysStruct , dwToggleKeysStructSize );

		StickyKeysStruct.cbSize = dwStickyKeysStructSize;
		FilterKeysStruct.cbSize = dwFilterKeysStructSize;
		ToggleKeysStruct.cbSize = dwToggleKeysStructSize;

		// Saving current state
		SystemParametersInfo( SPI_GETSTICKYKEYS , dwStickyKeysStructSize , ( PVOID ) &StickyKeysStruct , 0 );
		SystemParametersInfo( SPI_GETFILTERKEYS , dwFilterKeysStructSize , ( PVOID ) &FilterKeysStruct , 0 );
		SystemParametersInfo( SPI_GETTOGGLEKEYS , dwToggleKeysStructSize , ( PVOID ) &ToggleKeysStruct , 0 );

		if ( StickyKeysStruct.dwFlags & SKF_AVAILABLE ) {
			// Disable StickyKeys feature
			dwStickyKeysFlags = StickyKeysStruct.dwFlags;
			StickyKeysStruct.dwFlags = 0;
			SystemParametersInfo( SPI_SETSTICKYKEYS , dwStickyKeysStructSize , ( PVOID ) &StickyKeysStruct , 0 );
		}

		if ( FilterKeysStruct.dwFlags & FKF_AVAILABLE ) {
			// Disable FilterKeys feature
			dwFilterKeysFlags = FilterKeysStruct.dwFlags;
			FilterKeysStruct.dwFlags = 0;
			SystemParametersInfo( SPI_SETFILTERKEYS , dwFilterKeysStructSize , ( PVOID ) &FilterKeysStruct , 0 );
		}

		if ( ToggleKeysStruct.dwFlags & TKF_AVAILABLE ) {
			// Disable FilterKeys feature
			dwToggleKeysFlags = ToggleKeysStruct.dwFlags;
			ToggleKeysStruct.dwFlags = 0;
			SystemParametersInfo( SPI_SETTOGGLEKEYS , dwToggleKeysStructSize , ( PVOID ) &ToggleKeysStruct , 0 );
		}
	}

	~damn_keys_filter	()
	{
		if ( bScreenSaverState )
			// Restoring screen saver
			SystemParametersInfo( SPI_SETSCREENSAVEACTIVE , TRUE , NULL , 0 );

		if ( dwStickyKeysFlags) {
			// Restore StickyKeys feature
			StickyKeysStruct.dwFlags = dwStickyKeysFlags;
			SystemParametersInfo( SPI_SETSTICKYKEYS , dwStickyKeysStructSize , ( PVOID ) &StickyKeysStruct , 0 );
		}

		if ( dwFilterKeysFlags ) {
			// Restore FilterKeys feature
			FilterKeysStruct.dwFlags = dwFilterKeysFlags;
			SystemParametersInfo( SPI_SETFILTERKEYS , dwFilterKeysStructSize , ( PVOID ) &FilterKeysStruct , 0 );
		}

		if ( dwToggleKeysFlags ) {
			// Restore FilterKeys feature
			ToggleKeysStruct.dwFlags = dwToggleKeysFlags;
			SystemParametersInfo( SPI_SETTOGGLEKEYS , dwToggleKeysStructSize , ( PVOID ) &ToggleKeysStruct , 0 );
		}

	}
};

#undef dwStickyKeysStructSize
#undef dwFilterKeysStructSize
#undef dwToggleKeysStructSize

// ?????? ??? ????? ?????????? THQ ? ????? ???????????? ?????????????
BOOL IsOutOfVirtualMemory()
{
#define VIRT_ERROR_SIZE 256
#define VIRT_MESSAGE_SIZE 512

	//SECUROM_MARKER_HIGH_SECURITY_ON(1)

	MEMORYSTATUSEX statex;
	DWORD dwPageFileInMB = 0;
	DWORD dwPhysMemInMB = 0;
	HINSTANCE hApp = 0;
	char	pszError[ VIRT_ERROR_SIZE ];
	char	pszMessage[ VIRT_MESSAGE_SIZE ];

	ZeroMemory( &statex , sizeof( MEMORYSTATUSEX ) );
	statex.dwLength = sizeof( MEMORYSTATUSEX );
	
	if ( ! GlobalMemoryStatusEx( &statex ) )
		return 0;

	dwPageFileInMB = ( DWORD ) ( statex.ullTotalPageFile / ( 1024 * 1024 ) ) ;
	dwPhysMemInMB = ( DWORD ) ( statex.ullTotalPhys / ( 1024 * 1024 ) ) ;

	// ???????? ?????????? ???????
	if ( ( dwPhysMemInMB > 500 ) && ( ( dwPageFileInMB + dwPhysMemInMB ) > 2500  ) )
		return 0;

	hApp = GetModuleHandle( NULL );

	if ( ! LoadString( hApp , RC_VIRT_MEM_ERROR , pszError , VIRT_ERROR_SIZE ) )
		return 0;
 
	if ( ! LoadString( hApp , RC_VIRT_MEM_TEXT , pszMessage , VIRT_MESSAGE_SIZE ) )
		return 0;

	MessageBox( NULL , pszMessage , pszError , MB_OK | MB_ICONHAND );

	//SECUROM_MARKER_HIGH_SECURITY_OFF(1)

	return 1;
}

#include "xr_ioc_cmd.h"

typedef void DUMMY_STUFF (const void*,const u32&,void*);
XRCORE_API DUMMY_STUFF	*g_temporary_stuff;

#define TRIVIAL_ENCRYPTOR_DECODER
#include "trivial_encryptor.h"

//#define RUSSIAN_BUILD

#if 0
void foo	()
{
	typedef std::map<int,int>	TEST_MAP;
	TEST_MAP					temp;
	temp.insert					(std::make_pair(0,0));
	TEST_MAP::const_iterator	I = temp.upper_bound(2);
	if (I == temp.end())
		OutputDebugString		("end() returned\r\n");
	else
		OutputDebugString		("last element returned\r\n");

	typedef void*	pvoid;

	LPCSTR			path = "d:\\network\\stalker_net2";
	FILE			*f = fopen(path,"rb");
	int				file_handle = _fileno(f);
	u32				buffer_size = _filelength(file_handle);
	pvoid			buffer = xr_malloc(buffer_size);
	size_t			result = fread(buffer,buffer_size,1,f);
	R_ASSERT3		(!buffer_size || (result && (buffer_size >= result)),"Cannot read from file",path);
	fclose			(f);

	u32				compressed_buffer_size = rtc_csize(buffer_size);
	pvoid			compressed_buffer = xr_malloc(compressed_buffer_size);
	u32				compressed_size = rtc_compress(compressed_buffer,compressed_buffer_size,buffer,buffer_size);

	LPCSTR			compressed_path = "d:\\network\\stalker_net2.rtc";
	FILE			*f1 = fopen(compressed_path,"wb");
	fwrite			(compressed_buffer,compressed_size,1,f1);
	fclose			(f1);
}
#endif // 0

ENGINE_API	bool g_dedicated_server	= false;

#ifndef DEDICATED_SERVER
	// forward declaration for Parental Control checks
//	BOOL IsPCAccessAllowed(); 
#endif // DEDICATED_SERVER

int APIENTRY WinMain_impl(HINSTANCE hInstance,
                     HINSTANCE hPrevInstance,
                     char *    lpCmdLine,
                     int       nCmdShow)
{
#ifdef DEDICATED_SERVER
	Debug._initialize			(true);
#else // DEDICATED_SERVER
	Debug._initialize			(false);
#endif // DEDICATED_SERVER

	if (!IsDebuggerPresent()) {

		HMODULE const kernel32	= LoadLibrary("kernel32.dll");
		R_ASSERT				(kernel32);

		typedef BOOL (__stdcall*HeapSetInformation_type) (HANDLE, HEAP_INFORMATION_CLASS, PVOID, SIZE_T);
		HeapSetInformation_type const heap_set_information = 
			(HeapSetInformation_type)GetProcAddress(kernel32, "HeapSetInformation");
		if (heap_set_information) {
			ULONG HeapFragValue	= 2;
#ifdef DEBUG
			BOOL const result	= 
#endif // #ifdef DEBUG
				heap_set_information(
					GetProcessHeap(),
					HeapCompatibilityInformation,
					&HeapFragValue,
					sizeof(HeapFragValue)
				);
			VERIFY2				(result, "can't set process heap low fragmentation");
		}
	}

//	foo();
#ifndef DEDICATED_SERVER

	// Check for virtual memory
	if ( ( strstr( lpCmdLine , "--skipmemcheck" ) == NULL ) && IsOutOfVirtualMemory() )
		return 0;
/*
	// Parental Control for Vista and upper
	if ( ! IsPCAccessAllowed() ) {
		MessageBox( NULL , "Access restricted" , "Parental Control" , MB_OK | MB_ICONERROR );
		return 1;
	}
*/
	// Check for another instance
//#ifdef NO_MULTI_INSTANCES
	#define STALKER_PRESENCE_MUTEX "STALKER-SoC"
	
	HANDLE hCheckPresenceMutex = INVALID_HANDLE_VALUE;
	hCheckPresenceMutex = OpenMutex( READ_CONTROL , FALSE ,  STALKER_PRESENCE_MUTEX );
	if ( hCheckPresenceMutex == NULL ) {
		// New mutex
		hCheckPresenceMutex = CreateMutex( NULL , FALSE , STALKER_PRESENCE_MUTEX );
		if ( hCheckPresenceMutex == NULL )
			// Shit happens
			return 2;
	} else {
		// Already running
		CloseHandle( hCheckPresenceMutex );
		return 1;
	}
//#endif
#else // DEDICATED_SERVER
	g_dedicated_server			= true;
#endif // DEDICATED_SERVER

	// u3: DON'T do this, system will handle it for itself much better :)
	//
	// MSDN: Setting thread affinity should generally be avoided, 
	// because it can interfere with the scheduler's ability to schedule threads 
	// effectively across processors. This can decrease the performance gains 
	// produced by parallel processing.
	//
	// SetThreadAffinityMask		(GetCurrentThread(),1);

	// Title window
	logoWindow					= CreateDialog(GetModuleHandle(NULL),	MAKEINTRESOURCE(IDD_STARTUP), 0, logDlgProc );
	
	HWND logoPicture			= GetDlgItem(logoWindow, IDC_STATIC_LOGO);
	RECT logoRect;
	GetWindowRect(logoPicture, &logoRect);

	SetWindowPos				(
		logoWindow,
#ifndef DEBUG
		HWND_TOPMOST,
#else
		HWND_NOTOPMOST,
#endif // NDEBUG
		0,
		0,
		logoRect.right - logoRect.left,
		logoRect.bottom - logoRect.top,
		SWP_NOMOVE | SWP_SHOWWINDOW// | SWP_NOSIZE
	);
	UpdateWindow(logoWindow);
 
	// AVI
	g_bIntroFinished			= TRUE;

	g_sLaunchOnExit_app[0]		= NULL;
	g_sLaunchOnExit_params[0]	= NULL;

	LPCSTR						fsgame_ltx_name = "-fsltx ";
	string_path					fsgame = "";
	//MessageBox(0, lpCmdLine, "my cmd string", MB_OK);
	if (strstr(lpCmdLine, fsgame_ltx_name)) {
		int						sz = xr_strlen(fsgame_ltx_name);
		sscanf					(strstr(lpCmdLine,fsgame_ltx_name)+sz,"%[^ ] ",fsgame);
		//MessageBox(0, fsgame, "using fsltx", MB_OK);
	}

	g_temporary_stuff			= &trivial_encryptor::decode;
	
	compute_build_id			();
	Core._initialize			("xray",NULL, TRUE, fsgame[0] ? fsgame : NULL);
	InitSettings				();

	// Adjust player & computer name for Asian
	if ( pSettings->line_exist( "string_table" , "no_native_input" ) ) {
			xr_strcpy( Core.UserName , sizeof( Core.UserName ) , "Player" );
			xr_strcpy( Core.CompName , sizeof( Core.CompName ) , "Computer" );
	}

#ifndef DEDICATED_SERVER
	{
		damn_keys_filter		filter;
		(void)filter;
#endif // DEDICATED_SERVER

		FPU::m24r				();
		InitEngine				();

		InitInput				();

		InitConsole				();

		Engine.External.CreateRendererList();

		LPCSTR benchName = "-batch_benchmark ";
		if(strstr(lpCmdLine, benchName))
		{
			int sz = xr_strlen(benchName);
			string64				b_name;
			sscanf					(strstr(Core.Params,benchName)+sz,"%[^ ] ",b_name);
			doBenchmark				(b_name);
			return 0;
		}

		if (strstr(lpCmdLine,"-launcher")) 
		{
			int l_res = doLauncher();
			if (l_res != 0)
				return 0;
		};

#ifndef DEDICATED_SERVER
		if(strstr(Core.Params,"-r2a"))	
			Console->Execute			("renderer renderer_r2a");
		else
		if(strstr(Core.Params,"-r2"))	
			Console->Execute			("renderer renderer_r2");
		else
		if(strstr(Core.Params,"-r3"))	
			Console->Execute			("renderer renderer_r3");
		else
		if(strstr(Core.Params,"-r4"))	
			Console->Execute			("renderer renderer_r4");
		else
		{
			CCC_LoadCFG_custom*	pTmp = new CCC_LoadCFG_custom("renderer ");
			pTmp->Execute				(Console->ConfigFile);
			xr_delete					(pTmp);
		}
#else
			Console->Execute			("renderer renderer_r1");
#endif
//.		InitInput					( );
		Engine.External.Initialize	( );
		Console->Execute			("stat_memory");

#if 0
#if 0
		string_path weapons_path, cop_weapons_path;
		FS.update_path(weapons_path, "$game_config$", "weapons");
		xr_strcat(weapons_path, "\\");
		xr_strcpy(cop_weapons_path, weapons_path);
		xr_strcat(cop_weapons_path, "cop\\");
		const CLocatorAPI::file* folder = FS.exist(weapons_path);
		xr_vector<char*>* files = FS.file_list_open(folder->name, FS_ListFiles | FS_RootOnly);
		for (xr_vector<LPSTR>::iterator it = files->begin(), last = files->end(); it != last; ++it)
		{
			string256 name, ext, fullSoc, fullCop;
			_splitpath(*it, 0, 0, name, ext);
			xr_sprintf(fullCop, "%s%s%s", cop_weapons_path, name, ext);
			xr_sprintf(fullSoc, "%s%s%s", weapons_path, name, ext);
			if (!stricmp(ext, ".ltx") && name[0] == 'w' && name[1] == '_')
			{
				if (!FS.exist(fullCop)) continue;
				CInifile* socIni = new CInifile(fullSoc, FALSE, TRUE);
				CInifile* copIni = new CInifile(fullCop, TRUE);

			
				CInifile::Sect* section = socIni->sections()[0];
				LPCSTR section_name = section->Name.c_str();

				if (!socIni->line_exist(section_name, "upgr_icon_x"))
				{

					u32 x = READ_IF_EXISTS(copIni, r_u32, section_name, "upgr_icon_x", 0);
					u32 y = READ_IF_EXISTS(copIni, r_u32, section_name, "upgr_icon_y", 0);
					u32 w = READ_IF_EXISTS(copIni, r_u32, section_name, "upgr_icon_width", 0);
					u32 h = READ_IF_EXISTS(copIni, r_u32, section_name, "upgr_icon_height", 0);

					socIni->w_u32(section_name, "upgr_icon_x", x);
					socIni->w_u32(section_name, "upgr_icon_y", y);
					socIni->w_u32(section_name, "upgr_icon_width", w);
					socIni->w_u32(section_name, "upgr_icon_height", h);
				}

				string128 oth_section;
				xr_sprintf(oth_section, "%s_upg_base", section_name);
				if (pSettings->section_exist(oth_section) && !socIni->line_exist(section_name, "upgr_icon"))
				{
					shared_str upgr_icon = pSettings->r_string(oth_section, "icon");
					socIni->w_string(section_name, "upgr_icon", *upgr_icon);
				}
				xr_delete(socIni);
				xr_delete(copIni);
			}
		}

		FS.file_list_close(files);
#else
#define COPY_PARAM(src, dst, sect_name, param, r_method, w_method) if (src->section_exist(sect_name) && src->line_exist(sect_name, param) && !dst->line_exist(sect_name, param)) \
																			 dst->w_method(sect_name, param, src->r_method(sect_name, param))

		string_path items_path, fullSoc, fullCop;
		FS.update_path(items_path, "$game_config$", "misc\\items");

		xr_sprintf(fullSoc, "%s\\%s", items_path, "outfit.ltx");
		xr_sprintf(fullCop, "%s\\%s", items_path, "cop_outfit.ltx");

		CInifile* socIni = new CInifile(fullSoc, FALSE, TRUE);
		CInifile* copIni = new CInifile(fullCop, TRUE);

		for (u32 i = 0; i < socIni->section_count(); i++)
		{
			CInifile::Sect* section = socIni->sections()[i];
			LPCSTR section_name = section->Name.c_str();

			if (!socIni->line_exist(section_name, "upgr_icon_x"))
			{
				u32 x = READ_IF_EXISTS(copIni, r_u32, section_name, "upgr_icon_x", 0);
				u32 y = READ_IF_EXISTS(copIni, r_u32, section_name, "upgr_icon_y", 0);
				u32 w = READ_IF_EXISTS(copIni, r_u32, section_name, "upgr_icon_width", 0);
				u32 h = READ_IF_EXISTS(copIni, r_u32, section_name, "upgr_icon_height", 0);

				socIni->w_u32(section_name, "upgr_icon_x", x);
				socIni->w_u32(section_name, "upgr_icon_y", y);
				socIni->w_u32(section_name, "upgr_icon_width", w);
				socIni->w_u32(section_name, "upgr_icon_height", h);
			}
			string128 oth_section;
			xr_sprintf(oth_section, "%s_upg_base", section_name);
			if (pSettings->section_exist(oth_section) && !socIni->line_exist(section_name, "upgr_icon"))
			{
				shared_str upgr_icon = pSettings->r_string(oth_section, "icon");
				socIni->w_string(section_name, "upgr_icon", *upgr_icon);
			}
			COPY_PARAM(copIni, socIni, section_name, "hit_fraction_actor", r_string, w_string);
			COPY_PARAM(copIni, socIni, section_name, "upgrades", r_string, w_string);
			COPY_PARAM(copIni, socIni, section_name, "installed_upgrades", r_string, w_string);
			COPY_PARAM(copIni, socIni, section_name, "upgrade_scheme", r_string, w_string);
		}	
		xr_delete(socIni);
		xr_delete(copIni);
#endif
#else
		Startup	 					( );
#endif
		Core._destroy				( );

		// check for need to execute something external
		if (/*xr_strlen(g_sLaunchOnExit_params) && */xr_strlen(g_sLaunchOnExit_app) ) 
		{
			//CreateProcess need to return results to next two structures
			STARTUPINFO si;
			PROCESS_INFORMATION pi;
			ZeroMemory(&si, sizeof(si));
			si.cb = sizeof(si);
			ZeroMemory(&pi, sizeof(pi));
			//We use CreateProcess to setup working folder
			char const * temp_wf = (xr_strlen(g_sLaunchWorkingFolder) > 0) ? g_sLaunchWorkingFolder : NULL;
			CreateProcess(g_sLaunchOnExit_app, g_sLaunchOnExit_params, NULL, NULL, FALSE, 0, NULL, 
				temp_wf, &si, &pi);

		}
#ifndef DEDICATED_SERVER
#ifdef NO_MULTI_INSTANCES		
		// Delete application presence mutex
		CloseHandle( hCheckPresenceMutex );
#endif
	}
	// here damn_keys_filter class instanse will be destroyed
#endif // DEDICATED_SERVER

	return						0;
}

int stack_overflow_exception_filter	(int exception_code)
{
   if (exception_code == EXCEPTION_STACK_OVERFLOW)
   {
       // Do not call _resetstkoflw here, because
       // at this point, the stack is not yet unwound.
       // Instead, signal that the handler (the __except block)
       // is to be executed.
       return EXCEPTION_EXECUTE_HANDLER;
   }
   else
       return EXCEPTION_CONTINUE_SEARCH;
}

#include <boost/crc.hpp>

int APIENTRY WinMain(HINSTANCE hInstance,
                     HINSTANCE hPrevInstance,
                     char *    lpCmdLine,
                     int       nCmdShow)
{
	__try 
	{
		WinMain_impl		(hInstance,hPrevInstance,lpCmdLine,nCmdShow);
	}
	__except(stack_overflow_exception_filter(GetExceptionCode()))
	{
		_resetstkoflw		();
		FATAL				("stack overflow");
	}

	return					(0);
}

LPCSTR _GetFontTexName (LPCSTR section)
{
	static char* tex_names[]={"texture800","texture","texture1600"};
	int def_idx		= 1;//default 1024x768
	int idx			= def_idx;

#if 0
	u32 w = Device.dwWidth;

	if(w<=800)		idx = 0;
	else if(w<=1280)idx = 1;
	else 			idx = 2;
#else
	u32 h = Device.dwHeight;

	if(h<=600)		idx = 0;
	else if(h<1024)	idx = 1;
	else 			idx = 2;
#endif


	while(idx>=0){
		if( pSettings->line_exist(section,tex_names[idx]) )
			return pSettings->r_string(section,tex_names[idx]);
		--idx;
	}
	return pSettings->r_string(section,tex_names[def_idx]);
}

void _InitializeFont(CGameFont*& F, LPCSTR section, u32 flags)
{
	LPCSTR font_tex_name = _GetFontTexName(section);
	R_ASSERT(font_tex_name);

	LPCSTR sh_name = pSettings->r_string(section,"shader");
	if(!F){
		F = new CGameFont(sh_name, font_tex_name, flags);
	}else
		F->Initialize(sh_name, font_tex_name);

	if (pSettings->line_exist(section,"size")){
		float sz = pSettings->r_float(section,"size");
		if (flags&CGameFont::fsDeviceIndependent)	F->SetHeightI(sz);
		else										F->SetHeight(sz);
	}
	if (pSettings->line_exist(section,"interval"))
		F->SetInterval(pSettings->r_fvector2(section,"interval"));

}

CApplication::CApplication()
{
	ll_dwReference	= 0;

	max_load_stage = 0;

	// events
	eQuit						= Engine.Event.Handler_Attach("KERNEL:quit",this);
	eStart						= Engine.Event.Handler_Attach("KERNEL:start",this);
	eStartLoad					= Engine.Event.Handler_Attach("KERNEL:load",this);
	eDisconnect					= Engine.Event.Handler_Attach("KERNEL:disconnect",this);
	eConsole					= Engine.Event.Handler_Attach("KERNEL:console",this);
	//eStartMPDemo				= Engine.Event.Handler_Attach("KERNEL:start_mp_demo",this);

	// levels
	Level_Current				= u32(-1);
	Level_Scan					( );

	// Font
	pFontSystem					= NULL;

	// Register us
	Device.seqFrame.Add			(this, REG_PRIORITY_HIGH+1000);
	
	if (psDeviceFlags.test(mtSound))	Device.seqFrameMT.Add		(&SoundProcessor);
	else								Device.seqFrame.Add			(&SoundProcessor);

	Console->Show				( );

	// App Title
//	app_title[ 0 ] = '\0';
	ls_header[ 0 ] = '\0';
	ls_tip_number[ 0 ] = '\0';
	ls_tip[ 0 ] = '\0';
}

CApplication::~CApplication()
{
	Console->Hide				( );

	// font
	xr_delete					( pFontSystem		);

	Device.seqFrameMT.Remove	(&SoundProcessor);
	Device.seqFrame.Remove		(&SoundProcessor);
	Device.seqFrame.Remove		(this);


	// events
	Engine.Event.Handler_Detach	(eConsole,this);
	Engine.Event.Handler_Detach	(eDisconnect,this);
	Engine.Event.Handler_Detach	(eStartLoad,this);
	Engine.Event.Handler_Detach	(eStart,this);
	Engine.Event.Handler_Detach	(eQuit,this);
	//Engine.Event.Handler_Detach	(eStartMPDemo,this);
	
}

extern CRenderDevice Device;

void CApplication::OnEvent(EVENT E, u64 P1, u64 P2)
{
	if (E==eQuit)
	{
		FlushLog		();
		PostQuitMessage	(0);
		
		for (u32 i=0; i<Levels.size(); i++)
		{
			xr_free(Levels[i].folder	);
			xr_free(Levels[i].name	);
		}
	}
	else if(E==eStart) 
	{
		LPSTR		op_server		= LPSTR	(P1);
		LPSTR		op_client		= LPSTR	(P2);
		Level_Current				= u32(-1);
		R_ASSERT	(0==g_pGameLevel);
		R_ASSERT	(0!=g_pGamePersistent);

#ifdef NO_SINGLE
		Console->Execute("main_menu on");
		if (	(op_server == NULL)			||
				(!xr_strlen(op_server))		||
				(
					(	strstr(op_server, "/dm")	|| strstr(op_server, "/deathmatch") ||
						strstr(op_server, "/tdm")	|| strstr(op_server, "/teamdeathmatch") ||
						strstr(op_server, "/ah")	|| strstr(op_server, "/artefacthunt") ||
						strstr(op_server, "/cta")	|| strstr(op_server, "/capturetheartefact")
					) && 
					!strstr(op_server, "/alife")
				)
			)
#endif // #ifdef NO_SINGLE
		{		
			FlushLog						();
			Console->Execute("main_menu off");
			Console->Hide();
//!			this line is commented by Dima
//!			because I don't see any reason to reset device here
//!			Device.Reset					(false);
			//-----------------------------------------------------------
			g_pGamePersistent->PreStart		(op_server);
			//-----------------------------------------------------------
			g_pGameLevel					= (IGame_Level*)NEW_INSTANCE(CLSID_GAME_LEVEL);
			pApp->LoadBegin					(); 
			g_pGamePersistent->Start		(op_server);
			g_pGameLevel->net_Start			(op_server,op_client);
			pApp->LoadEnd					(); 
		}
		FlushLog			();
		xr_free							(op_server);
		xr_free							(op_client);
	} 
	else if (E==eDisconnect) 
	{
		ls_header[0] = '\0';
		ls_tip_number[0] = '\0';
		ls_tip[0] = '\0';

		if (g_pGameLevel) 
		{
			Console->Hide			();
			g_pGameLevel->net_Stop	();
			DEL_INSTANCE			(g_pGameLevel);
			Console->Show			();
			
			if( (FALSE == Engine.Event.Peek("KERNEL:quit")) &&(FALSE == Engine.Event.Peek("KERNEL:start")) )
			{
				Console->Execute("main_menu off");
				Console->Execute("main_menu on");
			}
		}
		FlushLog			();
		R_ASSERT			(0!=g_pGamePersistent);
		g_pGamePersistent->Disconnect();
	}
	else if (E == eConsole)
	{
		LPSTR command				= (LPSTR)P1;
		Console->ExecuteCommand		( command, false );
		xr_free						(command);
	}
/*
	else if (E == eStartMPDemo)
	{
		LPSTR demo_file				= LPSTR	(P1);

		R_ASSERT	(0==g_pGameLevel);
		R_ASSERT	(0!=g_pGamePersistent);

		Console->Execute("main_menu off");
		Console->Hide();
		Device.Reset					(false);

		g_pGameLevel					= (IGame_Level*)NEW_INSTANCE(CLSID_GAME_LEVEL);
		shared_str server_options;		= g_pGameLevel->OpenDemoFile(demo_file);
		
		//-----------------------------------------------------------
		g_pGamePersistent->PreStart		(server_options.c_str());
		//-----------------------------------------------------------
		
		pApp->LoadBegin					(); 
		g_pGamePersistent->Start		("");//server_options.c_str()); - no prefetch !
		g_pGameLevel->net_StartPlayDemo	();
		pApp->LoadEnd					(); 

		xr_free						(demo_file);
	}
*/
}

static	CTimer	phase_timer		;
extern	ENGINE_API BOOL			g_appLoaded = FALSE;

void CApplication::LoadBegin	()
{
	ll_dwReference++;
	if (1==ll_dwReference)	{

		g_appLoaded			= FALSE;

#ifndef DEDICATED_SERVER
		_InitializeFont		(pFontSystem,"ui_font_graffiti19_russian",0);

		m_pRender->LoadBegin();
#endif
		phase_timer.Start	();
		load_stage			= 0;

		CheckCopyProtection	();
	}
}

void CApplication::LoadEnd		()
{
	ll_dwReference--;
	if (0==ll_dwReference)		{
		Msg						("* phase time: %d ms",phase_timer.GetElapsed_ms());
		Msg						("* phase cmem: %d K", Memory.mem_usage()/1024);
		Console->Execute		("stat_memory");
		g_appLoaded				= TRUE;
//		DUMP_PHASE;
	}
}

void CApplication::destroy_loading_shaders()
{
	m_pRender->destroy_loading_shaders();
}

//u32 calc_progress_color(u32, u32, int, int);

PROTECT_API void CApplication::LoadDraw		()
{
	if(g_appLoaded)				return;
	Device.dwFrame				+= 1;


	if(!Device.Begin () )		return;

	if	(g_dedicated_server)
		Console->OnRender			();
	else
		load_draw_internal			();

	Device.End					();
	CheckCopyProtection			();
}

void CApplication::LoadTitleInt(LPCSTR str1)
{
	xr_strcpy					(ls_header, str1);

	LoadStage					();
}

void CApplication::ClearTitle()
{
	ls_header[ 0 ] = '\0';
}

void CApplication::LoadStage()
{
	load_stage++;
	VERIFY						(ll_dwReference);
	Msg							("* phase time: %d ms",phase_timer.GetElapsed_ms());	phase_timer.Start();
	Msg							("* phase cmem: %d K", Memory.mem_usage()/1024);
	
	if (g_pGamePersistent->GameType()==1 && strstr(Core.Params,"alife"))
		max_load_stage			= 17;
	else
		max_load_stage			= 14;
	LoadDraw					();
}
void CApplication::LoadSwitch	()
{
}

// Sequential
void CApplication::OnFrame	( )
{
	Engine.Event.OnFrame			();
	g_SpatialSpace->update			();
	g_SpatialSpacePhysic->update	();
	if (g_pGameLevel)				g_pGameLevel->SoundEvent_Dispatch	( );
}

void CApplication::Level_Append		(LPCSTR folder)
{
	string_path	N1,N2,N3,N4;
	strconcat	(sizeof(N1),N1,folder,"level");
	strconcat	(sizeof(N2),N2,folder,"level.ltx");
	strconcat	(sizeof(N3),N3,folder,"level.geom");
	strconcat	(sizeof(N4),N4,folder,"level.cform");
	if	(
		FS.exist("$game_levels$",N1)		&&
		FS.exist("$game_levels$",N2)		&&
		FS.exist("$game_levels$",N3)		&&
		FS.exist("$game_levels$",N4)	
		)
	{
		sLevelInfo			LI;
		LI.folder			= xr_strdup(folder);
		LI.name				= 0;
		Levels.push_back	(LI);
	}
}

void CApplication::Level_Scan()
{
	//SECUROM_MARKER_PERFORMANCE_ON(8)
#pragma todo("container is created in stack!")
	xr_vector<char*>*		folder			= FS.file_list_open		("$game_levels$",FS_ListFolders|FS_RootOnly);
	R_ASSERT				(folder&&folder->size());
	for (u32 i=0; i<folder->size(); i++)	Level_Append((*folder)[i]);
	FS.file_list_close		(folder);
#ifdef DEBUG
	folder									= FS.file_list_open		("$game_levels$","$debug$\\",FS_ListFolders|FS_RootOnly);
	if (folder){
		string_path	tmp_path;
		for (u32 i=0; i<folder->size(); i++)
		{
			strconcat			(sizeof(tmp_path),tmp_path,"$debug$\\",(*folder)[i]);
			Level_Append		(tmp_path);
		}

		FS.file_list_close	(folder);
	}
#endif
	//SECUROM_MARKER_PERFORMANCE_OFF(8)
}

void CApplication::Level_Set(u32 L)
{
	//SECUROM_MARKER_PERFORMANCE_ON(9)

	if (L>=Levels.size())	return;
	Level_Current = L;
	FS.get_path	("$level$")->_set	(Levels[L].folder);


	string_path					temp;
	string_path					temp2;
	strconcat					(sizeof(temp),temp,"intro\\intro_",Levels[L].folder);
	temp[xr_strlen(temp)-1] = 0;
	if (FS.exist(temp2, "$game_textures$", temp, ".dds"))
		m_pRender->setLevelLogo	(temp);
	else
		m_pRender->setLevelLogo ("intro\\intro_no_start_picture");
		

	CheckCopyProtection		();

	//SECUROM_MARKER_PERFORMANCE_OFF(9)
}

int CApplication::Level_ID(LPCSTR name)
{
	int result = -1;

	//SECUROM_MARKER_SECURITY_ON(7)

	char buffer	[256];
	strconcat	(sizeof(buffer),buffer,name,"\\");
	for (u32 I=0; I<Levels.size(); I++)
	{
		if (0==stricmp(buffer,Levels[I].folder))	
			result = int(I);
	}


//	if( arch_res )
//		g_pGamePersistent->OnAssetsChanged	();

	//SECUROM_MARKER_SECURITY_OFF(7)

	return result;
}



#ifndef DEDICATED_SERVER
// Parential control for Vista and upper
typedef BOOL (*PCCPROC)( CHAR* ); 
/*
BOOL IsPCAccessAllowed()
{
	CHAR szPCtrlChk[ MAX_PATH ] , szGDF[ MAX_PATH ] , *pszLastSlash;
	HINSTANCE hPCtrlChk = NULL;
	PCCPROC pctrlchk = NULL;
	BOOL bAllowed = TRUE;

	if ( ! GetModuleFileName( NULL , szPCtrlChk , MAX_PATH ) )
		return TRUE;

	if ( ( pszLastSlash = strrchr( szPCtrlChk , '\\' ) ) == NULL )
		return TRUE;

	*pszLastSlash = '\0';

	xr_strcpy( szGDF , szPCtrlChk );

	xr_strcat( szPCtrlChk , MAX_PATH "\\pctrlchk.dll" );
	if ( GetFileAttributes( szPCtrlChk ) == INVALID_FILE_ATTRIBUTES )
		return TRUE;

	if ( ( pszLastSlash = strrchr( szGDF , '\\' ) ) == NULL )
		return TRUE;

	*pszLastSlash = '\0';

	xr_strcat( szGDF , MAX_PATH, "\\xr_3da.exe" );
	if ( GetFileAttributes( szGDF ) == INVALID_FILE_ATTRIBUTES )
		return TRUE;

	if ( ( hPCtrlChk = LoadLibrary( szPCtrlChk ) ) == NULL )
		return TRUE;

	if ( ( pctrlchk = (PCCPROC) GetProcAddress( hPCtrlChk , "pctrlchk" ) ) == NULL ) {
		FreeLibrary( hPCtrlChk );
		return TRUE;
	}

	bAllowed = pctrlchk( szGDF );

	FreeLibrary( hPCtrlChk );

	return bAllowed;
}
*/
#endif // DEDICATED_SERVER

//launcher stuff----------------------------
extern "C"{
	typedef int	 __cdecl LauncherFunc	(int);
}
HMODULE			hLauncher		= NULL;
LauncherFunc*	pLauncher		= NULL;

void InitLauncher(){
	if(hLauncher)
		return;
	hLauncher	= LoadLibrary	("xrLauncher.dll");
	if (0==hLauncher)	R_CHK	(GetLastError());
	R_ASSERT2		(hLauncher,"xrLauncher DLL raised exception during loading or there is no xrLauncher.dll at all");

	pLauncher = (LauncherFunc*)GetProcAddress(hLauncher,"RunXRLauncher");
	R_ASSERT2		(pLauncher,"Cannot obtain RunXRLauncher function from xrLauncher.dll");
};

void FreeLauncher(){
	if (hLauncher)	{ 
		FreeLibrary(hLauncher); 
		hLauncher = NULL; pLauncher = NULL; };
}

int doLauncher()
{
/*
	execUserScript();
	InitLauncher();
	int res = pLauncher(0);
	FreeLauncher();
	if(res == 1) // do benchmark
		g_bBenchmark = true;

	if(g_bBenchmark){ //perform benchmark cycle
		doBenchmark();
	
		// InitLauncher	();
		// pLauncher	(2);	//show results
		// FreeLauncher	();

		Core._destroy			();
		return					(1);

	};
	if(res==8){//Quit
		Core._destroy			();
		return					(1);
	}
*/
	return 0;
}

void doBenchmark(LPCSTR name)
{
	g_bBenchmark = true;
	string_path in_file;
	FS.update_path(in_file,"$app_data_root$", name);
	CInifile ini(in_file);
	int test_count = ini.line_count("benchmark");
	LPCSTR test_name,t;
	shared_str test_command;
	for(int i=0;i<test_count;++i){
		ini.r_line			( "benchmark", i, &test_name, &t);
		xr_strcpy				(g_sBenchmarkName, test_name);
		
		test_command		= ini.r_string_wb("benchmark",test_name);
		xr_strcpy			(Core.Params,*test_command);
		_strlwr_s				(Core.Params);
		
		InitInput					();
		if(i){
			//ZeroMemory(&HW,sizeof(CHW));
			//	TODO: KILL HW here!
			//  pApp->m_pRender->KillHW();
			InitEngine();
		}


		Engine.External.Initialize	( );

		xr_strcpy						(Console->ConfigFile,"user.ltx");
		if (strstr(Core.Params,"-ltx ")) {
			string64				c_name;
			sscanf					(strstr(Core.Params,"-ltx ")+5,"%[^ ] ",c_name);
			xr_strcpy				(Console->ConfigFile,c_name);
		}

		Startup	 				();
	}
}
#pragma optimize("g", off)

void CApplication::load_draw_internal()
{
	m_pRender->load_draw_internal(*this);
}
