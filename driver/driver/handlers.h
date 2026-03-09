#pragma once
#include "tools.h"
#include "ia32.h"
#include "structs.h"
#include "comms.h"
#include "mem.h"
#include "cr3.h"

static uint32_t g_queued_nmis[ 64 ] = {};
static uint32_t g_pending_interrupt[ 64 ] = {};  // Per-core pending external interrupt vector
static bool g_has_pending_interrupt[ 64 ] = {};   // Whether there's a pending interrupt to inject

// Per-core APERF/MPERF offsets for hiding VM-exit overhead in performance counters
static volatile int64_t g_aperf_offset[ 64 ] = {};
static volatile int64_t g_mperf_offset[ 64 ] = {};

// Global target state shared across all VCPUs
inline hv_target_state g_target = {};

// Shared memory page for bulk reads (physical address)
inline volatile uint64_t g_shared_page_phys = 0;
inline volatile uint64_t g_caller_cr3 = 0;


namespace handlers
{
    extern "C"
    {
        void memcpy_s( exception_info_t& e, void* dst, void const* src, size_t size );

        void xsetbv_s( exception_info_t& e, uint32_t idx, uint64_t value );

        void wrmsr_s( exception_info_t& e, uint32_t msr, uint64_t value );

        uint64_t rdmsr_s( exception_info_t& e, uint32_t msr );
    }

    inline bool is_canonical( uint64_t addr )
    {
        return ( ( int64_t ) addr >> 47 ) == 0 || ( ( int64_t ) addr >> 47 ) == -1;
    }

    inline bool is_valid_msr( uint32_t msr )
    {
        return ( msr <= 0x1FFF ) || ( msr >= 0xC0000000 && msr <= 0xC0001FFF );
    }

    inline void move_rip( )
    {
        uint64_t rip = tools::vmread( VMCS_GUEST_RIP );
        uint64_t len = tools::vmread( VMCS_VMEXIT_INSTRUCTION_LENGTH );
        tools::vmwrite( VMCS_GUEST_RIP, rip + len );
    }

    inline void inject_hw( uint32_t vector, bool has_error = false, uint32_t error = 0 )
    {
        vmentry_interrupt_information interrupt = {};
        interrupt.vector = vector;
        interrupt.interruption_type = 3;
        interrupt.deliver_error_code = has_error ? 1 : 0;
        interrupt.valid = 1;
        tools::vmwrite( VMCS_CTRL_VMENTRY_INTERRUPTION_INFORMATION_FIELD, interrupt.flags );
        if ( has_error )
            tools::vmwrite( VMCS_CTRL_VMENTRY_EXCEPTION_ERROR_CODE, error );
    }

    inline void inject_nmi( )
    {
        vmentry_interrupt_information nmi = {};
        nmi.vector = 2;
        nmi.interruption_type = 2;
        nmi.valid = 1;
        tools::vmwrite( VMCS_CTRL_VMENTRY_INTERRUPTION_INFORMATION_FIELD, nmi.flags );
    }

    inline void inject_external_interrupt( uint32_t vector )
    {
        vmentry_interrupt_information intr = {};
        intr.vector = vector;
        intr.interruption_type = 0;  // external interrupt
        intr.valid = 1;
        tools::vmwrite( VMCS_CTRL_VMENTRY_INTERRUPTION_INFORMATION_FIELD, intr.flags );
    }

    inline bool guest_can_accept_interrupt( )
    {
        uint64_t rflags = tools::vmread( VMCS_GUEST_RFLAGS );
        if ( !( rflags & ( 1ULL << 9 ) ) )  // IF flag
            return false;
        uint64_t interruptibility = tools::vmread( VMCS_GUEST_INTERRUPTIBILITY_STATE );
        if ( interruptibility & 0x3 )  // blocking by STI or MOV SS
            return false;
        return true;
    }

