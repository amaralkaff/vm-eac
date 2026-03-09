#pragma once
#ifndef INIT
#define INIT
#include "dbg.h"
#include "tools.h"
#include "vcpu.h"
#include "vmcs.h"
#include "ept.h"
namespace init
{
	typedef void( ke_generic_call_dpc_t )( PKDEFERRED_ROUTINE, void* );
	typedef void ( ke_signal_call_dpc_sync_t )( void* );
	typedef void ( ke_signal_call_dpc_done_t )( void* );


	struct dpc_context
	{
		void* ke_signal_call_dpc_sync;
		void* ke_signal_call_dpc_done;
		vcpu_t** vcpus;
		volatile long success_count;
		volatile long fail_count;
	};

	extern "C" void capture_ctx( vcpu_t* vcpu );

	void init_routine( PKDPC dpc, void* ctx, void* sysarg1, void* sysarg2 )
	{
		auto* context = static_cast< dpc_context* >( ctx );
		auto sync_dpc = ( ke_signal_call_dpc_sync_t* ) context->ke_signal_call_dpc_sync;
		auto call_done = ( ke_signal_call_dpc_done_t* ) context->ke_signal_call_dpc_done;

		uint64_t current_core = __readgsdword( 0x1A4 ); // KPCR.Number - no API needed

		log::dbg_print( "virtualizing core -> %llu\n", current_core );

		if ( !tools::enable_vmx( ) )
		{
			log::dbg_print( "enable_vmx FAILED on core %llu\n", current_core );
			_InterlockedIncrement( &g_vmlaunch_fail_count );
			sync_dpc( sysarg2 );
			call_done( sysarg1 );
			return;
		}

		log::dbg_print( "enabled vmx on core -> %llu\n", current_core );

		vcpu_t* vcpu = context->vcpus[ current_core ];

		auto vmxon_result = __vmx_on( &vcpu->vmxon_region_physical );
		if ( vmxon_result != 0 )
		{
			log::dbg_print( "vmxon FAILED on core %llu (result=%d)\n", current_core, vmxon_result );
			_InterlockedIncrement( &g_vmlaunch_fail_count );
			sync_dpc( sysarg2 );
			call_done( sysarg1 );
			return;
		}

		log::dbg_print( "entered vmx root mode on core -> %llu\n", current_core );

		if ( __vmx_vmclear( &vcpu->vmcs_region_physical ) != 0 )
		{
			log::dbg_print( "vmclear FAILED on core %llu\n", current_core );
			_InterlockedIncrement( &g_vmlaunch_fail_count );
			__vmx_off( );
			sync_dpc( sysarg2 );
			call_done( sysarg1 );
			return;
		}

		if ( __vmx_vmptrld( &vcpu->vmcs_region_physical ) != 0 )
		{
			log::dbg_print( "vmptrld FAILED on core %llu\n", current_core );
			_InterlockedIncrement( &g_vmlaunch_fail_count );
			__vmx_off( );
			sync_dpc( sysarg2 );
			call_done( sysarg1 );
			return;
		}

		log::dbg_print( "loaded vmcs, launching on core %llu\n", current_core );

		capture_ctx( vcpu );

		// If we reach here, either vmlaunch succeeded (guest returned here)
		// or vmlaunch failed (vmcs_setup_and_launch returned)
		// The fail count is incremented inside vmcs_setup_and_launch on failure

		sync_dpc( sysarg2 );
		call_done( sysarg1 );

		return;
	}

	bool virtualize_all_cores( )
	{
		static void* ke_generic_call_dpc = tools::get_system_routine( L"KeGenericCallDpc" );
		if ( !ke_generic_call_dpc )
			return false;

		uint32_t core_count = resolver::g_KeQueryActiveProcessorCountEx( ALL_PROCESSOR_GROUPS );

		vcpu_t** vcpus = ( vcpu_t** ) resolver::g_ExAllocatePool( NonPagedPool, sizeof( vcpu_t* ) * core_count );

		for ( uint32_t i = 0; i < core_count; i++ )
		{
			vcpus[ i ] = vcpu::allocate_vcpu( );
			if ( !vcpus[ i ] ) return false;
		}

		// Hide VMX-signature pages from guest via EPT
		// VMCS and VMXON contain VMX revision IDs that anti-cheat could scan for
		if ( ept::g_ept )
		{
			for ( uint32_t i = 0; i < core_count; i++ )
			{
				ept::hide_allocation( vcpus[i]->vmcs_region, sizeof( vmcs_t ) );
				ept::hide_allocation( vcpus[i]->vmxon_region, sizeof( vmxon_t ) );
			}
			log::dbg_print( "[ept] Hidden %u VMCS + VMXON regions\n", core_count );
		}

		g_vmlaunch_fail_count = 0;

		dpc_context ctx{
			tools::get_system_routine( L"KeSignalCallDpcSynchronize" ),
			tools::get_system_routine( L"KeSignalCallDpcDone" ),
			vcpus,
			0, 0
		};

		ke_generic_call_dpc_t* send_dpc = ( ke_generic_call_dpc_t* ) ke_generic_call_dpc;

		send_dpc( init::init_routine, &ctx );

		if ( g_vmlaunch_fail_count > 0 )
		{
			log::dbg_print( "[hv] VMLAUNCH failed on %ld / %u cores\n", g_vmlaunch_fail_count, core_count );
			return false;
		}

		log::dbg_print( "[hv] All %u cores virtualized successfully\n", core_count );
		return true;
	}
}

#endif // !INIT
