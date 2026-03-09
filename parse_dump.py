import struct, sys, glob

BUGCHECKS = {
    0x0A: "IRQL_NOT_LESS_OR_EQUAL",
    0x19: "BAD_POOL_HEADER",
    0x1E: "KMODE_EXCEPTION_NOT_HANDLED",
    0x1A: "MEMORY_MANAGEMENT",
    0x3B: "SYSTEM_SERVICE_EXCEPTION",
    0x50: "PAGE_FAULT_IN_NONPAGED_AREA",
    0x7E: "SYSTEM_THREAD_EXCEPTION_NOT_HANDLED",
    0x7F: "UNEXPECTED_KERNEL_MODE_TRAP",
    0xBE: "ATTEMPTED_WRITE_TO_READONLY_MEMORY",
    0xC1: "SPECIAL_POOL_DETECTED_MEMORY_CORRUPTION",
    0xC4: "DRIVER_VERIFIER_DETECTED_VIOLATION",
    0xCC: "PAGE_FAULT_IN_FREED_SPECIAL_POOL",
    0xD1: "DRIVER_IRQL_NOT_LESS_OR_EQUAL",
    0xFC: "ATTEMPTED_EXECUTE_OF_NOEXECUTE_MEMORY",
    0x101: "CLOCK_WATCHDOG_TIMEOUT",
    0x109: "CRITICAL_STRUCTURE_CORRUPTION",
    0x133: "DPC_WATCHDOG_VIOLATION",
    0x139: "KERNEL_SECURITY_CHECK_FAILURE",
    0x154: "UNEXPECTED_STORE_EXCEPTION",
    0x9F: "DRIVER_POWER_STATE_FAILURE",
    0xA0: "INTERNAL_POWER_ERROR",
}

