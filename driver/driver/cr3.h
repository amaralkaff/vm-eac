#pragma once
#ifndef CR3_H
#define CR3_H

#include "tools.h"
#include "mem.h"
#include "resolver.h"
#include "structs.h"
#include <ntifs.h>
#include <ntimage.h>
#include <cstdint>

//
// CR3 / DirBase resolution
// Finds the real CR3 for a process by enumerating MMPFN entries
// Bypasses CR3 randomization on newer Windows versions
// Ported from km/driver.cpp
//

namespace cr3_resolver
{
    // MMPFN structure (minimal fields we need)
    struct mmpfn_entry
    {
        uintptr_t flags;
        uintptr_t pte_address;
        uintptr_t unused_1;
        uintptr_t unused_2;
        uintptr_t unused_3;
        uintptr_t unused_4;
    };

    // Globals initialized once during driver_entry (before virtualization)
    inline uint64_t g_mm_pfn_database = 0;
    inline uint64_t g_pte_base = 0;
    inline uint64_t g_pde_base = 0;
    inline uint64_t g_ppe_base = 0;
    inline uint64_t g_pxe_base = 0;
    inline uint64_t g_self_map_idx = 0;

    // ---------------------------------------------------------------
    // Pattern scan in a PE section (kernel image)
    // ---------------------------------------------------------------
    inline intptr_t search_pattern( void* module_handle, const char* section, const char* sig )
    {
        auto in_range = []( auto x, auto a, auto b ) { return ( x >= a && x <= b ); };
        auto get_bits = [&]( auto x ) { return ( in_range( ( x & ( ~0x20 ) ), 'A', 'F' ) ? ( ( x & ( ~0x20 ) ) - 'A' + 0xA ) : ( in_range( x, '0', '9' ) ? x - '0' : 0 ) ); };
        auto get_byte = [&]( auto x ) { return ( get_bits( x[0] ) << 4 | get_bits( x[1] ) ); };

        auto dos = reinterpret_cast<PIMAGE_DOS_HEADER>( module_handle );
        auto nt  = reinterpret_cast<PIMAGE_NT_HEADERS>( reinterpret_cast<uintptr_t>( module_handle ) + dos->e_lfanew );
        auto sec = reinterpret_cast<PIMAGE_SECTION_HEADER>( nt + 1 );

        uintptr_t range_start = 0, range_end = 0;
        for ( auto cur = sec; cur < sec + nt->FileHeader.NumberOfSections; cur++ )
        {
            if ( strcmp( reinterpret_cast<const char*>( cur->Name ), section ) == 0 )
            {
                range_start = reinterpret_cast<uintptr_t>( module_handle ) + cur->VirtualAddress;
                range_end = range_start + cur->Misc.VirtualSize;
            }
        }

        if ( range_start == 0 )
            return 0;

        uintptr_t first_match = 0;
        auto pat = sig;

        for ( uintptr_t cur = range_start; cur < range_end; cur++ )
        {
            if ( *pat == '\0' )
                return first_match;

            if ( *reinterpret_cast<const uint8_t*>( pat ) == '\?' || *reinterpret_cast<uint8_t*>( cur ) == get_byte( pat ) )
            {
                if ( !first_match )
                    first_match = cur;

                if ( !pat[2] )
                    return first_match;

                if ( *reinterpret_cast<const uint16_t*>( pat ) == 16191 || *reinterpret_cast<const uint8_t*>( pat ) != '\?' )
                    pat += 3;
                else
                    pat += 2;
            }
            else
            {
                pat = sig;
                first_match = 0;
            }
        }
        return 0;
    }

    // ---------------------------------------------------------------
    // Find PTE base by locating the self-referencing PML4 entry
    // Must be called BEFORE virtualization (needs __readcr3 to reflect real CR3)
    // ---------------------------------------------------------------
    inline bool init_pte_base( )
    {
        uint64_t system_cr3 = __readcr3( ) & 0xFFFFFFFFFFFFF000ULL;
        uint64_t dir_base_phys = ( system_cr3 >> 12 ) << 12; // same thing, just clear low bits

        // Read the PML4 table via physical memory
        // We need to find the self-referencing entry
        for ( uint64_t idx = 0; idx < 0x200; idx++ )
        {
            uint64_t entry = 0;
            if ( !mem::read_physical( dir_base_phys + idx * 8, &entry, sizeof( entry ) ) )
                continue;

            // Check if this PML4 entry points back to itself
            uint64_t entry_pfn = ( entry >> 12 ) & 0xFFFFFFFFFULL;
            uint64_t cr3_pfn = system_cr3 >> 12;

            if ( entry_pfn == cr3_pfn )
            {
                g_self_map_idx = idx;
                g_pte_base = ( idx + 0x1FFFE00ULL ) << 39ULL;
                g_pde_base = ( idx << 30ULL ) + g_pte_base;
                g_ppe_base = ( idx << 30ULL ) + g_pte_base + ( idx << 21ULL );
                g_pxe_base = ( idx << 12ULL ) + g_ppe_base;
                return true;
            }
        }
        return false;
    }