    // ---------------------------------------------------------------
    // Hypercall dispatch - processes commands from usermode
    // ---------------------------------------------------------------
    inline void dispatch_hypercall( guest_regs_t* regs )
    {
        uint32_t cmd = static_cast<uint32_t>( regs->rcx );

        switch ( cmd )
        {
        case HV_CMD_PING:
        {
            regs->rax = HV_STATUS_SUCCESS;
            regs->rbx = HV_MAGIC;
            regs->rcx = 0;
            regs->rdx = 0;
            break;
        }

        case HV_CMD_SET_TARGET:
        {
            uint32_t pid = static_cast<uint32_t>( regs->rdx );

            // Resolve CR3 for the target process
            uint64_t cr3 = cr3_resolver::get_process_cr3( pid );
            uint64_t base = cr3_resolver::get_process_base( pid );

            if ( cr3 && base )
            {
                g_target.pid = pid;
                g_target.cr3 = cr3;
                g_target.base_address = base;
                g_target.initialized = true;

                regs->rax = HV_STATUS_SUCCESS;
                regs->rbx = cr3;
                regs->rcx = base;
            }
            else
            {
                regs->rax = HV_STATUS_FAILED;
                regs->rbx = 0;
                regs->rcx = 0;
            }
            regs->rdx = 0;
            break;
        }

        case HV_CMD_READ_VIRT:
        {
            if ( !g_target.initialized || !g_target.cr3 )
            {
                regs->rax = HV_STATUS_NOT_INIT;
                regs->rbx = 0;
                break;
            }

            uint64_t address = regs->rdx;
            uint32_t size = static_cast<uint32_t>( regs->rbx );

            // For register-based return, max 8 bytes
            if ( size > 8 ) size = 8;

            uint64_t data = 0;
            if ( mem::read_virtual( g_target.cr3, address, &data, size ) )
            {
                regs->rax = HV_STATUS_SUCCESS;
                regs->rbx = data;
            }
            else
            {
                regs->rax = HV_STATUS_FAILED;
                regs->rbx = 0;
            }
            regs->rcx = 0;
            regs->rdx = 0;
            break;
        }

        case HV_CMD_WRITE_VIRT:
        {
            if ( !g_target.initialized || !g_target.cr3 )
            {
                regs->rax = HV_STATUS_NOT_INIT;
                break;
            }

            uint64_t address = regs->rdx;
            uint64_t value = regs->rbx;
            uint32_t size = static_cast<uint32_t>( regs->r8 );

            if ( size > 8 ) size = 8;

            if ( mem::write_virtual( g_target.cr3, address, &value, size ) )
                regs->rax = HV_STATUS_SUCCESS;
            else
                regs->rax = HV_STATUS_FAILED;

            regs->rbx = 0;
            regs->rcx = 0;
            regs->rdx = 0;
            break;
        }

        case HV_CMD_READ_PHYS:
        {
            uint64_t phys_addr = regs->rdx;
            uint32_t size = static_cast<uint32_t>( regs->rbx );
            if ( size > 8 ) size = 8;

            uint64_t data = 0;
            if ( mem::read_physical( phys_addr, &data, size ) )
            {
                regs->rax = HV_STATUS_SUCCESS;
                regs->rbx = data;
            }
            else
            {
                regs->rax = HV_STATUS_FAILED;
                regs->rbx = 0;
            }
            regs->rcx = 0;
            regs->rdx = 0;
            break;
        }

        case HV_CMD_GET_CR3:
        {
            uint32_t pid = static_cast<uint32_t>( regs->rdx );
            uint64_t cr3 = cr3_resolver::get_process_cr3( pid );

            regs->rax = cr3 ? HV_STATUS_SUCCESS : HV_STATUS_FAILED;
            regs->rbx = cr3;
            regs->rcx = 0;
            regs->rdx = 0;
            break;
        }

        case HV_CMD_GET_BASE:
        {
            uint32_t pid = static_cast<uint32_t>( regs->rdx );
            uint64_t base = cr3_resolver::get_process_base( pid );

            regs->rax = base ? HV_STATUS_SUCCESS : HV_STATUS_FAILED;
            regs->rbx = base;
            regs->rcx = 0;
            regs->rdx = 0;
            break;
        }

        case HV_CMD_REGISTER_PAGE:
        {
            // Register shared memory page for bulk operations
            // RDX = usermode virtual address of the page
            uint64_t user_va = regs->rdx;

            // Get the caller's CR3 from VMCS (at VM-exit, this is the guest's current CR3)
            uint64_t caller_cr3 = tools::vmread( VMCS_GUEST_CR3 );

            // Translate the usermode VA to physical address
            uint64_t phys = mem::translate_va( caller_cr3, user_va );
            if ( phys )
            {
                g_shared_page_phys = phys;
                g_caller_cr3 = caller_cr3;
                regs->rax = HV_STATUS_SUCCESS;
                regs->rbx = phys;
            }
            else
            {
                regs->rax = HV_STATUS_FAILED;
                regs->rbx = 0;
            }
            regs->rcx = 0;
            regs->rdx = 0;
            break;
        }

        case HV_CMD_BULK_READ:
        {
            if ( !g_shared_page_phys || !g_target.initialized || !g_target.cr3 )
            {
                regs->rax = HV_STATUS_NOT_INIT;
                break;
            }

            // Read the request header from the shared page
            bulk_read_request req = {};
            if ( !mem::read_physical( g_shared_page_phys, &req, sizeof( req ) ) )
            {
                regs->rax = HV_STATUS_FAILED;
                break;
            }

            if ( req.size == 0 || req.size > SHARED_PAGE_MAX_DATA )
            {
                regs->rax = HV_STATUS_FAILED;
                break;
            }

            // Read target process memory directly into shared page (no temp buffer)
            // Uses MmMapIoSpaceEx to map shared page, then MmCopyMemory into it
            // This avoids a 4KB stack allocation that was overflowing the 24KB VMM stack
            uint64_t data_phys = g_shared_page_phys + SHARED_PAGE_HEADER_SIZE;
            size_t offset = 0;

            // Map the shared page data region once for writing
            PHYSICAL_ADDRESS shared_pa = {};
            shared_pa.QuadPart = static_cast<LONGLONG>( data_phys );
            PVOID shared_mapped = resolver::g_MmMapIoSpaceEx( shared_pa, SHARED_PAGE_MAX_DATA, PAGE_READWRITE );
            if ( !shared_mapped )
            {
                regs->rax = HV_STATUS_FAILED;
                uint32_t fail = 1;
                mem::write_physical( g_shared_page_phys + offsetof( bulk_read_request, status ), &fail, sizeof( fail ) );
                goto bulk_done;
            }

            while ( offset < req.size )
            {
                uint64_t phys = mem::translate_va( g_target.cr3, req.target_va + offset );
                if ( !phys )
                {
                    regs->rax = HV_STATUS_FAILED;
                    uint32_t fail = 1;
                    mem::write_physical( g_shared_page_phys + offsetof( bulk_read_request, status ), &fail, sizeof( fail ) );
                    resolver::g_MmUnmapIoSpace( shared_mapped, SHARED_PAGE_MAX_DATA );
                    goto bulk_done;
                }

                uint64_t page_rem = 0x1000 - ( phys & 0xFFF );
                size_t chunk = req.size - offset;
                if ( chunk > page_rem )
                    chunk = static_cast<size_t>( page_rem );

                // Read from target physical directly into the mapped shared page
                MM_COPY_ADDRESS src_addr = {};
                src_addr.PhysicalAddress.QuadPart = static_cast<LONGLONG>( phys );
                SIZE_T bytes_read = 0;
                NTSTATUS status = resolver::g_MmCopyMemory(
                    reinterpret_cast<uint8_t*>( shared_mapped ) + offset,
                    src_addr, chunk, MM_COPY_MEMORY_PHYSICAL, &bytes_read );

                if ( !NT_SUCCESS( status ) || bytes_read != chunk )
                {
                    regs->rax = HV_STATUS_FAILED;
                    uint32_t fail = 1;
                    mem::write_physical( g_shared_page_phys + offsetof( bulk_read_request, status ), &fail, sizeof( fail ) );
                    resolver::g_MmUnmapIoSpace( shared_mapped, SHARED_PAGE_MAX_DATA );
                    goto bulk_done;
                }

                offset += chunk;
            }

            resolver::g_MmUnmapIoSpace( shared_mapped, SHARED_PAGE_MAX_DATA );

            // Write success status
            {
                uint32_t ok = 0;
                mem::write_physical( g_shared_page_phys + offsetof( bulk_read_request, status ), &ok, sizeof( ok ) );
                regs->rax = HV_STATUS_SUCCESS;
                regs->rbx = req.size;
            }

        bulk_done:
            regs->rcx = 0;
            regs->rdx = 0;
            break;
        }

        default:
            regs->rax = HV_STATUS_INVALID_CMD;
            regs->rbx = 0;
            regs->rcx = 0;
            regs->rdx = 0;
            break;
        }
    }

