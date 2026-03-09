#pragma once
#ifndef EPT_H
#define EPT_H

#include "resolver.h"
#include "dbg.h"
#include <ntifs.h>

namespace ept
{
	// EPT entry permission bits
	constexpr uint64_t EPT_READ    = ( 1ULL << 0 );
	constexpr uint64_t EPT_WRITE   = ( 1ULL << 1 );
	constexpr uint64_t EPT_EXECUTE = ( 1ULL << 2 );
	constexpr uint64_t EPT_RWX     = EPT_READ | EPT_WRITE | EPT_EXECUTE;
	constexpr uint64_t EPT_IGNORE_PAT = ( 1ULL << 6 );
	constexpr uint64_t EPT_LARGE   = ( 1ULL << 7 );

	// Memory types for EPT leaf entries (bits 5:3)
	constexpr uint64_t EPT_MT_UC = 0ULL;
	constexpr uint64_t EPT_MT_WB = 6ULL;

	constexpr uint32_t MAX_SPLITS = 64;

	struct split_page
	{
		uint64_t entries[512];
	};

	struct ept_state
	{
		uint64_t* pml4;
		uint64_t  pml4_phys;

		uint64_t* pdpt;
		uint64_t  pdpt_phys;

		// PD tables: one per 1GB region (512 x 2MB entries each)
		uint64_t* pd[512];
		uint64_t  pd_phys[512];
		uint32_t  pd_count;

		// Split PT tables for 4KB granularity hiding
		split_page* splits[MAX_SPLITS];
		uint64_t    splits_phys[MAX_SPLITS];
		uint32_t    split_pdpt_idx[MAX_SPLITS];
		uint32_t    split_pd_idx[MAX_SPLITS];
		uint32_t    split_count;

		// Zero page: reads of hidden pages return zeros
		uint8_t*  zero_page;
		uint64_t  zero_page_phys;

		uint64_t  eptp;
	};

	inline ept_state* g_ept = nullptr;

	// ---- EPT entry builders ----

	inline uint64_t make_2mb_entry( uint64_t phys_2mb, uint64_t mem_type )
	{
		// EPT WB + ignore-PAT=0: effective type = MTRR combined with guest PAT
		// This makes EPT completely transparent for memory typing (same as bare metal)
		return ( phys_2mb & 0xFFFFFFFE00000ULL ) | EPT_LARGE | ( mem_type << 3 ) | EPT_RWX;
	}

	inline uint64_t make_4kb_entry( uint64_t phys_4kb, uint64_t mem_type )
	{
		return ( phys_4kb & 0xFFFFFFFFF000ULL ) | ( mem_type << 3 ) | EPT_RWX;
	}

	inline uint64_t make_table_entry( uint64_t table_phys )
	{
		return ( table_phys & 0xFFFFFFFFF000ULL ) | EPT_RWX;
	}

	inline uint64_t make_eptp( uint64_t pml4_phys )
	{
		// bits 2:0 = memory type for EPT structures (6 = WB)
		// bits 5:3 = page walk length minus 1 (3 = 4-level)
		return ( pml4_phys & 0xFFFFFFFFF000ULL ) | ( 3ULL << 3 ) | EPT_MT_WB;
	}

	// ---- Helpers ----

	inline void* alloc_page( )
	{
		void* p = resolver::g_ExAllocatePool( NonPagedPool, 0x1000 );
		if ( p ) memset( p, 0, 0x1000 );
		return p;
	}

	inline uint64_t virt_to_phys( void* va )
	{
		return resolver::g_MmGetPhysicalAddress( va ).QuadPart;
	}

	// ---- Check if a 2MB page is FULLY contained within RAM ----
	// Conservative: only WB if the entire 2MB is RAM. Partial overlap = UC (safe for MMIO)

	inline bool is_fully_ram( PPHYSICAL_MEMORY_RANGE ranges, uint64_t phys_2mb )
	{
		uint64_t page_start = phys_2mb;
		uint64_t page_end   = phys_2mb + ( 1ULL << 21 );

		for ( int r = 0; ranges[r].BaseAddress.QuadPart || ranges[r].NumberOfBytes.QuadPart; r++ )
		{
			uint64_t rng_start = ranges[r].BaseAddress.QuadPart;
			uint64_t rng_end   = rng_start + ranges[r].NumberOfBytes.QuadPart;
			// 2MB page must be entirely within this RAM range
			if ( page_start >= rng_start && page_end <= rng_end )
				return true;
		}
		return false;
	}

	// ---- Initialize EPT with identity-mapped 2MB pages ----

