#include "resolver.h"
#include "init.h"
#include "vmexit.h"
#include "cr3.h"
#include "ept.h"
#include "dbg.h"

extern "C" __declspec(dllimport) void __stdcall KeBugCheckEx(
	unsigned long BugCheckCode,
	unsigned __int64 BugCheckParameter1,
	unsigned __int64 BugCheckParameter2,
	unsigned __int64 BugCheckParameter3,
	unsigned __int64 BugCheckParameter4
);

// Bugcheck codes:
// 0xDEAD0001 = resolver failed
// 0xDEAD0002 = VMX not supported (CR4.VMXE or MSR)
// 0xDEAD0003 = VMX locked off by BIOS
// 0xDEAD0004 = EPT init failed (non-fatal, continues)
// 0xDEAD0005 = virtualize_all_cores failed
// NO BSOD   = success! Test CPUID interception

extern "C" nt_status_t driver_entry( driver_object_t* drv, unicode_string_t* reg )
{
	// Diagnose get_nt_base first
	uint64_t nt_base = get_nt_base( );
	if ( !nt_base )
		KeBugCheckEx( 0xDEAD0001, 0xBA5E0000, 0, 0, 0 );

	// Try to read DOS header
	auto dos = reinterpret_cast<IMAGE_DOS_HEADER*>( nt_base );
	if ( dos->e_magic != 0x5A4D )
		KeBugCheckEx( 0xDEAD0001, 0xBA5E0001, nt_base, dos->e_magic, 0 );

	if ( !resolver::initialize( ) )
		KeBugCheckEx( 0xDEAD0001, nt_base, 0, 0, 0 );

	// Check VMX support
	int cpu_info[4];
	__cpuid( cpu_info, 1 );
	if ( !( cpu_info[2] & ( 1 << 5 ) ) )
		KeBugCheckEx( 0xDEAD0002, cpu_info[2], 0, 0, 0 );

	uint64_t feature_control = __readmsr( 0x3A );
	if ( ( feature_control & 0x1 ) && !( feature_control & 0x4 ) )
		KeBugCheckEx( 0xDEAD0003, feature_control, 0, 0, 0 );

	// EPT disabled - causes GPU TDR (0x116) on this system
	// Core memory ops use MmCopyMemory/MmMapIoSpaceEx which don't need EPT
	// EPT only needed for VMCS/VMXON hiding (anti-cheat stealth)
	// ept::initialize( );

	if ( !init::virtualize_all_cores( ) )
		KeBugCheckEx( 0xDEAD0005, g_vmlaunch_fail_count, 0, 0, 0 );

	// If we reach here -> hypervisor is running on all cores
	return nt_status_t::success;
}