    // ---------------------------------------------------------------
    // CPUID exit handler - intercepts hypercalls + hides hypervisor
    // ---------------------------------------------------------------
    inline void handle_cpuid( guest_regs_t* regs )
    {
        uint32_t leaf = static_cast<uint32_t>( regs->rax );

        // Check for our secret hypercall leaf
        if ( leaf == HV_CPUID_LEAF )
        {
            // Authenticate caller: R8 or R9 must contain HV_MAGIC
            // Our ASM stubs set R8 (standard) or R9 (extended) before CPUID
            // Without magic, return normal CPUID output (stay invisible)
            if ( regs->r8 != HV_MAGIC && regs->r9 != HV_MAGIC )
            {
                int result[4] = {};
                __cpuidex( result, 0, 0 );
                regs->rax = static_cast<uint32_t>( result[0] );
                regs->rbx = static_cast<uint32_t>( result[1] );
                regs->rcx = static_cast<uint32_t>( result[2] );
                regs->rdx = static_cast<uint32_t>( result[3] );
                move_rip( );
                return;
            }

            dispatch_hypercall( regs );
            move_rip( );
            return;
        }

        // Execute real CPUID
        int result[ 4 ] = {};
        __cpuidex( result, static_cast<int>( regs->rax ), static_cast<int>( regs->rcx ) );

        // Anti-detection: HIDE hypervisor present bit (CPUID.1:ECX bit 31)
        if ( leaf == 1 )
            result[ 2 ] &= ~( 1u << 31 );

        // Anti-detection: pass through 0x40000000+ leaves without revealing ourselves
        // Do NOT advertise a hypervisor vendor string

        regs->rax = static_cast<uint32_t>( result[ 0 ] );
        regs->rbx = static_cast<uint32_t>( result[ 1 ] );
        regs->rcx = static_cast<uint32_t>( result[ 2 ] );
        regs->rdx = static_cast<uint32_t>( result[ 3 ] );
        move_rip( );
    }