	inline bool initialize( )
	{
		g_ept = ( ept_state* ) resolver::g_ExAllocatePool( NonPagedPool, sizeof( ept_state ) );
		if ( !g_ept ) return false;
		memset( g_ept, 0, sizeof( ept_state ) );

		// Zero page for hiding
		g_ept->zero_page = ( uint8_t* ) alloc_page( );
		if ( !g_ept->zero_page ) return false;
		g_ept->zero_page_phys = virt_to_phys( g_ept->zero_page );

		// PML4 table
		g_ept->pml4 = ( uint64_t* ) alloc_page( );
		if ( !g_ept->pml4 ) return false;
		g_ept->pml4_phys = virt_to_phys( g_ept->pml4 );

		// PDPT table
		g_ept->pdpt = ( uint64_t* ) alloc_page( );
		if ( !g_ept->pdpt ) return false;
		g_ept->pdpt_phys = virt_to_phys( g_ept->pdpt );

		// Link PML4[0] -> PDPT
		g_ept->pml4[0] = make_table_entry( g_ept->pdpt_phys );

		// Use CPUID.80000008 to get max physical address bits
		int cpu_info[4];
		__cpuid( cpu_info, 0x80000008 );
		uint32_t phys_bits = cpu_info[0] & 0xFF;
		uint64_t max_phys_addr = 1ULL << phys_bits;

		// Cap at 512GB (one PML4 entry), but also cap at 128GB practical limit
		// 128GB = 128 PD tables = 512KB pool, covers all consumer systems
		if ( max_phys_addr > ( 128ULL << 30 ) )
			max_phys_addr = ( 128ULL << 30 );

		uint32_t gb_count = ( uint32_t ) ( ( max_phys_addr + ( 1ULL << 30 ) - 1 ) >> 30 );
		if ( gb_count > 512 ) gb_count = 512;
		g_ept->pd_count = gb_count;

		log::dbg_print( "[ept] MAXPHYADDR=%u bits, mapping %u GB (all WB, MTRR+PAT determines actual type)\n", phys_bits, gb_count );

		// Allocate PD tables and fill with 2MB identity-mapped WB entries
		// All entries are WB with ignore-PAT=0: effective type = MTRR combined with guest PAT
		// This is transparent - same memory typing as bare metal
		for ( uint32_t i = 0; i < gb_count; i++ )
		{
			g_ept->pd[i] = ( uint64_t* ) alloc_page( );
			if ( !g_ept->pd[i] ) return false;
			g_ept->pd_phys[i] = virt_to_phys( g_ept->pd[i] );

			for ( uint32_t j = 0; j < 512; j++ )
			{
				uint64_t phys_2mb = ( ( uint64_t ) i << 30 ) | ( ( uint64_t ) j << 21 );
				g_ept->pd[i][j] = make_2mb_entry( phys_2mb, EPT_MT_WB );
			}

			g_ept->pdpt[i] = make_table_entry( g_ept->pd_phys[i] );
		}

		// Build EPTP
		g_ept->eptp = make_eptp( g_ept->pml4_phys );

		log::dbg_print( "[ept] Initialized - EPTP = 0x%llx\n", g_ept->eptp );
		return true;
	}

	// ---- Split a 2MB page into 512 x 4KB pages ----

	inline bool split_2mb( uint64_t phys_addr )
	{
		if ( !g_ept ) return false;

		uint32_t pdpt_idx = ( uint32_t ) ( phys_addr >> 30 ) & 0x1FF;
		uint32_t pd_idx   = ( uint32_t ) ( phys_addr >> 21 ) & 0x1FF;

		if ( pdpt_idx >= g_ept->pd_count ) return false;

		// Check if already split
		for ( uint32_t i = 0; i < g_ept->split_count; i++ )
		{
			if ( g_ept->split_pdpt_idx[i] == pdpt_idx && g_ept->split_pd_idx[i] == pd_idx )
				return true;
		}

		if ( g_ept->split_count >= MAX_SPLITS ) return false;

		// Allocate a 4KB-aligned page for the PT table
		auto* pt = ( split_page* ) alloc_page( );
		if ( !pt ) return false;

		// Fill with identity-mapped 4KB entries
		uint64_t base_2mb = ( ( uint64_t ) pdpt_idx << 30 ) | ( ( uint64_t ) pd_idx << 21 );
		for ( uint32_t i = 0; i < 512; i++ )
		{
			pt->entries[i] = make_4kb_entry( base_2mb + ( ( uint64_t ) i << 12 ), EPT_MT_WB );
		}

		uint64_t pt_phys = virt_to_phys( pt );
		uint32_t idx = g_ept->split_count++;

		g_ept->splits[idx] = pt;
		g_ept->splits_phys[idx] = pt_phys;
		g_ept->split_pdpt_idx[idx] = pdpt_idx;
		g_ept->split_pd_idx[idx] = pd_idx;

		// Replace 2MB PDE with table entry -> PT
		g_ept->pd[pdpt_idx][pd_idx] = make_table_entry( pt_phys );

		return true;
	}

	// ---- Hide a single 4KB physical page (redirect reads to zero page) ----

	inline bool hide_page( uint64_t phys_addr )
	{
		if ( !g_ept ) return false;
		if ( !split_2mb( phys_addr ) ) return false;

		uint32_t pdpt_idx = ( uint32_t ) ( phys_addr >> 30 ) & 0x1FF;
		uint32_t pd_idx   = ( uint32_t ) ( phys_addr >> 21 ) & 0x1FF;
		uint32_t pt_idx   = ( uint32_t ) ( phys_addr >> 12 ) & 0x1FF;

		for ( uint32_t i = 0; i < g_ept->split_count; i++ )
		{
			if ( g_ept->split_pdpt_idx[i] == pdpt_idx && g_ept->split_pd_idx[i] == pd_idx )
			{
				// Redirect to zero page: guest reads see zeros, writes go to shared zero page
				g_ept->splits[i]->entries[pt_idx] = make_4kb_entry( g_ept->zero_page_phys, EPT_MT_WB );
				return true;
			}
		}
		return false;
	}

	// ---- Hide a virtual kernel allocation (multi-page) ----

	inline void hide_allocation( void* va, size_t size )
	{
		uint8_t* ptr = ( uint8_t* ) va;
		for ( size_t offset = 0; offset < size; offset += 0x1000 )
		{
			uint64_t pa = virt_to_phys( ptr + offset );
			if ( pa )
			{
				if ( hide_page( pa ) )
					log::dbg_print( "[ept] Hidden page PA 0x%llx (VA 0x%p)\n", pa, ptr + offset );
			}
		}
	}

	inline uint64_t get_eptp( )
	{
		return g_ept ? g_ept->eptp : 0;
	}
}

#endif // EPT_H
