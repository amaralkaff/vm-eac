#pragma once
#include <Windows.h>
#include <cstdint>
#include <cstdio>
#include <TlHelp32.h>
#include <atomic>
#include <intrin.h>

//
// Usermode hypervisor communication interface
// Uses CPUID instruction to send commands to VMX root
// No kernel handle, no device object, no IOCTL
//

// Must match driver/driver/comms.h exactly
constexpr uint32_t HV_CPUID_LEAF    = 0x13371337;
constexpr uint64_t HV_MAGIC         = 0xDEAD1337C0DE5AFE;

enum hv_command : uint32_t
{
    HV_CMD_PING             = 0x01,
    HV_CMD_READ_PHYS        = 0x02,
    HV_CMD_READ_VIRT        = 0x03,
    HV_CMD_GET_CR3          = 0x04,
    HV_CMD_GET_BASE         = 0x05,
    HV_CMD_SET_TARGET       = 0x06,
    HV_CMD_WRITE_VIRT       = 0x07,
    HV_CMD_REGISTER_PAGE    = 0x10,
    HV_CMD_BULK_READ        = 0x11,
};

// Shared page layout - must match driver/driver/comms.h
constexpr uint32_t SHARED_PAGE_HEADER_SIZE = 0x100;
constexpr uint32_t SHARED_PAGE_MAX_DATA    = 0x1000 - SHARED_PAGE_HEADER_SIZE;

struct bulk_read_request
{
    uint64_t target_va;
    uint32_t size;
    uint32_t status;
};

enum hv_status : uint64_t
{
    HV_STATUS_SUCCESS       = 0,
    HV_STATUS_INVALID_MAGIC = 1,
    HV_STATUS_INVALID_CMD   = 2,
    HV_STATUS_FAILED        = 3,
    HV_STATUS_NOT_INIT      = 4,
};

// ASM helpers: sets full 64-bit registers before CPUID, reads 64-bit results
// Defined in hv_cpuid.asm
extern "C" void hv_cpuid_call(
    uint32_t leaf,       // -> EAX
    uint32_t command,    // -> ECX (sub-leaf)
    uint64_t param1,     // -> RDX
    uint64_t param2,     // -> RBX
    uint64_t* out        // <- 4x uint64_t array [rax, rbx, rcx, rdx]
);

// Extended version: also sets R8 (for write size, etc.)
extern "C" void hv_cpuid_call_ex(
    uint32_t leaf,       // -> EAX
    uint32_t command,    // -> ECX
    uint64_t param1,     // -> RDX
    uint64_t param2,     // -> RBX
    uint64_t param3,     // -> R8
    uint64_t* out        // <- 4x uint64_t array
);

struct hv_result
{
    uint64_t rax;   // status
    uint64_t rbx;   // result1
    uint64_t rcx;   // result2
    uint64_t rdx;   // result3
};

class HvInterface
{
public:
    HvInterface( ) = default;

    ~HvInterface( )
    {
        if ( m_shared_page )
        {
            VirtualFree( m_shared_page, 0, MEM_RELEASE );
            m_shared_page = nullptr;
        }
    }

    bool Initialize( )
    {
        hv_result r = hypercall( HV_CMD_PING, 0, 0 );
        printf( "  [dbg] PING result: rax=0x%llx rbx=0x%llx rcx=0x%llx rdx=0x%llx\n",
            r.rax, r.rbx, r.rcx, r.rdx );
        printf( "  [dbg] Expected: rax=0x0 rbx=0x%llx\n", HV_MAGIC );

        // Also check raw CPUID leaf 1 bit 31 (hypervisor present)
        int cpuinfo[4] = {};
        __cpuidex( cpuinfo, 1, 0 );
        printf( "  [dbg] CPUID.1.ECX bit31 (hv present): %d\n", ( cpuinfo[2] >> 31 ) & 1 );

        if ( r.rax == HV_STATUS_SUCCESS && r.rbx == HV_MAGIC )
        {
            m_active = true;
            InitSharedPage( );
            return true;
        }
        return false;
    }

    bool SetTarget( uint32_t pid )
    {
        hv_result r = hypercall( HV_CMD_SET_TARGET, pid, 0 );
        if ( r.rax == HV_STATUS_SUCCESS )
        {
            m_pid = pid;
            m_cr3 = r.rbx;
            m_base = r.rcx;
            return true;
        }
        return false;
    }