    inline void handle_rdmsr( guest_regs_t* regs )
    {
        uint32_t msr_idx = static_cast<uint32_t>( regs->rcx );

        // Anti-detection: hide VMX enable in IA32_FEATURE_CONTROL
        // Shows "BIOS disabled VMX" — makes VMXON VMfailInvalid consistent
        if ( msr_idx == IA32_FEATURE_CONTROL )
        {
            LARGE_INTEGER val;
            val.QuadPart = __readmsr( msr_idx );
            // Clear VMX enable bits (bit 1 = inside SMX, bit 2 = outside SMX)
            // Keep lock bit (bit 0) intact — locked + VMX disabled = valid BIOS config
            val.QuadPart &= ~0x6ULL;
            regs->rax = val.LowPart;
            regs->rdx = val.HighPart;
            move_rip( );
            return;
        }

        // Anti-detection: hide VM-exit overhead in APERF/MPERF
        // EAC uses APERF-based timing (not just TSC) to detect hypervisors.
        // Return real counter value minus accumulated VM-exit overhead.
        if ( msr_idx == 0xE8 || msr_idx == 0xE7 )  // IA32_APERF or IA32_MPERF
        {
            uint32_t core = __readgsdword( 0x1A4 );
            int64_t real_val = static_cast<int64_t>( __readmsr( msr_idx ) );
            if ( core < 64 )
            {
                int64_t offset = ( msr_idx == 0xE8 ) ? g_aperf_offset[ core ] : g_mperf_offset[ core ];
                real_val += offset;
                if ( real_val < 0 ) real_val = 0;
            }
            regs->rax = static_cast<uint32_t>( real_val );
            regs->rdx = static_cast<uint32_t>( real_val >> 32 );
            move_rip( );
            return;
        }

        // Generic RDMSR passthrough — read the real MSR value.
        // Use SEH to catch #GP from invalid/reserved MSRs and inject #GP to guest.
        // Previously, MSRs outside the whitelist silently returned 0, causing
        // kernel retry loops that hit the loop detector (0xDEAD0010).
        LARGE_INTEGER msr = { 0 };
        bool gp_fault = false;

        __try
        {
            msr.QuadPart = __readmsr( static_cast<uint32_t>( regs->rcx ) );
        }
        __except ( 1 )
        {
            gp_fault = true;
        }

        if ( gp_fault )
        {
            // Inject #GP(0) to guest — same behavior as bare metal for invalid MSR
            inject_hw( 13, true, 0 );
            return;
        }

        regs->rax = msr.LowPart;
        regs->rdx = msr.HighPart;

        move_rip( );

        return;
    }



