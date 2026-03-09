// Minimal hypervisor ping test - standalone
// Compile: cl /O2 /EHsc test_hv.cpp test_cpuid.asm /Fe:test_hv.exe
#include <cstdio>
#include <cstdint>
#include <intrin.h>

constexpr uint32_t HV_CPUID_LEAF = 0x13371337;
constexpr uint64_t HV_MAGIC      = 0xDEAD1337C0DE5AFE;

// ASM helper (same as hv_cpuid.asm)
extern "C" void hv_cpuid_call(
    uint32_t leaf, uint32_t command,
    uint64_t param1, uint64_t param2,
    uint64_t* out );

int main()
{
    printf("=== Hypervisor Ping Test ===\n\n");

    // Test 1: CPUID leaf 0 (vendor string)
    {
        int r[4] = {};
        __cpuid(r, 0);
        printf("[1] CPUID.0: max_leaf=%d vendor=%.4s%.4s%.4s\n",
            r[0], (char*)&r[1], (char*)&r[3], (char*)&r[2]);
    }

    // Test 2: CPUID leaf 1 (hypervisor present bit)
    {
        int r[4] = {};
        __cpuid(r, 1);
        printf("[2] CPUID.1: ECX=0x%08X bit31(hv)=%d\n",
            r[2], (r[2] >> 31) & 1);
    }

    // Test 3: CPUID leaf 0x40000000 (hypervisor vendor)
    {
        int r[4] = {};
        __cpuid(r, 0x40000000);
        printf("[3] CPUID.40000000: EAX=0x%08X EBX=0x%08X ECX=0x%08X EDX=0x%08X\n",
            r[0], r[1], r[2], r[3]);
        if (r[1] || r[2] || r[3])
            printf("    vendor=%.4s%.4s%.4s\n", (char*)&r[1], (char*)&r[2], (char*)&r[3]);
    }

    // Test 4: Raw CPUID with our leaf (no magic - should return CPUID.0 if HV running)
    {
        int r[4] = {};
        __cpuidex(r, HV_CPUID_LEAF, 0);
        printf("[4] CPUID.%08X (raw, no magic): EAX=0x%08X EBX=0x%08X ECX=0x%08X EDX=0x%08X\n",
            HV_CPUID_LEAF, r[0], r[1], r[2], r[3]);
    }

    // Test 5: Our hypercall with magic (via ASM stub)
    {
        uint64_t out[4] = {};
        hv_cpuid_call( HV_CPUID_LEAF, 0x01, 0, 0, out );
        printf("[5] Hypercall PING: rax=0x%llx rbx=0x%llx rcx=0x%llx rdx=0x%llx\n",
            out[0], out[1], out[2], out[3]);
        if (out[0] == 0 && out[1] == HV_MAGIC)
            printf("    >>> HYPERVISOR ACTIVE! <<<\n");
        else
            printf("    >>> Hypervisor NOT responding\n");
    }

    // Test 6: Check if we're in VMX non-root by checking if CPUID
    // with our leaf triggers different behavior than hardware
    {
        int r1[4] = {}, r2[4] = {};
        __cpuidex(r1, 0, 0);           // Standard CPUID 0
        __cpuidex(r2, HV_CPUID_LEAF, 0); // Our leaf

        // If HV is running: r2 should be CPUID.0 result (unauthenticated path)
        // If HV is NOT running: r2 is CPU-dependent for unsupported leaf
        printf("[6] Comparison:\n");
        printf("    CPUID.0:          EAX=%08X EBX=%08X ECX=%08X EDX=%08X\n", r1[0], r1[1], r1[2], r1[3]);
        printf("    CPUID.%08X: EAX=%08X EBX=%08X ECX=%08X EDX=%08X\n", HV_CPUID_LEAF, r2[0], r2[1], r2[2], r2[3]);

        if (r2[1] == r1[1] && r2[2] == r1[2] && r2[3] == r1[3] && r1[1] != 0)
            printf("    -> Same as leaf 0 = HV intercepts but auth failed\n");
        else if (r2[0] == 0 && r2[1] == 0 && r2[2] == 0 && r2[3] == 0)
            printf("    -> All zeros = NO HV interception (bare metal)\n");
        else
            printf("    -> Different = CPU-specific fallback leaf behavior\n");
    }

    return 0;
}
