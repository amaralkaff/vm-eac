#pragma once
#ifndef RESOLVER_H
#define RESOLVER_H

#include <ntifs.h>
#include <ntimage.h>
#include <cstdint>

//
// Dynamic kernel API resolver for manual-map compatibility (KDU)
// Resolves all kernel APIs at runtime by walking ntoskrnl PE exports
// Result: zero import table entries -> invisible to static analysis
//

extern "C" uint64_t get_nt_base( );

namespace resolver
{
	// ---- Minimal string compare (no CRT dependency) ----

	inline bool str_eq( const char* a, const char* b )
	{
		while ( *a && *b && *a == *b ) { a++; b++; }
		return *a == *b;
	}

	// ---- PE export resolver ----

	inline void* find_nt_export( const char* name )
	{
		uint64_t base = get_nt_base( );
		if ( !base ) return nullptr;

		auto dos = reinterpret_cast<IMAGE_DOS_HEADER*>( base );
		if ( dos->e_magic != 0x5A4D ) return nullptr;

		auto nt = reinterpret_cast<IMAGE_NT_HEADERS64*>( base + dos->e_lfanew );
		auto& exp_dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
		if ( !exp_dir.VirtualAddress ) return nullptr;

		auto exports   = reinterpret_cast<IMAGE_EXPORT_DIRECTORY*>( base + exp_dir.VirtualAddress );
		auto names     = reinterpret_cast<uint32_t*>( base + exports->AddressOfNames );
		auto ordinals  = reinterpret_cast<uint16_t*>( base + exports->AddressOfNameOrdinals );
		auto functions = reinterpret_cast<uint32_t*>( base + exports->AddressOfFunctions );

		for ( uint32_t i = 0; i < exports->NumberOfNames; i++ )
		{
			if ( str_eq( reinterpret_cast<const char*>( base + names[i] ), name ) )
				return reinterpret_cast<void*>( base + functions[ordinals[i]] );
		}
		return nullptr;
	}

	template<typename T>
	inline T resolve( const char* name )
	{
		return reinterpret_cast<T>( find_nt_export( name ) );
	}

	// ---- Function pointer types ----

	using fn_ExAllocatePool                 = PVOID              ( __stdcall* )( POOL_TYPE, SIZE_T );
	using fn_ExFreePoolWithTag              = void               ( __stdcall* )( PVOID, ULONG );
	using fn_MmAllocateContiguousMemory     = PVOID              ( __stdcall* )( SIZE_T, PHYSICAL_ADDRESS );
	using fn_MmGetPhysicalAddress           = PHYSICAL_ADDRESS   ( __stdcall* )( PVOID );
	using fn_MmCopyMemory                   = NTSTATUS           ( __stdcall* )( PVOID, MM_COPY_ADDRESS, SIZE_T, ULONG, PSIZE_T );
	using fn_MmMapIoSpaceEx                 = PVOID              ( __stdcall* )( PHYSICAL_ADDRESS, SIZE_T, ULONG );
	using fn_MmUnmapIoSpace                 = void               ( __stdcall* )( PVOID, SIZE_T );
	using fn_MmIsAddressValid               = BOOLEAN            ( __stdcall* )( PVOID );
	using fn_MmGetPhysicalMemoryRanges      = PPHYSICAL_MEMORY_RANGE ( __stdcall* )( );
	using fn_MmGetSystemRoutineAddress       = PVOID              ( __stdcall* )( PUNICODE_STRING );
	using fn_RtlInitUnicodeString           = void               ( __stdcall* )( PUNICODE_STRING, PCWSTR );
	using fn_PsLookupProcessByProcessId      = NTSTATUS           ( __stdcall* )( HANDLE, PEPROCESS* );
	using fn_PsGetProcessSectionBaseAddress  = PVOID              ( __stdcall* )( PEPROCESS );
	using fn_KeQueryActiveProcessorCountEx   = ULONG              ( __stdcall* )( USHORT );
	using fn_KeGetCurrentProcessorIndex      = ULONG              ( __stdcall* )( );
	using fn_KeBugCheck                      = void               ( __stdcall* )( ULONG );
	using fn_ObfDereferenceObject            = LONG_PTR           ( __fastcall* )( PVOID );
	using fn_DbgPrint                        = ULONG              ( __cdecl*   )( PCSTR, ... );

	// ---- Resolved function pointers ----