    inline void handle_wrmsr( guest_regs_t* regs )
    {
        uint32_t msr_idx = static_cast<uint32_t>( regs->rcx );

        // Reset APERF/MPERF offset when OS writes to these counters
        // (OS periodically writes 0 to reset for frequency calculation)
        if ( msr_idx == 0xE8 || msr_idx == 0xE7 )
        {
            uint32_t core = __readgsdword( 0x1A4 );
            if ( core < 64 )
            {
                if ( msr_idx == 0xE8 ) g_aperf_offset[ core ] = 0;
                else g_mperf_offset[ core ] = 0;
            }
            // Pass through the write
            __writemsr( msr_idx, ( static_cast<uint64_t>( regs->rdx ) << 32 ) | ( regs->rax & 0xFFFFFFFF ) );
            move_rip( );
            return;
        }

        // Generic WRMSR passthrough with #GP injection for invalid MSRs
        LARGE_INTEGER msr;
        msr.LowPart = ( ULONG ) regs->rax;
        msr.HighPart = ( ULONG ) regs->rdx;
        bool gp_fault = false;

        __try
        {
            __writemsr( static_cast<uint32_t>( regs->rcx ), msr.QuadPart );
        }
        __except ( 1 )
        {
            gp_fault = true;
        }

        if ( gp_fault )
        {
            inject_hw( 13, true, 0 );
            return;
        }

        move_rip( );

        return;
    }