    bool ReadMemory( uint32_t pid, uint64_t address, void* buffer, size_t size )
    {
        if ( !m_active || !buffer || !size )
            return false;

        if ( pid != m_pid )
        {
            if ( !SetTarget( pid ) )
                return false;
        }

        uint8_t* dst = reinterpret_cast<uint8_t*>( buffer );
        size_t offset = 0;

        while ( offset < size )
        {
            size_t chunk = size - offset;
            if ( chunk > 8 ) chunk = 8;

            hv_result r = hypercall( HV_CMD_READ_VIRT, address + offset, chunk );
            if ( r.rax != HV_STATUS_SUCCESS )
                return false;

            memcpy( dst + offset, &r.rbx, chunk );
            offset += chunk;
        }

        return true;
    }

    // Bulk read via shared memory page - single VM-exit for up to ~3840 bytes
    bool ReadMemoryBulk( uint32_t pid, uint64_t address, void* buffer, size_t size )
    {
        if ( !m_active || !buffer || !size || !m_shared_page )
            return ReadMemory( pid, address, buffer, size ); // fallback

        if ( pid != m_pid )
        {
            if ( !SetTarget( pid ) )
                return false;
        }

        uint8_t* dst = reinterpret_cast<uint8_t*>( buffer );
        size_t offset = 0;

        while ( offset < size )
        {
            size_t chunk = size - offset;
            if ( chunk > SHARED_PAGE_MAX_DATA )
                chunk = SHARED_PAGE_MAX_DATA;

            // Write request header to shared page
            auto* req = reinterpret_cast<bulk_read_request*>( m_shared_page );
            req->target_va = address + offset;
            req->size = static_cast<uint32_t>( chunk );
            req->status = 0xFFFFFFFF; // sentinel

            // Issue bulk read hypercall
            hv_result r = hypercall( HV_CMD_BULK_READ, 0, 0 );
            if ( r.rax != HV_STATUS_SUCCESS || req->status != 0 )
                return false;

            // Copy data from shared page payload area
            memcpy( dst + offset, m_shared_page + SHARED_PAGE_HEADER_SIZE, chunk );
            offset += chunk;
        }

        return true;
    }

    bool WriteMemory( uint32_t pid, uint64_t address, void* buffer, size_t size )
    {
        if ( !m_active || !buffer || !size )
            return false;

        if ( pid != m_pid )
        {
            if ( !SetTarget( pid ) )
                return false;
        }

        uint8_t* src = reinterpret_cast<uint8_t*>( buffer );
        size_t offset = 0;

        while ( offset < size )
        {
            size_t chunk = size - offset;
            if ( chunk > 8 ) chunk = 8;

            uint64_t value = 0;
            memcpy( &value, src + offset, chunk );

            // Use extended hypercall to pass size in R8
            hv_result r = hypercall_ex( HV_CMD_WRITE_VIRT, address + offset, value, chunk );
            if ( r.rax != HV_STATUS_SUCCESS )
                return false;

            offset += chunk;
        }

        return true;
    }

    uint64_t GetProcessCr3( uint32_t pid )
    {
        hv_result r = hypercall( HV_CMD_GET_CR3, pid, 0 );
        return ( r.rax == HV_STATUS_SUCCESS ) ? r.rbx : 0;
    }

    uint64_t GetModuleBase( uint32_t pid, const wchar_t* moduleName = nullptr )
    {
        (void)moduleName;
        hv_result r = hypercall( HV_CMD_GET_BASE, pid, 0 );
        return ( r.rax == HV_STATUS_SUCCESS ) ? r.rbx : 0;
    }

    uint32_t GetProcessId( const wchar_t* processName )
    {
        HANDLE snap = CreateToolhelp32Snapshot( TH32CS_SNAPPROCESS, 0 );
        if ( snap == INVALID_HANDLE_VALUE )
            return 0;

        PROCESSENTRY32W pe = {};
        pe.dwSize = sizeof( pe );
        uint32_t pid = 0;

        if ( Process32FirstW( snap, &pe ) )
        {
            do
            {
                if ( _wcsicmp( pe.szExeFile, processName ) == 0 )
                {
                    pid = pe.th32ProcessID;
                    break;
                }
            } while ( Process32NextW( snap, &pe ) );
        }

        CloseHandle( snap );
        return pid;
    }

    void SetCurrentPid( uint32_t pid ) { m_pid = pid; }

