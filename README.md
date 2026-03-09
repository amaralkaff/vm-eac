# vm-eac

bare-metal vt-x hypervisor for apex legends on windows 10 (19045). cpuid-based hypercall for kernel-usermode comms — no device object, no ioctl, no handle.

## status

hypervisor loads via kdu, virtualizes all 12 cores. cpuid interception + tsc/aperf/mperf compensation working. usermode esp/aimbot overlay via xbox game bar hijack. ept disabled (causes gpu tdr 0x116).

## known issues

- bugcheck 0x139 (stack cookie) — temp[0x1000] on 24kb vmm stack overflows /gs cookie. fixed: map shared page directly, vmm stack increased to 64kb.
- bugcheck 0xdead0010 (rdmsr loop) — rdmsr returned 0 for unknown msrs, kernel retried forever. fixed: seh passthrough, inject #gp for invalid msrs.
- bugcheck 0x1e (illegal instruction) — xsaves/xrstors intercepted but skipped, corrupting fpu/sse/avx state. fixed: disabled xsaves interception.
- gpu tdr (0x116) — ept identity map doesn't exclude gpu mmio. needs per-device mmio exclusion before ept can be re-enabled.

## structure

driver/driver/ — kernel-mode vt-x hypervisor (vmcs, vmexit, ept, handlers, cr3 resolver)
src/ — usermode esp/aimbot overlay (hv_interface, memory reader, entity scanner, imgui)
parse_dump.py — crash dump parser

## build

requires vs 2025, wdk 10.0.26100.0.

driver: msbuild driver.vcxproj -p:Configuration=Release -p:Platform=x64 -p:WindowsTargetPlatformVersion=10.0.26100.0 -p:SignMode=Off
usermode: msbuild apex_esp.vcxproj -p:Configuration=Release -p:Platform=x64 -p:PlatformToolset=v145

## load

kdu.exe -map driver\driver\x64\Release\driver.sys

## comms

cpuid leaf 0x13371337 with magic 0xDEAD1337C0DE5AFE in r8. ping (0x01), read_virt (0x03), set_target (0x06), register_page (0x10), bulk_read (0x11).

## help wanted

ept mmio exclusion for gpu, proper ept identity map that handles pci bar regions. prs welcome.