    inline void handle_cr_access( guest_regs_t* regs )
    {
        uint64_t qualification = tools::vmread( VMCS_EXIT_QUALIFICATION );
        uint64_t cr_num = qualification & 0xF;
        uint64_t access = ( qualification >> 4 ) & 0x3;
        uint64_t gpr_idx = ( qualification >> 8 ) & 0xF;

        auto read_gpr = [&]( uint64_t idx ) -> uint64_t
            {
                switch ( idx )
                {
                case 0:  return regs->rax;
                case 1:  return regs->rcx;
                case 2:  return regs->rdx;
                case 3:  return regs->rbx;
                case 4:  return tools::vmread( VMCS_GUEST_RSP );
                case 5:  return regs->rbp;
                case 6:  return regs->rsi;
                case 7:  return regs->rdi;
                case 8:  return regs->r8;
                case 9:  return regs->r9;
                case 10: return regs->r10;
                case 11: return regs->r11;
                case 12: return regs->r12;
                case 13: return regs->r13;
                case 14: return regs->r14;
                case 15: return regs->r15;
                default: return 0;
                }
            };

        auto write_gpr = [&]( uint64_t idx, uint64_t val )
            {
                switch ( idx )
                {
                case 0:  regs->rax = val; break;
                case 1:  regs->rcx = val; break;
                case 2:  regs->rdx = val; break;
                case 3:  regs->rbx = val; break;
                case 4:  tools::vmwrite( VMCS_GUEST_RSP, val ); break;
                case 5:  regs->rbp = val; break;
                case 6:  regs->rsi = val; break;
                case 7:  regs->rdi = val; break;
                case 8:  regs->r8 = val; break;
                case 9:  regs->r9 = val; break;
                case 10: regs->r10 = val; break;
                case 11: regs->r11 = val; break;
                case 12: regs->r12 = val; break;
                case 13: regs->r13 = val; break;
                case 14: regs->r14 = val; break;
                case 15: regs->r15 = val; break;
                }
            };

        if ( access == 0 ) 
        {
            uint64_t val = read_gpr( gpr_idx );
            switch ( cr_num )
            {
            case 0:
            {
                cr0 new_cr0;
                new_cr0.flags = val;
                if ( !new_cr0.paging_enable || !new_cr0.protection_enable )
                {
                    inject_hw( 13, true, 0 );
                    return;
                }
                // Apply VMX fixed bits to guest CR0
                uint64_t cr0_fixed0 = __readmsr( IA32_VMX_CR0_FIXED0 );
                uint64_t cr0_fixed1 = __readmsr( IA32_VMX_CR0_FIXED1 );
                tools::vmwrite( VMCS_CTRL_CR0_READ_SHADOW, val );
                val |= cr0_fixed0;
                val &= cr0_fixed1;
                tools::vmwrite( VMCS_GUEST_CR0, val );
                break;
            }
            case 3:
                tools::vmwrite( VMCS_GUEST_CR3, val );
                break;
            case 4:
            {
                cr4 new_cr4;
                new_cr4.flags = val;
                if ( !new_cr4.physical_address_extension )
                {
                    inject_hw( 13, true, 0 );
                    return;
                }


                uint64_t cr4_fixed0 = __readmsr( IA32_VMX_CR4_FIXED0 );
                uint64_t cr4_fixed1 = __readmsr( IA32_VMX_CR4_FIXED1 );
                tools::vmwrite( VMCS_CTRL_CR4_READ_SHADOW, new_cr4.flags );

                new_cr4.flags |= cr4_fixed0;
                new_cr4.flags &= cr4_fixed1;
                tools::vmwrite( VMCS_GUEST_CR4, new_cr4.flags );
                break;
            }
            }
        }
        else if ( access == 1 ) 
        {
            uint64_t val = 0;
            switch ( cr_num )
            {
            case 0: val = tools::vmread( VMCS_GUEST_CR0 ); break;
            case 3: val = tools::vmread( VMCS_GUEST_CR3 ); break;
            case 4: val = tools::vmread( VMCS_GUEST_CR4 ); break;
            }
            write_gpr( gpr_idx, val );
        }
        else if ( access == 2 )
        {
            // CLTS: clear Task Switched flag in guest CR0
            uint64_t cr0_val = tools::vmread( VMCS_GUEST_CR0 );
            cr0_val &= ~CR0_TASK_SWITCHED_FLAG;
            tools::vmwrite( VMCS_GUEST_CR0, cr0_val );
            // Shadow shows guest-visible value (TS cleared)
            uint64_t shadow = tools::vmread( VMCS_CTRL_CR0_READ_SHADOW );
            shadow &= ~CR0_TASK_SWITCHED_FLAG;
            tools::vmwrite( VMCS_CTRL_CR0_READ_SHADOW, shadow );
        }
        
        move_rip( );
    }

    inline void handle_xsetbv( guest_regs_t* regs )
    {

        auto mask = tools::vmread( VMCS_CTRL_CR4_GUEST_HOST_MASK );

        cr4 cr4;
        cr4.flags = ( tools::vmread( VMCS_CTRL_CR4_READ_SHADOW ) & mask ) | ( tools::vmread( VMCS_GUEST_CR4 ) & ~mask );

        if ( !cr4.os_xsave )
        {
            inject_hw( invalid_opcode );
            return;
        }

        if ( regs->rcx != 0 )
        {
            inject_hw( general_protection );
            return;
        }

        xcr0 new_xcr0;
        new_xcr0.flags = ( regs->rdx << 32 ) | regs->rax;

        cpuid_eax_0d_ecx_00 cpuid_0d;
        __cpuidex( reinterpret_cast< int* >( &cpuid_0d ), 0x0D, 0x00 );

        uint64_t xcr0_unsupported_mask = ~( ( static_cast< uint64_t >(
            cpuid_0d.edx.flags ) << 32 ) | cpuid_0d.eax.flags );


        if ( new_xcr0.flags & xcr0_unsupported_mask ) {
            inject_hw( general_protection );
            return;
        }

        if ( !new_xcr0.x87 ) {
            inject_hw( general_protection );
            return;
        }

        if ( new_xcr0.avx && !new_xcr0.sse ) {
            inject_hw( general_protection );
            return;
        }

        if ( !new_xcr0.avx && ( new_xcr0.opmask || new_xcr0.zmm_hi256 || new_xcr0.zmm_hi16 ) ) {
            inject_hw( general_protection );
            return;
        }

        if ( new_xcr0.bndreg != new_xcr0.bndcsr ) {
            inject_hw( general_protection );
            return;
        }

        if ( new_xcr0.opmask != new_xcr0.zmm_hi256 || new_xcr0.zmm_hi256 != new_xcr0.zmm_hi16 ) {
            inject_hw( general_protection );
            return;
        }

        _xsetbv( ( uint32_t ) regs->rcx, new_xcr0.flags );

        move_rip( );
    }

