#pragma once
#include <intrin.h>
#include "dbg.h"
#ifndef VCPU
#define VCPU
#include "ia32.h"
#include "resolver.h"
#include <ntifs.h>

constexpr size_t vmm_stack_size = 0x10000; // 64KB — was 24KB, too tight for deep call chains


typedef struct vcpu_t
{
	alignas( 0x1000 )vmx_msr_bitmap* msr_bitmap;
	uint64_t msr_bitmap_physical;
	uint64_t* vmm_stack;
	alignas( 0x1000 )vmcs_t* vmcs_region;
	alignas( 0x1000 )vmxon_t* vmxon_region;
	uint64_t vmcs_region_physical;
	uint64_t vmxon_region_physical;
	uint32_t queued_nmis;
};

namespace vcpu
{
	// Set a bit in the MSR read bitmap to trigger VM-exit on RDMSR
	inline void set_msr_read_intercept( vmx_msr_bitmap* bitmap, uint32_t msr )
	{
		if ( msr <= 0x1FFF )
		{
			uint8_t* read_low = reinterpret_cast<uint8_t*>( bitmap );
			read_low[ msr / 8 ] |= ( 1u << ( msr % 8 ) );
		}
		else if ( msr >= 0xC0000000 && msr <= 0xC0001FFF )
		{
			uint32_t off = msr - 0xC0000000;
			uint8_t* read_high = reinterpret_cast<uint8_t*>( bitmap ) + 0x400;
			read_high[ off / 8 ] |= ( 1u << ( off % 8 ) );
		}
	}

	// Set a bit in the MSR write bitmap to trigger VM-exit on WRMSR
	inline void set_msr_write_intercept( vmx_msr_bitmap* bitmap, uint32_t msr )
	{
		if ( msr <= 0x1FFF )
		{
			uint8_t* write_low = reinterpret_cast<uint8_t*>( bitmap ) + 0x800;
			write_low[ msr / 8 ] |= ( 1u << ( msr % 8 ) );
		}
		else if ( msr >= 0xC0000000 && msr <= 0xC0001FFF )
		{
			uint32_t off = msr - 0xC0000000;
			uint8_t* write_high = reinterpret_cast<uint8_t*>( bitmap ) + 0xC00;
			write_high[ off / 8 ] |= ( 1u << ( off % 8 ) );
		}
	}

	vcpu_t* allocate_vcpu( )
	{
		vcpu_t* vcpu = ( vcpu_t* ) resolver::g_ExAllocatePool( NonPagedPool, sizeof( vcpu_t ) );
		PHYSICAL_ADDRESS phys;
		phys.QuadPart = MAXULONG64;

		vcpu->vmxon_region = (vmxon_t*)resolver::g_MmAllocateContiguousMemory( sizeof( vmxon_t ), phys );
		vcpu->vmcs_region = ( vmcs_t* ) resolver::g_MmAllocateContiguousMemory( sizeof( vmcs_t ), phys );

		if ( !vcpu->vmxon_region || !vcpu->vmcs_region )
			return nullptr;

		log::dbg_print( "allocated vmxon region -> 0x%llx", vcpu->vmxon_region );
		log::dbg_print( "allocated vmcs region -> 0x%llx", vcpu->vmcs_region );

		memset( vcpu->vmcs_region, 0x0, sizeof( vmcs_t ) );
		memset( vcpu->vmxon_region, 0x0, sizeof( vmxon_t ) );

		vcpu->vmcs_region_physical = ( uint64_t ) resolver::g_MmGetPhysicalAddress( vcpu->vmcs_region ).QuadPart;
		vcpu->vmxon_region_physical = ( uint64_t ) resolver::g_MmGetPhysicalAddress( vcpu->vmxon_region ).QuadPart;

		log::dbg_print( "vmcs region physical -> 0x%llx", vcpu->vmcs_region_physical );
		log::dbg_print( "vmxon region physical -> 0x%llx", vcpu->vmxon_region_physical );

		vcpu->msr_bitmap = ( vmx_msr_bitmap* ) resolver::g_MmAllocateContiguousMemory( 0x4000, phys );
		if ( !vcpu->msr_bitmap )
			return nullptr;

		memset( vcpu->msr_bitmap, 0x0, 0x4000 );

		// Anti-detection MSR interception strategy:
		// - VMX MSRs (0x480-0x491): DO NOT intercept. Pass through real values.
		//   CPUID says VMX supported → MSRs must have real values (zeroing = instant detect)
		// - IA32_FEATURE_CONTROL (0x3A): Intercept RDMSR to show VMX disabled.
		//   Makes VMXON VMfailInvalid consistent ("BIOS disabled VMX" is valid config)
		// - IA32_APERF/MPERF (0xE8/0xE7): Intercept RDMSR/WRMSR to hide VM-exit overhead.
		//   APERF-based timing is "the most dangerous" detection — catches TSC-only hiders.
		set_msr_read_intercept( vcpu->msr_bitmap, 0x3A );   // IA32_FEATURE_CONTROL
		set_msr_read_intercept( vcpu->msr_bitmap, 0xE7 );   // IA32_MPERF
		set_msr_read_intercept( vcpu->msr_bitmap, 0xE8 );   // IA32_APERF
		set_msr_write_intercept( vcpu->msr_bitmap, 0xE7 );  // MPERF write (reset offset on OS reset)
		set_msr_write_intercept( vcpu->msr_bitmap, 0xE8 );  // APERF write (reset offset on OS reset)

		vcpu->msr_bitmap_physical = ( uint64_t ) resolver::g_MmGetPhysicalAddress( vcpu->msr_bitmap ).QuadPart;
		log::dbg_print( "msr bitmap physical -> 0x%llx", vcpu->msr_bitmap_physical );


		ia32_vmx_basic_register vmx_basic;
		vmx_basic.flags = __readmsr( IA32_VMX_BASIC );

		vcpu->vmcs_region->revision_id = vmx_basic.vmcs_revision_id;
		vcpu->vmxon_region->revision_id = vmx_basic.vmcs_revision_id;

		log::dbg_print( "vmcs revision id -> 0x%llx", vmx_basic.vmcs_revision_id );

		vcpu->vmm_stack = ( uint64_t* ) resolver::g_ExAllocatePool( NonPagedPool, vmm_stack_size );

		log::dbg_print( "vmm stack -> 0x%llx", vcpu->vmm_stack );


		return vcpu;
	}
}

#endif // VCPU
