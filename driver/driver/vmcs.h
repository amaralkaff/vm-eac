#pragma once
#ifndef VMCS
#define VMCS


#include "tools.h"
#include "vcpu.h"
#include "segments.h"
#include "ept.h"

// Tracks how many cores failed vmlaunch
inline volatile long g_vmlaunch_fail_count = 0;

extern "C" void vmwrite_rsp_rip( uint64_t rsp, uint64_t rip )
{
	tools::vmwrite( VMCS_GUEST_RSP, rsp );
	tools::vmwrite( VMCS_GUEST_RIP, rip );
}
extern "C" void restore_ctx( void );

namespace vmcs
{
	extern "C" void vmexit_handler( void );

	extern "C" bool vmcs_setup_and_launch( vcpu_t* vcpu, uint64_t guest_rsp )
	{
		//guest-state area

		uint64_t cr0_fixed0 = __readmsr( IA32_VMX_CR0_FIXED0 );
		uint64_t cr0_fixed1 = __readmsr( IA32_VMX_CR0_FIXED1 );
		uint64_t cr4_fixed0 = __readmsr( IA32_VMX_CR4_FIXED0 );
		uint64_t cr4_fixed1 = __readmsr( IA32_VMX_CR4_FIXED1 );

		uint64_t guest_cr0 = __readcr0( );
		guest_cr0 |= cr0_fixed0;
		guest_cr0 &= cr0_fixed1;

		uint64_t guest_cr4 = __readcr4( );
		guest_cr4 |= cr4_fixed0;
		guest_cr4 &= cr4_fixed1;

		tools::vmwrite( VMCS_GUEST_CR0, guest_cr0 );
		tools::vmwrite( VMCS_GUEST_CR3, __readcr3( ) );
		tools::vmwrite( VMCS_GUEST_CR4, guest_cr4 );

		//rip and rsp are set on ctx.asm


		tools::vmwrite( VMCS_GUEST_RFLAGS, __readeflags( ) );

		tools::vmwrite( VMCS_GUEST_CS_SELECTOR, segments::get_cs( ).flags );
		tools::vmwrite( VMCS_GUEST_SS_SELECTOR, segments::get_ss( ).flags );
		tools::vmwrite( VMCS_GUEST_DS_SELECTOR, segments::get_ds( ).flags );
		tools::vmwrite( VMCS_GUEST_ES_SELECTOR, segments::get_es( ).flags );
		tools::vmwrite( VMCS_GUEST_FS_SELECTOR, segments::get_fs( ).flags );
		tools::vmwrite( VMCS_GUEST_GS_SELECTOR, segments::get_gs( ).flags );
		tools::vmwrite( VMCS_GUEST_LDTR_SELECTOR, segments::get_ldtr( ).flags );
		tools::vmwrite( VMCS_GUEST_TR_SELECTOR, segments::get_tr( ).flags );

		segment_descriptor_register_64_t gdtr, idtr;
		segments::_sgdt( &gdtr );
		__sidt( &idtr );
		tools::vmwrite( VMCS_GUEST_CS_BASE, segments::segment_base( gdtr, segments::get_cs( ) ) );
		tools::vmwrite( VMCS_GUEST_SS_BASE, segments::segment_base( gdtr, segments::get_ss( ) ) );
		tools::vmwrite( VMCS_GUEST_DS_BASE, segments::segment_base( gdtr, segments::get_ds( ) ) );
		tools::vmwrite( VMCS_GUEST_ES_BASE, segments::segment_base( gdtr, segments::get_es( ) ) );
		tools::vmwrite( VMCS_GUEST_FS_BASE, __readmsr( IA32_FS_BASE ) );
		tools::vmwrite( VMCS_GUEST_GS_BASE, __readmsr( IA32_GS_BASE ) );
		tools::vmwrite( VMCS_GUEST_LDTR_BASE, segments::segment_base( gdtr, segments::get_ldtr( ) ) );
		tools::vmwrite( VMCS_GUEST_TR_BASE, segments::segment_base( gdtr, segments::get_tr( ) ) );


		tools::vmwrite( VMCS_GUEST_CS_LIMIT, __segmentlimit( segments::get_cs( ).flags ) );
		tools::vmwrite( VMCS_GUEST_SS_LIMIT, __segmentlimit( segments::get_ss( ).flags ) );
		tools::vmwrite( VMCS_GUEST_DS_LIMIT, __segmentlimit( segments::get_ds( ).flags ) );
		tools::vmwrite( VMCS_GUEST_ES_LIMIT, __segmentlimit( segments::get_es( ).flags ) );
		tools::vmwrite( VMCS_GUEST_FS_LIMIT, __segmentlimit( segments::get_fs( ).flags ) );
		tools::vmwrite( VMCS_GUEST_GS_LIMIT, __segmentlimit( segments::get_gs( ).flags ) );
		tools::vmwrite( VMCS_GUEST_LDTR_LIMIT, __segmentlimit( segments::get_ldtr( ).flags ) );
		tools::vmwrite( VMCS_GUEST_TR_LIMIT, __segmentlimit( segments::get_tr( ).flags ) );


		tools::vmwrite( VMCS_GUEST_CS_ACCESS_RIGHTS, segments::segment_access(gdtr, segments::get_cs( )).flags );
		tools::vmwrite( VMCS_GUEST_SS_ACCESS_RIGHTS, segments::segment_access( gdtr, segments::get_ss( ) ).flags );
		tools::vmwrite( VMCS_GUEST_DS_ACCESS_RIGHTS, segments::segment_access( gdtr, segments::get_ds( ) ).flags );
		tools::vmwrite( VMCS_GUEST_ES_ACCESS_RIGHTS, segments::segment_access( gdtr, segments::get_es( ) ).flags );
		tools::vmwrite( VMCS_GUEST_FS_ACCESS_RIGHTS, segments::segment_access( gdtr, segments::get_fs( ) ).flags );
		tools::vmwrite( VMCS_GUEST_GS_ACCESS_RIGHTS, segments::segment_access( gdtr, segments::get_gs( ) ).flags );
		tools::vmwrite( VMCS_GUEST_LDTR_ACCESS_RIGHTS, segments::segment_access( gdtr, segments::get_ldtr( ) ).flags );
		tools::vmwrite( VMCS_GUEST_TR_ACCESS_RIGHTS, segments::segment_access( gdtr, segments::get_tr( ) ).flags );

		tools::vmwrite( VMCS_GUEST_GDTR_BASE, gdtr.base_address );
		tools::vmwrite( VMCS_GUEST_GDTR_LIMIT, gdtr.limit );

		tools::vmwrite( VMCS_GUEST_IDTR_BASE, idtr.base_address );
		tools::vmwrite( VMCS_GUEST_IDTR_LIMIT, idtr.limit );

		tools::vmwrite( VMCS_GUEST_DEBUGCTL, __readmsr( IA32_DEBUGCTL ) );
		tools::vmwrite( VMCS_GUEST_SYSENTER_CS, __readmsr( IA32_SYSENTER_CS ) );
		tools::vmwrite( VMCS_GUEST_SYSENTER_ESP, __readmsr( IA32_SYSENTER_ESP ) );
		tools::vmwrite( VMCS_GUEST_SYSENTER_EIP, __readmsr( IA32_SYSENTER_EIP ) );
		tools::vmwrite( VMCS_GUEST_PAT, __readmsr( IA32_PAT ) );
		tools::vmwrite( VMCS_GUEST_VMCS_LINK_POINTER, MAXULONG64 );

		//host-state area

		uint64_t host_cr0 = __readcr0( );
		host_cr0 |= cr0_fixed0;
		host_cr0 &= cr0_fixed1;

		uint64_t host_cr4 = __readcr4( );
		host_cr4 |= cr4_fixed0;
		host_cr4 &= cr4_fixed1;

		tools::vmwrite( VMCS_HOST_CR0, host_cr0 );
		tools::vmwrite( VMCS_HOST_CR3, tools::get_kernel_cr3( ) );
		tools::vmwrite( VMCS_HOST_CR4, host_cr4 );

		tools::vmwrite( VMCS_GUEST_ACTIVITY_STATE, vmx_active ); 

		tools::vmwrite( VMCS_GUEST_INTERRUPTIBILITY_STATE, 0 );

		tools::vmwrite( VMCS_GUEST_PENDING_DEBUG_EXCEPTIONS, 0 );
		

		auto rsp = ( ( uint64_t ) vcpu->vmm_stack + vmm_stack_size );
		rsp &= ~0xFULL;
		rsp -= 8;

		tools::vmwrite( VMCS_HOST_RSP, rsp );
		tools::vmwrite( VMCS_HOST_RIP, ( uint64_t ) vmexit_handler );
		
		tools::vmwrite( VMCS_HOST_CS_SELECTOR, segments::get_cs( ).flags & 0xF8 );
		tools::vmwrite( VMCS_HOST_SS_SELECTOR, segments::get_ss( ).flags & 0xF8 );
		tools::vmwrite( VMCS_HOST_DS_SELECTOR, segments::get_ds( ).flags & 0xF8 );
		tools::vmwrite( VMCS_HOST_ES_SELECTOR, segments::get_es( ).flags & 0xF8 );
		tools::vmwrite( VMCS_HOST_FS_SELECTOR, segments::get_fs( ).flags & 0xF8 );
		tools::vmwrite( VMCS_HOST_GS_SELECTOR, segments::get_gs( ).flags & 0xF8 );
		tools::vmwrite( VMCS_HOST_TR_SELECTOR, segments::get_tr( ).flags & 0xF8 );

		tools::vmwrite( VMCS_HOST_FS_BASE, __readmsr( IA32_FS_BASE ) );
		tools::vmwrite( VMCS_HOST_GS_BASE, __readmsr( IA32_GS_BASE ) );
		tools::vmwrite( VMCS_HOST_TR_BASE, segments::segment_base( gdtr, segments::get_tr( ) ) );
		tools::vmwrite( VMCS_HOST_GDTR_BASE, gdtr.base_address );
		tools::vmwrite( VMCS_HOST_IDTR_BASE, idtr.base_address );

		tools::vmwrite( VMCS_HOST_SYSENTER_CS, __readmsr( IA32_SYSENTER_CS ) );
		tools::vmwrite( VMCS_HOST_SYSENTER_EIP, __readmsr( IA32_SYSENTER_EIP ) );
		tools::vmwrite( VMCS_HOST_SYSENTER_ESP, __readmsr( IA32_SYSENTER_ESP ) );

		ia32_pat_register pat;
		pat.flags = 0;
		pat.pa0 = MEMORY_TYPE_WRITE_BACK;
		pat.pa1 = MEMORY_TYPE_WRITE_THROUGH;
		pat.pa2 = MEMORY_TYPE_UNCACHEABLE_MINUS;
		pat.pa3 = MEMORY_TYPE_UNCACHEABLE;
		pat.pa4 = MEMORY_TYPE_WRITE_BACK;
		pat.pa5 = MEMORY_TYPE_WRITE_THROUGH;
		pat.pa6 = MEMORY_TYPE_UNCACHEABLE_MINUS;
		pat.pa7 = MEMORY_TYPE_UNCACHEABLE;
		tools::vmwrite( VMCS_HOST_PAT, pat.flags );

		tools::vmwrite( VMCS_CTRL_TSC_OFFSET, 0 );

		//ctl fields

		tools::vmwrite( VMCS_CTRL_CR0_GUEST_HOST_MASK, 0 );
		// Mask CR4 bits 13 (VMXE) and 18 (OSXSAVE) - host controls these
		tools::vmwrite( VMCS_CTRL_CR4_GUEST_HOST_MASK, ( 1ULL << 13 ) | ( 1ULL << 18 ) );
		// Shadow hides VMXE from guest reads (clear bit 13 in shadow)
		uint64_t cr4_shadow = guest_cr4 & ~( 1ULL << 13 );
		tools::vmwrite( VMCS_CTRL_CR4_READ_SHADOW, cr4_shadow );
		tools::vmwrite( VMCS_CTRL_CR0_READ_SHADOW, guest_cr0 );

		tools::vmwrite( VMCS_CTRL_PAGEFAULT_ERROR_CODE_MASK, 0 );
		tools::vmwrite( VMCS_CTRL_PAGEFAULT_ERROR_CODE_MATCH, 0 );
		tools::vmwrite( VMCS_CTRL_EXCEPTION_BITMAP, 0 );
		tools::vmwrite( VMCS_CTRL_VMEXIT_MSR_LOAD_COUNT, 0 );
		tools::vmwrite( VMCS_CTRL_VMENTRY_MSR_LOAD_COUNT, 0 );
		tools::vmwrite( VMCS_CTRL_VMEXIT_MSR_LOAD_ADDRESS, 0 );
		tools::vmwrite( VMCS_CTRL_VMENTRY_MSR_LOAD_ADDRESS, 0 );
		tools::vmwrite( VMCS_CTRL_VMEXIT_MSR_STORE_COUNT, 0 );

		ia32_vmx_basic_register vmx_basic;
		vmx_basic.flags = __readmsr( IA32_VMX_BASIC );

		auto adjust_controls = [ ]( uint32_t controls, uint32_t msr ) -> uint32_t {
			ia32_vmx_true_ctls_register caps;
			caps.flags = __readmsr( msr );
			controls |= caps.allowed_0_settings;   
			controls &= caps.allowed_1_settings;   
			return controls;
			};

		// Pin-based: DON'T force NMI exiting — let hardware must-be-1 bits decide.
		// Forcing NMI exiting creates VM-exits for EVERY NMI (~100s/sec), adding
		// detectable jitter. On most Intel CPUs, NMI exiting is NOT forced.
		// If hardware forces it, also enable virtual NMIs for correct NMI-window behavior.
		ia32_vmx_pinbased_ctls_register pin = {};
		uint32_t pin_msr = vmx_basic.vmx_controls ? IA32_VMX_TRUE_PINBASED_CTLS : IA32_VMX_PINBASED_CTLS;
		uint32_t adjusted_pin = adjust_controls( pin.flags, pin_msr );

		// Check if NMI exiting was forced by must-be-1 bits
		ia32_vmx_pinbased_ctls_register final_pin;
		final_pin.flags = adjusted_pin;
		if ( final_pin.nmi_exiting )
		{
			// Hardware forces NMI exiting — also need virtual NMIs for proper handling
			pin.nmi_exiting = 1;
			pin.virtual_nmi = 1;
			adjusted_pin = adjust_controls( pin.flags, pin_msr );
			final_pin.flags = adjusted_pin;
		}
		bool ext_int_exiting = final_pin.external_interrupt_exiting;
		tools::vmwrite( VMCS_CTRL_PIN_BASED_VM_EXECUTION_CONTROLS, adjusted_pin );

		ia32_vmx_procbased_ctls_register proc = {};
		proc.use_tsc_offsetting = 1;
		proc.use_msr_bitmaps = 1;
		proc.activate_secondary_controls = 1;
		tools::vmwrite( VMCS_CTRL_PROCESSOR_BASED_VM_EXECUTION_CONTROLS,
			adjust_controls( proc.flags,
				vmx_basic.vmx_controls ? IA32_VMX_TRUE_PROCBASED_CTLS : IA32_VMX_PROCBASED_CTLS ) );

		tools::vmwrite( VMCS_CTRL_MSR_BITMAP_ADDRESS, vcpu->msr_bitmap_physical );


		ia32_vmx_procbased_ctls2_register proc2 = {};
		proc2.enable_rdtscp = 1;
		// NOTE: Do NOT set enable_xsaves here.
		// When enable_xsaves=1, XSAVES/XRSTORS cause VM-exits.
		// Our handler was just skipping them (move_rip) without executing,
		// which corrupts FPU/SSE/AVX state → GPU driver crash, screen flicker,
		// EA app crash. With enable_xsaves=0, they execute natively in guest.

		// Only enable EPT if it was successfully initialized
		uint64_t eptp = ept::get_eptp( );
		if ( eptp )
		{
			proc2.enable_ept = 1;
			// VPID is CRITICAL when EPT is enabled - without it, every VM-exit
			// flushes the TLB, causing 2D page walks on every memory access
			// This overhead triggers GPU TDR (VIDEO_TDR_FAILURE 0x116)
			proc2.enable_vpid = 1;
			proc2.enable_invpcid = 1;
		}

		tools::vmwrite( VMCS_CTRL_SECONDARY_PROCESSOR_BASED_VM_EXECUTION_CONTROLS,
			adjust_controls( proc2.flags,
				IA32_VMX_PROCBASED_CTLS2 ) );

		// Set EPT pointer and VPID after secondary controls
		if ( eptp )
		{
			tools::vmwrite( VMCS_CTRL_EPT_POINTER, eptp );
			// VPID must be non-zero when VPID is enabled (use core index + 1)
			uint32_t core = __readgsdword( 0x1A4 );
			tools::vmwrite( VMCS_CTRL_VIRTUAL_PROCESSOR_IDENTIFIER, core + 1 );
		}

		ia32_vmx_exit_ctls_register exit_ctls = {};
		exit_ctls.host_address_space_size = 1;
		// Only acknowledge interrupts if external-interrupt exiting was forced
		// (otherwise external interrupts pass through natively, no VM-exit)
		if ( ext_int_exiting )
			exit_ctls.acknowledge_interrupt_on_exit = 1;
		tools::vmwrite( VMCS_CTRL_PRIMARY_VMEXIT_CONTROLS,
			adjust_controls( exit_ctls.flags,
				vmx_basic.vmx_controls ? IA32_VMX_TRUE_EXIT_CTLS : IA32_VMX_EXIT_CTLS ) );

		ia32_vmx_entry_ctls_register entry_ctls = {};
		entry_ctls.ia32e_mode_guest = 1;
		tools::vmwrite( VMCS_CTRL_VMENTRY_CONTROLS,
			adjust_controls( entry_ctls.flags,
				vmx_basic.vmx_controls ? IA32_VMX_TRUE_ENTRY_CTLS : IA32_VMX_ENTRY_CTLS ) );

		tools::vmwrite( VMCS_GUEST_RSP, guest_rsp );
		tools::vmwrite( VMCS_GUEST_RIP, ( uint64_t ) restore_ctx );

		auto launch_result = __vmx_vmlaunch( );
		// If we reach here, vmlaunch FAILED (success would jump to restore_ctx)
		uint64_t vm_err = tools::vmread( VMCS_VM_INSTRUCTION_ERROR );
		log::dbg_print( "vmlaunch FAILED on core: vmx_result=%d, vm_instruction_error=%llu\n", launch_result, vm_err );

		// Log specific VMCS fields for debugging
		log::dbg_print( "  guest_cr0=0x%llx guest_cr3=0x%llx guest_cr4=0x%llx\n",
			tools::vmread( VMCS_GUEST_CR0 ), tools::vmread( VMCS_GUEST_CR3 ), tools::vmread( VMCS_GUEST_CR4 ) );
		log::dbg_print( "  host_cr0=0x%llx host_cr3=0x%llx host_cr4=0x%llx\n",
			tools::vmread( VMCS_HOST_CR0 ), tools::vmread( VMCS_HOST_CR3 ), tools::vmread( VMCS_HOST_CR4 ) );
		log::dbg_print( "  guest_rip=0x%llx guest_rsp=0x%llx host_rip=0x%llx host_rsp=0x%llx\n",
			tools::vmread( VMCS_GUEST_RIP ), tools::vmread( VMCS_GUEST_RSP ),
			tools::vmread( VMCS_HOST_RIP ), tools::vmread( VMCS_HOST_RSP ) );

		_InterlockedIncrement( &g_vmlaunch_fail_count );
		__vmx_off( );
		return false;
	}
}

#endif // !VMCS