    // Input via SendInput (usermode, no driver needed)
    void SetMouseSignature( uint32_t, uint32_t ) { }

    void TestRandomMouseMoveLoop( std::atomic_bool& running )
    {
        srand( static_cast<unsigned>( GetTickCount64( ) ) );
        while ( running.load( std::memory_order_relaxed ) )
        {
            int dx = ( rand( ) % 11 ) - 5;
            int dy = ( rand( ) % 11 ) - 5;
            MoveMouse( dx, dy );
            Sleep( 30 + ( rand( ) % 40 ) );
        }
    }

    void MoveMouse( int dx, int dy )
    {
        if ( dx == 0 && dy == 0 ) return;
        INPUT input = {};
        input.type = INPUT_MOUSE;
        input.mi.dx = dx;
        input.mi.dy = dy;
        input.mi.dwFlags = MOUSEEVENTF_MOVE;
        SendInput( 1, &input, sizeof( INPUT ) );
    }

    void InjectMouseMove( int dx, int dy )
    {
        MoveMouse( dx, dy );
    }

    bool InjectMouseClick( int button = 0 )
    {
        MouseClick( button );
        return true;
    }

    void MouseClick( int button )
    {
        INPUT inputs[2] = {};
        inputs[0].type = INPUT_MOUSE;
        inputs[1].type = INPUT_MOUSE;

        if ( button == 0 )
        {
            inputs[0].mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
            inputs[1].mi.dwFlags = MOUSEEVENTF_LEFTUP;
        }
        else
        {
            inputs[0].mi.dwFlags = MOUSEEVENTF_RIGHTDOWN;
            inputs[1].mi.dwFlags = MOUSEEVENTF_RIGHTUP;
        }

        SendInput( 1, &inputs[0], sizeof( INPUT ) );
        Sleep( 8 + ( rand( ) % 18 ) );
        SendInput( 1, &inputs[1], sizeof( INPUT ) );
    }
    void Cleanup( )
    {
        m_active = false;
        if ( m_shared_page )
        {
            VirtualFree( m_shared_page, 0, MEM_RELEASE );
            m_shared_page = nullptr;
        }
    }

    bool IsActive( ) const { return m_active; }
    uint64_t GetCachedCr3( ) const { return m_cr3; }
    uint64_t GetCachedBase( ) const { return m_base; }

private:
    bool     m_active      = false;
    uint32_t m_pid         = 0;
    uint64_t m_cr3         = 0;
    uint64_t m_base        = 0;
    uint8_t* m_shared_page = nullptr;

    bool InitSharedPage( )
    {
        // Allocate a page-aligned 4KB page, lock it in physical memory
        m_shared_page = reinterpret_cast<uint8_t*>(
            VirtualAlloc( nullptr, 0x1000, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE )
        );
        if ( !m_shared_page )
            return false;

        // Lock page so it stays in physical memory (hypervisor accesses PA directly)
        VirtualLock( m_shared_page, 0x1000 );
        memset( m_shared_page, 0, 0x1000 );

        // Register the shared page VA with the hypervisor
        // It will translate to PA using our CR3 and cache it
        hv_result r = hypercall( HV_CMD_REGISTER_PAGE, reinterpret_cast<uint64_t>( m_shared_page ), 0 );
        if ( r.rax != HV_STATUS_SUCCESS )
        {
            VirtualFree( m_shared_page, 0, MEM_RELEASE );
            m_shared_page = nullptr;
            return false;
        }

        return true;
    }

    hv_result hypercall( uint32_t cmd, uint64_t param1, uint64_t param2 )
    {
        uint64_t out[4] = {};
        hv_cpuid_call( HV_CPUID_LEAF, cmd, param1, param2, out );

        hv_result res;
        res.rax = out[0];
        res.rbx = out[1];
        res.rcx = out[2];
        res.rdx = out[3];
        return res;
    }

    hv_result hypercall_ex( uint32_t cmd, uint64_t param1, uint64_t param2, uint64_t param3 )
    {
        uint64_t out[4] = {};
        hv_cpuid_call_ex( HV_CPUID_LEAF, cmd, param1, param2, param3, out );

        hv_result res;
        res.rax = out[0];
        res.rbx = out[1];
        res.rcx = out[2];
        res.rdx = out[3];
        return res;
    }
};

// Alias so existing code compiles without changes to main.cpp type declarations
using DriverInterfaceV3 = HvInterface;
