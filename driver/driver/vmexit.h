#pragma once
#ifndef VMEXIT
#define VMEXIT

#include "handlers.h"
#include "dbg.h"



extern "C" void vm_restore_context( CONTEXT* ctx );
extern "C" void vmresume_fn( );
// Per-core loop detection: tracks consecutive exits and total exit volume
static volatile uint64_t g_last_rip[ 64 ] = {};
static volatile uint32_t g_same_rip_count[ 64 ] = {};
// Volume detector: catches alternating-RIP loops the same-RIP detector misses
static volatile uint32_t g_exit_volume[ 64 ] = {};
static volatile uint64_t g_volume_tsc[ 64 ] = {};

extern "C" void vmentry_handler_cpp( guest_regs_t* regs )
{
    // TSC compensation: measure time spent in VMX root mode
    uint64_t tsc_entry = __rdtsc( );

    unsigned long reason = tools::vmread( VMCS_EXIT_REASON ) & 0xffff;

    uint32_t core = __readgsdword( 0x1A4 );

    // Loop detector: bugcheck if same guest RIP causes 100000+ consecutive exits
    uint64_t guest_rip = tools::vmread( VMCS_GUEST_RIP );
    if ( core < 64 )
    {
        if ( guest_rip == g_last_rip[ core ] )
        {
            if ( ++g_same_rip_count[ core ] > 100000 )
                KeBugCheckEx( 0xDEAD0010, reason, guest_rip, core, g_same_rip_count[ core ] );
        }
        else
        {
            g_last_rip[ core ] = guest_rip;
            g_same_rip_count[ core ] = 0;
        }

        // Volume detector: if >2M exits in ~1 second (~3GHz), something is wrong
        g_exit_volume[ core ]++;
        uint64_t elapsed = tsc_entry - g_volume_tsc[ core ];
        if ( elapsed > 3000000000ULL ) // ~1 second at 3GHz
        {
            if ( g_exit_volume[ core ] > 2000000 )
                KeBugCheckEx( 0xDEAD0012, reason, guest_rip, core, g_exit_volume[ core ] );
            g_exit_volume[ core ] = 0;
            g_volume_tsc[ core ] = tsc_entry;
        }
    }

    // Flag: hide VM-exit overhead from guest TSC for timing-sensitive exits
    // Async events (NMI, interrupts, exceptions) should NOT be hidden —
    // they are natural timing events the guest expects to take time
    bool hide_overhead = false;

    switch ( reason )
    {
    case VMX_EXIT_REASON_EXCEPTION_OR_NMI:
        handlers::handle_exception_or_nmi( core );
        break;
    case VMX_EXIT_REASON_EXTERNAL_INTERRUPT:
        handlers::handle_external_interrupt( core );
        break;
    case VMX_EXIT_REASON_INTERRUPT_WINDOW:
        handlers::handle_interrupt_window( core );
        break;
    case VMX_EXIT_REASON_NMI_WINDOW:
        handlers::handle_nmi_window( core );
        break;
    case VMX_EXIT_REASON_EXECUTE_CPUID:
        handlers::handle_cpuid( regs );
        hide_overhead = true;  // EAC measures rdtsc→cpuid→rdtsc
        break;
    case VMX_EXIT_REASON_MOV_CR:
        handlers::handle_cr_access( regs );
        hide_overhead = true;
        break;
    case VMX_EXIT_REASON_EXECUTE_RDTSCP:
    {
        // Return TSC with current offset applied (consistent with guest's RDTSC)
        uint64_t current_offset = tools::vmread( VMCS_CTRL_TSC_OFFSET );
        unsigned int aux = 0;
        uint64_t tsc = __rdtscp( &aux ) + current_offset;
        regs->rax = tsc & 0xFFFFFFFF;
        regs->rdx = ( tsc >> 32 ) & 0xFFFFFFFF;
        regs->rcx = aux;
        handlers::move_rip( );
        hide_overhead = true;
        break;
    }
    case VMX_EXIT_REASON_EXECUTE_XSETBV:
        handlers::handle_xsetbv( regs );
        hide_overhead = true;
        break;
    case VMX_EXIT_REASON_EXECUTE_GETSEC:
        handlers::handle_getsec( );
        hide_overhead = true;
        break;
    case VMX_EXIT_REASON_EXECUTE_INVD:
        handlers::handle_invd( );
        hide_overhead = true;
        break;
    case VMX_EXIT_REASON_EXECUTE_HLT:
        // Advance RIP past HLT, then put guest in halt state.
        // CPU sleeps until interrupt/NMI arrives — no busy-loop VM-exits.
        // Intel SDM: injecting an event on vmresume auto-wakes from halt.
        handlers::move_rip( );
        tools::vmwrite( VMCS_GUEST_ACTIVITY_STATE, 1 ); // vmx_hlt
        break;
    case VMX_EXIT_REASON_EXECUTE_VMCALL:
        handlers::handle_vmcall( regs );
        hide_overhead = true;
        break;
    case VMX_EXIT_REASON_EXECUTE_WRMSR:
        handlers::handle_wrmsr( regs );
        hide_overhead = true;
        break;
    case VMX_EXIT_REASON_EXECUTE_RDMSR:
        handlers::handle_rdmsr( regs );
        hide_overhead = true;
        break;
    // XSAVES/XRSTORS: no longer intercepted (enable_xsaves=0 in VMCS).
    // Previously skipping these with move_rip() corrupted FPU/SSE/AVX state
    // causing GPU driver crashes and screen flickering.
    case VMX_EXIT_REASON_EXECUTE_XSAVES:
    case VMX_EXIT_REASON_EXECUTE_XRSTORS:
        // Should never reach here with enable_xsaves=0, but handle gracefully
        KeBugCheckEx( 0xDEAD0013, reason, guest_rip, core, 0 );
        break;
    case VMX_EXIT_REASON_EXECUTE_VMXON:
        handlers::handle_vmxon( );
        hide_overhead = true;
        break;
    case VMX_EXIT_REASON_EXECUTE_VMXOFF:
    case VMX_EXIT_REASON_EXECUTE_VMCLEAR:
    case VMX_EXIT_REASON_EXECUTE_VMPTRLD:
    case VMX_EXIT_REASON_EXECUTE_VMPTRST:
    case VMX_EXIT_REASON_EXECUTE_VMREAD:
    case VMX_EXIT_REASON_EXECUTE_VMWRITE:
    case VMX_EXIT_REASON_EXECUTE_VMLAUNCH:
    case VMX_EXIT_REASON_EXECUTE_VMRESUME:
    case VMX_EXIT_REASON_EXECUTE_INVEPT:
    case VMX_EXIT_REASON_EXECUTE_INVVPID:
    case VMX_EXIT_REASON_EXECUTE_VMFUNC:
        handlers::handle_vmx_instruction( );
        hide_overhead = true;
        break;
    case VMX_EXIT_REASON_EPT_VIOLATION:
    {
        uint64_t gpa = tools::vmread( VMCS_GUEST_PHYSICAL_ADDRESS );
        uint64_t qual = tools::vmread( VMCS_EXIT_QUALIFICATION );
        log::dbg_print( "[ept] violation: GPA=0x%llx qual=0x%llx\n", gpa, qual );
        handlers::move_rip( );
        break;
    }
    case VMX_EXIT_REASON_EPT_MISCONFIGURATION:
    {
        uint64_t gpa = tools::vmread( VMCS_GUEST_PHYSICAL_ADDRESS );
        log::dbg_print( "[ept] misconfiguration: GPA=0x%llx\n", gpa );
        resolver::g_KeBugCheck( HYPERVISOR_ERROR );
        break;
    }
    case VMX_EXIT_REASON_TRIPLE_FAULT:
        resolver::g_KeBugCheck( HYPERVISOR_ERROR );
        break;
    default:
        KeBugCheckEx( 0xDEAD0011, reason, guest_rip, core, 0 );
        break;
    }

    // TSC compensation: subtract VM-exit overhead from TSC offset
    // so guest never sees the time spent in VMX root mode.
    // Only applies to timing-sensitive exits (CPUID, MSR, CR, VMX instructions).
    // Async events (NMI, interrupts, exceptions) are left alone — they're natural
    // latency the guest expects, and hiding them would cause detection via drift.
    if ( hide_overhead )
    {
        uint64_t tsc_exit = __rdtsc( );
        // Constant accounts for unmeasured overhead outside our RDTSC pair:
        //   VM-exit HW transition:  ~500 cycles (before tsc_entry)
        //   ASM register save/restore: ~100 cycles (each way)
        //   VM-entry HW transition: ~500 cycles (after tsc_exit)
        //   Minus bare-metal instruction time: ~150 cycles
        //   Net: ~1050 → use 1000 (HLT halt-state fix removed the heavy-feeling issue)
        uint64_t overhead = ( tsc_exit - tsc_entry ) + 1000;
        if ( overhead < 50000 )
        {
            // TSC compensation
            int64_t current_offset = static_cast<int64_t>( tools::vmread( VMCS_CTRL_TSC_OFFSET ) );
            current_offset -= static_cast<int64_t>( overhead );
            tools::vmwrite( VMCS_CTRL_TSC_OFFSET, static_cast<uint64_t>( current_offset ) );

            // APERF/MPERF compensation — critical for defeating APERF-based timing detection
            // EAC measures CPUID cost using IA32_APERF (not just TSC). Without this,
            // even perfect TSC hiding gets caught by the "IET divergence" check.
            if ( core < 64 )
            {
                g_aperf_offset[ core ] -= static_cast<int64_t>( overhead );
                g_mperf_offset[ core ] -= static_cast<int64_t>( overhead );
            }
        }
    }
}

//used for debugging 
//extern "C" void vmentry_handler_cpp( guest_regs_t* guest_regs )
//{
//    unsigned long reason = tools::vmread( VMCS_EXIT_REASON ) & 0xffff;
//    uint64_t vmexits = 0;
//    vmexits++;
//
//    switch ( reason )
//    {
//    //case VMX_EXIT_REASON_EXECUTE_WRMSR:
//    //    handlers::handle_wrmsr( guest_regs );
//    //    break;
//    default:
//        KeBugCheck( 0xdead0000 | reason );
//        break;
//    }
//}



#endif // !VMEXIT