def read_kernel_dump(path):
    with open(path, 'rb') as f:
        data = f.read()

    sig = data[0:8]
    print(f'Format: {sig}')

    if sig == b'PAGEDU64':
        # DUMP_HEADER64 for Windows 10 x64:
        # +0x000 Signature      "PAGE"
        # +0x004 ValidDump      "DU64"
        # +0x008 MajorVersion   (ULONG)
        # +0x00C MinorVersion   (ULONG) = build number
        # +0x010 DirectoryTableBase (ULONG64)
        # +0x018 PfnDatabase        (ULONG64)
        # +0x020 PsLoadedModuleList (ULONG64)
        # +0x028 PsActiveProcessHead(ULONG64)
        # +0x030 MachineImageType   (ULONG)
        # +0x034 NumberProcessors   (ULONG)
        # +0x038 BugCheckCode       (ULONG)
        # +0x03C (padding)
        # +0x040 BugCheckParameter1 (ULONG64)
        # +0x048 BugCheckParameter2 (ULONG64)
        # +0x050 BugCheckParameter3 (ULONG64)
        # +0x058 BugCheckParameter4 (ULONG64)

        major = struct.unpack_from('<I', data, 0x008)[0]
        minor = struct.unpack_from('<I', data, 0x00C)[0]
        dtb   = struct.unpack_from('<Q', data, 0x010)[0]
        machine = struct.unpack_from('<I', data, 0x030)[0]
        nproc   = struct.unpack_from('<I', data, 0x034)[0]

        print(f'MajorVersion: {major}')
        print(f'MinorVersion: {minor} (build)')
        print(f'DirectoryTableBase: 0x{dtb:016X}')
        print(f'MachineType: 0x{machine:X}')
        print(f'NumProcessors: {nproc}')

        bc = struct.unpack_from('<I', data, 0x038)[0]
        p1 = struct.unpack_from('<Q', data, 0x040)[0]
        p2 = struct.unpack_from('<Q', data, 0x048)[0]
        p3 = struct.unpack_from('<Q', data, 0x050)[0]
        p4 = struct.unpack_from('<Q', data, 0x058)[0]

        name = BUGCHECKS.get(bc, f"UNKNOWN_0x{bc:X}")
        print(f'\n*** BugCheck 0x{bc:X} ({name})')
        print(f'    Param1: 0x{p1:016X}')
        print(f'    Param2: 0x{p2:016X}')
        print(f'    Param3: 0x{p3:016X}')
        print(f'    Param4: 0x{p4:016X}')

        # Interpret based on bugcheck type
        if bc == 0x1E:
            exc_code = p1 & 0xFFFFFFFF
            exc_names = {0xC0000005: "ACCESS_VIOLATION", 0xC0000096: "PRIVILEGED_INSTRUCTION",
                         0x80000003: "BREAKPOINT", 0xC000001D: "ILLEGAL_INSTRUCTION"}
            print(f'\n    KMODE_EXCEPTION_NOT_HANDLED:')
            print(f'    Exception: 0x{exc_code:X} ({exc_names.get(exc_code, "?")})')
            print(f'    Faulting IP: 0x{p2:016X}')
            print(f'    ExceptionInfo: 0x{p3:016X}')
            print(f'    BadAddress: 0x{p4:016X}')
            if exc_code == 0xC0000005:
                if p3 == 0:
                    print(f'    -> Read access violation at 0x{p4:016X}')
                elif p3 == 1:
                    print(f'    -> Write access violation at 0x{p4:016X}')
                elif p3 == 8:
                    print(f'    -> Execute access violation (DEP/NX) at 0x{p4:016X}')
        elif bc == 0x3B:
            print(f'\n    SYSTEM_SERVICE_EXCEPTION:')
            print(f'    Exception code: 0x{p1:X}')
            print(f'    Faulting address: 0x{p2:016X}')
            print(f'    Exception record: 0x{p3:016X}')
        elif bc == 0x50:
            print(f'\n    PAGE_FAULT_IN_NONPAGED_AREA:')
            print(f'    Faulting VA: 0x{p1:016X}')
            print(f'    Access type: {"WRITE" if p2 else "READ"}')
            print(f'    Faulting IP: 0x{p3:016X}')
        elif bc == 0x0A:
            print(f'\n    IRQL_NOT_LESS_OR_EQUAL:')
            print(f'    Faulting VA: 0x{p1:016X}')
            print(f'    IRQL: {p2}')
            access = []
            if p3 & 1: access.append("WRITE")
            else: access.append("READ")
            if p3 & 8: access.append("EXECUTE")
            print(f'    Access: {"+".join(access)}')
            print(f'    Faulting IP: 0x{p4:016X}')
        elif bc == 0xD1:
            print(f'\n    DRIVER_IRQL_NOT_LESS_OR_EQUAL:')
            print(f'    Faulting VA: 0x{p1:016X}')
            print(f'    IRQL: {p2}')
            print(f'    Access: {"WRITE" if p3 & 1 else "READ"}')
            print(f'    Faulting IP: 0x{p4:016X}')
        elif bc == 0x139:
            types = {0: "STACK_BUFFER_OVERRUN", 1: "VTABLE_CORRUPTION",
                     3: "GS_COOKIE_CORRUPTION", 4: "STACK_COOKIE"}
            print(f'\n    KERNEL_SECURITY_CHECK_FAILURE:')
            print(f'    Type: {types.get(p1, f"UNKNOWN({p1})")}')
        elif bc == 0x109:
            print(f'\n    CRITICAL_STRUCTURE_CORRUPTION:')
            print(f'    Corrupted region: 0x{p1:016X}')
            print(f'    Modified object: 0x{p2:016X}')
            print(f'    Corruption type: 0x{p3:X}')
        elif bc == 0xFC:
            print(f'\n    ATTEMPTED_EXECUTE_OF_NOEXECUTE_MEMORY:')
            print(f'    Address attempted to execute: 0x{p1:016X}')
        elif bc == 0x7F:
            trap_types = {0: "DIVIDE_ERROR", 4: "OVERFLOW", 5: "BOUND",
                          6: "INVALID_OPCODE", 8: "DOUBLE_FAULT",
                          0xD: "GP_FAULT", 0xE: "PAGE_FAULT"}
            print(f'\n    UNEXPECTED_KERNEL_MODE_TRAP:')
            print(f'    Trap: 0x{p1:X} ({trap_types.get(p1, "?")})')

        # Dump raw hex for context
        print(f'\n  Raw hex [0x30..0x68]:')
        for row in range(0x30, 0x68, 16):
            hexstr = ' '.join(f'{data[row+i]:02X}' for i in range(min(16, len(data)-row)))
            print(f'    {row:04X}: {hexstr}')

        return bc, p1, p2, p3, p4

    elif sig[:4] == b'MDMP':
        print("MDMP (minidump) format - use WinDbg for analysis")

    return None, 0, 0, 0, 0

if len(sys.argv) > 1:
    paths = [sys.argv[1]]
else:
    import os
    all_dumps = glob.glob(r'C:\Windows\Minidump\*.dmp')
    paths = sorted(all_dumps, key=os.path.getmtime)[-3:]

for p in paths:
    print(f'\n{"="*60}')
    print(f'FILE: {p.split(chr(92))[-1]}')
    print(f'{"="*60}')
    read_kernel_dump(p)
    print()
