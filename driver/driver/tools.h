#ifndef TOOLS_H
#define TOOLS_H
#include <ntifs.h>
#include <cstdint>
#include "ia32.h"
#include "resolver.h"
#include <intrin.h>
namespace tools
{
	extern "C" uint64_t get_nt_base( );

	void* get_system_routine( const wchar_t* routine_name )
	{
		UNICODE_STRING uc;
		resolver::g_RtlInitUnicodeString( &uc, routine_name );
		return resolver::g_MmGetSystemRoutineAddress( &uc );

	}

	bool is_vmx_supported( ) {
		int cpuInfo[ 4 ] = { 0 };
		__cpuid( cpuInfo, 1 );

		return ( cpuInfo[ 2 ] & ( 1 << 5 ) ) != 0;
	}

	bool enable_vmx( )
	{
		ia32_feature_control_register feature_ctl;
		feature_ctl.flags = __readmsr( IA32_FEATURE_CONTROL );

		if ( !feature_ctl.lock_bit )
		{
			feature_ctl.enable_vmx_outside_smx = 1;
			feature_ctl.lock_bit = 1;
			__writemsr( IA32_FEATURE_CONTROL, feature_ctl.flags );
		}
		else if ( !feature_ctl.enable_vmx_outside_smx )
		{
			return false;
		}

		uint64_t cr0_fixed0 = __readmsr( IA32_VMX_CR0_FIXED0 ); 
		uint64_t cr0_fixed1 = __readmsr( IA32_VMX_CR0_FIXED1 );

		uint64_t cr4_fixed0 = __readmsr( IA32_VMX_CR4_FIXED0 );
		uint64_t cr4_fixed1 = __readmsr( IA32_VMX_CR4_FIXED1 ); 

		uint64_t cr0 = __readcr0( );
		cr0 |= cr0_fixed0;  
		cr0 &= cr0_fixed1;  
		__writecr0( cr0 );

		uint64_t cr4 = __readcr4( );
		cr4 |= cr4_fixed0;   
		cr4 &= cr4_fixed1;
		__writecr4( cr4 );

		return true;
	}

	uint64_t vmread( uint64_t field )
	{
		uint64_t result = 0;
		__vmx_vmread( field, &result );
		return result;
	}

	void vmwrite( uint64_t field, uint64_t value )
	{
		__vmx_vmwrite( field, value );
	}

	uint64_t get_kernel_cr3( )
	{
		uint64_t cr3 = *( uint64_t* ) ( ( uint64_t ) resolver::g_PsInitialSystemProcess + 0x28 );
		return cr3;
	}

}

#endif // TOOLS_H