    // ---------------------------------------------------------------
    // Find MmPfnDatabase via pattern scan in ntoskrnl .text section
    // Must be called BEFORE virtualization
    // ---------------------------------------------------------------
    extern "C" uint64_t get_nt_base( );

    inline bool init_mmpfn_database( )
    {
        uint64_t nt_base = get_nt_base( );
        if ( !nt_base )
            return false;

        auto search = search_pattern(
            reinterpret_cast<void*>( nt_base ),
            ".text",
            "B9 ? ? ? ? 48 8B 05 ? ? ? ? 48 89 43 18"
        );

        if ( !search )
            return false;

        search += 5; // skip past "B9 xx xx xx xx" to the "48 8B 05" instruction
        auto resolved_base = search + *reinterpret_cast<int32_t*>( search + 3 ) + 7;
        g_mm_pfn_database = *reinterpret_cast<uint64_t*>( resolved_base );
        return g_mm_pfn_database != 0;
    }

    // ---------------------------------------------------------------
    // Initialize all CR3 resolution globals
    // Must be called once from driver_entry BEFORE virtualize_all_cores
    // ---------------------------------------------------------------
    inline bool initialize( )
    {
        if ( !init_pte_base( ) )
            return false;

        if ( !init_mmpfn_database( ) )
            return false;

        return true;
    }

    // ---------------------------------------------------------------
    // Resolve the real CR3 for a given PID
    // Enumerates physical memory ranges and walks MMPFN entries
    // Can be called from VMX root (only uses physical memory reads)
    // ---------------------------------------------------------------
    inline uint64_t get_process_cr3( uint32_t pid )
    {
        if ( !g_pxe_base || !g_mm_pfn_database )
            return 0;

        // We need the EPROCESS for comparison
        PEPROCESS process = nullptr;
        if ( !NT_SUCCESS( resolver::g_PsLookupProcessByProcessId( reinterpret_cast<HANDLE>( static_cast<uintptr_t>( pid ) ), &process ) ) )
            return 0;

        uint64_t cr3_pte_base = g_self_map_idx * 8 + g_pxe_base;

        auto mem_range = resolver::g_MmGetPhysicalMemoryRanges( );
        if ( !mem_range )
        {
            resolver::g_ObfDereferenceObject( process );
            return 0;
        }

        uint64_t result_cr3 = 0;

        for ( int i = 0; i < 200; i++ )
        {
            if ( mem_range[i].BaseAddress.QuadPart == 0 && mem_range[i].NumberOfBytes.QuadPart == 0 )
                break;

            uint64_t start_pfn = mem_range[i].BaseAddress.QuadPart >> 12;
            uint64_t end_pfn   = start_pfn + ( mem_range[i].NumberOfBytes.QuadPart >> 12 );

            for ( uint64_t pfn = start_pfn; pfn < end_pfn; pfn++ )
            {
                // Read MMPFN entry for this PFN
                mmpfn_entry entry = {};
                uint64_t mmpfn_addr = g_mm_pfn_database + 0x30 * pfn;

                // Read flags and pte_address fields
                if ( !mem::read_physical(
                    resolver::g_MmGetPhysicalAddress( reinterpret_cast<PVOID>( mmpfn_addr ) ).QuadPart,
                    &entry, sizeof( entry ) ) )
                {
                    // MMPFN is in kernel virtual memory, read it directly
                    if ( resolver::g_MmIsAddressValid( reinterpret_cast<PVOID>( mmpfn_addr ) ) )
                        RtlCopyMemory( &entry, reinterpret_cast<PVOID>( mmpfn_addr ), sizeof( entry ) );
                    else
                        continue;
                }

                if ( !entry.flags || entry.flags == 1 )
                    continue;

                if ( entry.pte_address != cr3_pte_base )
                    continue;

                // Decode EPROCESS from MMPFN flags
                uint64_t decoded_eprocess = ( ( entry.flags | 0xF000000000000000ULL ) >> 0xD ) | 0xFFFF000000000000ULL;

                if ( resolver::g_MmIsAddressValid( reinterpret_cast<PVOID>( decoded_eprocess ) ) &&
                     reinterpret_cast<PEPROCESS>( decoded_eprocess ) == process )
                {
                    result_cr3 = pfn << 12;
                    break;
                }
            }

            if ( result_cr3 )
                break;
        }

        resolver::g_ObfDereferenceObject( process );
        return result_cr3;
    }

    // ---------------------------------------------------------------
    // Get process base address (SectionBaseAddress from EPROCESS)
    // ---------------------------------------------------------------
    inline uint64_t get_process_base( uint32_t pid )
    {
        PEPROCESS process = nullptr;
        if ( !NT_SUCCESS( resolver::g_PsLookupProcessByProcessId( reinterpret_cast<HANDLE>( static_cast<uintptr_t>( pid ) ), &process ) ) )
            return 0;

        uint64_t base = reinterpret_cast<uint64_t>( resolver::g_PsGetProcessSectionBaseAddress( process ) );

        resolver::g_ObfDereferenceObject( process );
        return base;
    }
}

#endif // CR3_H