    inline void handle_getsec( )
    {
        inject_hw( 13, true, 0 );
    }

    inline void handle_invd( )
    {
        __wbinvd( );
        move_rip( );
    }

    inline void handle_vmx_instruction( )
    {
        inject_hw( 6 ); // #ud — correct for non-VMXON VMX instructions outside VMX operation
    }

    // VMXON requires special handling to avoid detection:
    // - CR4.VMXE=0 → #UD (correct, same as bare metal)
    // - CR4.VMXE=1 → VMfailInvalid (CF=1, advance RIP)
    //   On bare metal, VMXON with a bad VMXON region returns VMfailInvalid.
    //   Injecting #UD when VMXE=1 is IMPOSSIBLE on real hardware → instant detection.
    inline void handle_vmxon( )
    {
        // Check guest-visible CR4.VMXE from the read shadow
        uint64_t cr4_shadow = tools::vmread( VMCS_CTRL_CR4_READ_SHADOW );
        if ( !( cr4_shadow & ( 1ULL << 13 ) ) )
        {
            // CR4.VMXE=0 → #UD (same as bare metal)
            inject_hw( 6 );
            return;
        }

        // CR4.VMXE=1 → emulate VMfailInvalid
        // Set CF=1, clear ZF/PF/AF/SF/OF in RFLAGS
        uint64_t rflags = tools::vmread( VMCS_GUEST_RFLAGS );
        rflags |= ( 1ULL << 0 );   // CF = 1
        rflags &= ~( 1ULL << 6 );  // ZF = 0
        rflags &= ~( 1ULL << 2 );  // PF = 0
        rflags &= ~( 1ULL << 4 );  // AF = 0
        rflags &= ~( 1ULL << 7 );  // SF = 0
        rflags &= ~( 1ULL << 11 ); // OF = 0
        tools::vmwrite( VMCS_GUEST_RFLAGS, rflags );

        move_rip( );
    }

    inline void handle_vmcall( guest_regs_t* regs )
    {
        // Check if this is our hypercall (same protocol as CPUID)
        if ( static_cast<uint32_t>( regs->rax ) == HV_CPUID_LEAF )
        {
            // Authenticate: require magic in R8 or R9
            if ( regs->r8 != HV_MAGIC && regs->r9 != HV_MAGIC )
            {
                inject_hw( 6 ); // reject unauthenticated calls
                return;
            }

            dispatch_hypercall( regs );
            move_rip( );
            return;
        }

        // Not ours - inject #UD to the guest (VMX instructions in guest cause #UD)
        inject_hw( 6 ); // #ud
    }

    void handle_nmi_window( uint64_t core )
    {
        if ( g_queued_nmis[ core ] == 0 )
        {
            // Safety: disable NMI-window exiting to prevent infinite loop
            ia32_vmx_procbased_ctls_register procbased;
            procbased.flags = tools::vmread( VMCS_CTRL_PROCESSOR_BASED_VM_EXECUTION_CONTROLS );
            procbased.nmi_window_exiting = 0;
            tools::vmwrite( VMCS_CTRL_PROCESSOR_BASED_VM_EXECUTION_CONTROLS, procbased.flags );
            return;
        }

        g_queued_nmis[ core ]--;
        inject_nmi( );

        // Disable NMI-window exiting when queue is drained
        if ( g_queued_nmis[ core ] == 0 )
        {
            ia32_vmx_procbased_ctls_register procbased;
            procbased.flags = tools::vmread( VMCS_CTRL_PROCESSOR_BASED_VM_EXECUTION_CONTROLS );
            procbased.nmi_window_exiting = 0;
            tools::vmwrite( VMCS_CTRL_PROCESSOR_BASED_VM_EXECUTION_CONTROLS, procbased.flags );
        }
    }

