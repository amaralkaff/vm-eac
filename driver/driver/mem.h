#pragma once
#ifndef MEM_H
#define MEM_H

#include "tools.h"
#include "resolver.h"
#include "structs.h"
#include <cstdint>
#include <intrin.h>

//
// Physical memory read/write from VMX root mode
// Uses MmGetVirtualForPhysical when available, falls back to MmMapIoSpaceEx
// Page table walk for VA -> PA translation
//

namespace mem
{
    // ---------------------------------------------------------------
    // Physical address mask for page table entries (bits 12-47)
    // ---------------------------------------------------------------
    constexpr uint64_t PT_PHYS_MASK = 0x000FFFFFFFFFF000ULL; // bits 12-51 of PTE

    // ---------------------------------------------------------------
    // Read from physical address using MmCopyMemory (MM_COPY_MEMORY_PHYSICAL)
    // Safe to call at DISPATCH_LEVEL (which we are at during VM-exit on DPC)
    // ---------------------------------------------------------------
    inline bool read_physical( uint64_t phys_addr, void* buffer, size_t size )
    {
        if ( !phys_addr || !buffer || !size )
            return false;

        MM_COPY_ADDRESS addr = {};
        addr.PhysicalAddress.QuadPart = static_cast<LONGLONG>( phys_addr );
        SIZE_T bytes_read = 0;

        NTSTATUS status = resolver::g_MmCopyMemory( buffer, addr, size, MM_COPY_MEMORY_PHYSICAL, &bytes_read );
        return NT_SUCCESS( status ) && bytes_read == size;
    }

    // ---------------------------------------------------------------
    // Write to physical address using MmMapIoSpaceEx
    // ---------------------------------------------------------------
    inline bool write_physical( uint64_t phys_addr, void* buffer, size_t size )
    {
        if ( !phys_addr || !buffer || !size )
            return false;

        PHYSICAL_ADDRESS addr = {};
        addr.QuadPart = static_cast<LONGLONG>( phys_addr );

        PVOID mapped = resolver::g_MmMapIoSpaceEx( addr, size, PAGE_READWRITE );
        if ( !mapped )
            return false;

        memcpy( mapped, buffer, size );
        resolver::g_MmUnmapIoSpace( mapped, size );
        return true;
    }

    // ---------------------------------------------------------------
    // 4-level page table walk: translate virtual address to physical
    // Uses the target process CR3 (directory table base)
    // Ported from km/driver.cpp translate_linear()
    // ---------------------------------------------------------------
    inline uint64_t translate_va( uint64_t dir_table_base, uint64_t virtual_address )
    {
        dir_table_base &= ~0xFULL;

        uint64_t page_offset = virtual_address & ~( ~0ULL << 12 );
        uint64_t pte_idx  = ( virtual_address >> 12 ) & 0x1FFULL;
        uint64_t pt_idx   = ( virtual_address >> 21 ) & 0x1FFULL;
        uint64_t pd_idx   = ( virtual_address >> 30 ) & 0x1FFULL;
        uint64_t pdp_idx  = ( virtual_address >> 39 ) & 0x1FFULL;

        // PML4E
        uint64_t pdpe = 0;
        if ( !read_physical( dir_table_base + 8 * pdp_idx, &pdpe, sizeof( pdpe ) ) )
            return 0;
        if ( ~pdpe & 1 )
            return 0;

        // PDPTE
        uint64_t pde = 0;
        if ( !read_physical( ( pdpe & PT_PHYS_MASK ) + 8 * pd_idx, &pde, sizeof( pde ) ) )
            return 0;
        if ( ~pde & 1 )
            return 0;

        // 1GB large page
        if ( pde & 0x80 )
            return ( pde & ( ~0ULL << 42 >> 12 ) ) + ( virtual_address & ~( ~0ULL << 30 ) );

        // PDE
        uint64_t pte_addr = 0;
        if ( !read_physical( ( pde & PT_PHYS_MASK ) + 8 * pt_idx, &pte_addr, sizeof( pte_addr ) ) )
            return 0;
        if ( ~pte_addr & 1 )
            return 0;

        // 2MB large page
        if ( pte_addr & 0x80 )
            return ( pte_addr & PT_PHYS_MASK ) + ( virtual_address & ~( ~0ULL << 21 ) );

        // PTE (4KB page)
        uint64_t phys = 0;
        if ( !read_physical( ( pte_addr & PT_PHYS_MASK ) + 8 * pte_idx, &phys, sizeof( phys ) ) )
            return 0;
        phys &= PT_PHYS_MASK;

        if ( !phys )
            return 0;

        return phys + page_offset;
    }

    // ---------------------------------------------------------------
    // Read virtual memory of a target process
    // Translates VA -> PA, then reads physical
    // Handles page boundary crossing
    // ---------------------------------------------------------------
    inline bool read_virtual( uint64_t cr3, uint64_t address, void* buffer, size_t size )
    {
        if ( !cr3 || !address || !buffer || !size )
            return false;

        size_t offset = 0;
        while ( offset < size )
        {
            uint64_t phys = translate_va( cr3, address + offset );
            if ( !phys )
                return false;

            // bytes remaining on this page
            uint64_t page_remaining = 0x1000 - ( phys & 0xFFF );
            size_t   chunk = ( size - offset );
            if ( chunk > page_remaining )
                chunk = static_cast<size_t>( page_remaining );

            if ( !read_physical( phys, reinterpret_cast<uint8_t*>( buffer ) + offset, chunk ) )
                return false;

            offset += chunk;
        }

        return true;
    }

    // ---------------------------------------------------------------
    // Write virtual memory of a target process
    // ---------------------------------------------------------------
    inline bool write_virtual( uint64_t cr3, uint64_t address, void* buffer, size_t size )
    {
        if ( !cr3 || !address || !buffer || !size )
            return false;

        size_t offset = 0;
        while ( offset < size )
        {
            uint64_t phys = translate_va( cr3, address + offset );
            if ( !phys )
                return false;

            uint64_t page_remaining = 0x1000 - ( phys & 0xFFF );
            size_t   chunk = ( size - offset );
            if ( chunk > page_remaining )
                chunk = static_cast<size_t>( page_remaining );

            if ( !write_physical( phys, reinterpret_cast<uint8_t*>( buffer ) + offset, chunk ) )
                return false;

            offset += chunk;
        }

        return true;
    }
}

#endif // MEM_H