	inline fn_ExAllocatePool                 g_ExAllocatePool = nullptr;
	inline fn_ExFreePoolWithTag              g_ExFreePoolWithTag = nullptr;
	inline fn_MmAllocateContiguousMemory     g_MmAllocateContiguousMemory = nullptr;
	inline fn_MmGetPhysicalAddress           g_MmGetPhysicalAddress = nullptr;
	inline fn_MmCopyMemory                   g_MmCopyMemory = nullptr;
	inline fn_MmMapIoSpaceEx                 g_MmMapIoSpaceEx = nullptr;
	inline fn_MmUnmapIoSpace                 g_MmUnmapIoSpace = nullptr;
	inline fn_MmIsAddressValid               g_MmIsAddressValid = nullptr;
	inline fn_MmGetPhysicalMemoryRanges      g_MmGetPhysicalMemoryRanges = nullptr;
	inline fn_MmGetSystemRoutineAddress       g_MmGetSystemRoutineAddress = nullptr;
	inline fn_RtlInitUnicodeString           g_RtlInitUnicodeString = nullptr;
	inline fn_PsLookupProcessByProcessId      g_PsLookupProcessByProcessId = nullptr;
	inline fn_PsGetProcessSectionBaseAddress  g_PsGetProcessSectionBaseAddress = nullptr;
	inline fn_KeQueryActiveProcessorCountEx   g_KeQueryActiveProcessorCountEx = nullptr;
	inline fn_KeGetCurrentProcessorIndex      g_KeGetCurrentProcessorIndex = nullptr;
	inline fn_KeBugCheck                      g_KeBugCheck = nullptr;
	inline fn_ObfDereferenceObject            g_ObfDereferenceObject = nullptr;
	inline fn_DbgPrint                        g_DbgPrint = nullptr;

	// Resolved kernel variable
	inline PEPROCESS g_PsInitialSystemProcess = nullptr;

	// ---- Initialize all function pointers ----

	inline bool initialize( )
	{
		g_ExAllocatePool                = resolve<fn_ExAllocatePool>( "ExAllocatePool" );
		g_ExFreePoolWithTag             = resolve<fn_ExFreePoolWithTag>( "ExFreePoolWithTag" );
		g_MmAllocateContiguousMemory    = resolve<fn_MmAllocateContiguousMemory>( "MmAllocateContiguousMemory" );
		g_MmGetPhysicalAddress          = resolve<fn_MmGetPhysicalAddress>( "MmGetPhysicalAddress" );
		g_MmCopyMemory                  = resolve<fn_MmCopyMemory>( "MmCopyMemory" );
		g_MmMapIoSpaceEx                = resolve<fn_MmMapIoSpaceEx>( "MmMapIoSpaceEx" );
		g_MmUnmapIoSpace                = resolve<fn_MmUnmapIoSpace>( "MmUnmapIoSpace" );
		g_MmIsAddressValid              = resolve<fn_MmIsAddressValid>( "MmIsAddressValid" );
		g_MmGetPhysicalMemoryRanges     = resolve<fn_MmGetPhysicalMemoryRanges>( "MmGetPhysicalMemoryRanges" );
		g_MmGetSystemRoutineAddress     = resolve<fn_MmGetSystemRoutineAddress>( "MmGetSystemRoutineAddress" );
		g_RtlInitUnicodeString          = resolve<fn_RtlInitUnicodeString>( "RtlInitUnicodeString" );
		g_PsLookupProcessByProcessId    = resolve<fn_PsLookupProcessByProcessId>( "PsLookupProcessByProcessId" );
		g_PsGetProcessSectionBaseAddress = resolve<fn_PsGetProcessSectionBaseAddress>( "PsGetProcessSectionBaseAddress" );
		g_KeQueryActiveProcessorCountEx = resolve<fn_KeQueryActiveProcessorCountEx>( "KeQueryActiveProcessorCountEx" );
		g_KeGetCurrentProcessorIndex    = resolve<fn_KeGetCurrentProcessorIndex>( "KeGetCurrentProcessorIndex" );
		g_KeBugCheck                    = resolve<fn_KeBugCheck>( "KeBugCheck" );
		g_ObfDereferenceObject          = resolve<fn_ObfDereferenceObject>( "ObfDereferenceObject" );
		g_DbgPrint                      = resolve<fn_DbgPrint>( "DbgPrint" );

		// PsInitialSystemProcess is an exported variable (pointer to EPROCESS)
		auto* p = reinterpret_cast<PEPROCESS*>( find_nt_export( "PsInitialSystemProcess" ) );
		if ( p ) g_PsInitialSystemProcess = *p;

		// Verify all critical functions resolved
		return g_ExAllocatePool && g_ExFreePoolWithTag
			&& g_MmAllocateContiguousMemory && g_MmGetPhysicalAddress
			&& g_MmCopyMemory && g_MmMapIoSpaceEx && g_MmUnmapIoSpace
			&& g_MmIsAddressValid && g_MmGetPhysicalMemoryRanges
			&& g_MmGetSystemRoutineAddress && g_RtlInitUnicodeString
			&& g_PsLookupProcessByProcessId && g_PsGetProcessSectionBaseAddress
			&& g_KeQueryActiveProcessorCountEx
			&& g_ObfDereferenceObject && g_DbgPrint
			&& g_PsInitialSystemProcess;
	}
}

#endif // RESOLVER_H