    void handle_external_interrupt( uint64_t core )
    {
        // With acknowledge_interrupt_on_exit=1, the interrupt vector is in exit info
        vmentry_interrupt_information info;
        info.flags = tools::vmread( VMCS_VMEXIT_INTERRUPTION_INFORMATION );

        if ( !info.valid )
            return;

        uint32_t vector = info.vector;

        // Try to inject immediately if guest can accept
        if ( guest_can_accept_interrupt( ) )
        {
            inject_external_interrupt( vector );
            return;
        }

        // Guest can't accept right now — save and wait for interrupt window
        g_pending_interrupt[ core ] = vector;
        g_has_pending_interrupt[ core ] = true;

        ia32_vmx_procbased_ctls_register procbased;
        procbased.flags = tools::vmread( VMCS_CTRL_PROCESSOR_BASED_VM_EXECUTION_CONTROLS );
        procbased.interrupt_window_exiting = 1;
        tools::vmwrite( VMCS_CTRL_PROCESSOR_BASED_VM_EXECUTION_CONTROLS, procbased.flags );
    }

    void handle_interrupt_window( uint64_t core )
    {
        // Disable interrupt-window exiting first
        ia32_vmx_procbased_ctls_register procbased;
        procbased.flags = tools::vmread( VMCS_CTRL_PROCESSOR_BASED_VM_EXECUTION_CONTROLS );
        procbased.interrupt_window_exiting = 0;
        tools::vmwrite( VMCS_CTRL_PROCESSOR_BASED_VM_EXECUTION_CONTROLS, procbased.flags );

        // Inject the saved interrupt
        if ( g_has_pending_interrupt[ core ] )
        {
            inject_external_interrupt( g_pending_interrupt[ core ] );
            g_has_pending_interrupt[ core ] = false;
        }
    }

    inline void handle_exception_or_nmi( uint64_t core )
    {
        vmentry_interrupt_information info;
        info.flags = tools::vmread( VMCS_VMEXIT_INTERRUPTION_INFORMATION );

        // NMI (type 2, vector 2): queue and re-inject via NMI window
        if ( info.interruption_type == 2 )
        {
            g_queued_nmis[ core ]++;

            ia32_vmx_procbased_ctls_register procbased;
            procbased.flags = tools::vmread( VMCS_CTRL_PROCESSOR_BASED_VM_EXECUTION_CONTROLS );
            procbased.nmi_window_exiting = 1;
            tools::vmwrite( VMCS_CTRL_PROCESSOR_BASED_VM_EXECUTION_CONTROLS, procbased.flags );
            return;
        }

        // Hardware exception (type 3) or software exception (type 6):
        // Re-inject with original type preserved
        vmentry_interrupt_information reinject = {};
        reinject.vector = info.vector;
        reinject.interruption_type = info.interruption_type;
        reinject.deliver_error_code = info.deliver_error_code;
        reinject.valid = 1;
        tools::vmwrite( VMCS_CTRL_VMENTRY_INTERRUPTION_INFORMATION_FIELD, reinject.flags );

        if ( info.deliver_error_code )
            tools::vmwrite( VMCS_CTRL_VMENTRY_EXCEPTION_ERROR_CODE,
                tools::vmread( VMCS_VMEXIT_INTERRUPTION_ERROR_CODE ) );

        // Software exceptions (INT3/INTO) require instruction length for re-injection
        if ( info.interruption_type == 4 || info.interruption_type == 5 || info.interruption_type == 6 )
            tools::vmwrite( VMCS_CTRL_VMENTRY_INSTRUCTION_LENGTH,
                tools::vmread( VMCS_VMEXIT_INSTRUCTION_LENGTH ) );
    }
}