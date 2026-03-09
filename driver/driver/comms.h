#pragma once
#ifndef COMMS_H
#define COMMS_H

#include <cstdint>

//
// CPUID hypercall communication protocol
// Usermode calls CPUID with leaf = HV_CPUID_LEAF
// VM-exit handler intercepts and dispatches command
//

// Secret CPUID leaf - must not collide with real Intel/AMD leaves
// Avoid 0x40000000-0x4FFFFFFF (hypervisor reserved range)
constexpr uint32_t HV_CPUID_LEAF    = 0x13371337;

// Authentication magic - must match between driver and usermode
constexpr uint64_t HV_MAGIC         = 0xDEAD1337C0DE5AFE;

// Command IDs passed via RCX (sub-leaf)
enum hv_command : uint32_t
{
    HV_CMD_PING             = 0x01,   // verify hypervisor is active
    HV_CMD_READ_PHYS        = 0x02,   // read physical memory
    HV_CMD_READ_VIRT        = 0x03,   // read virtual memory (translates VA -> PA)
    HV_CMD_GET_CR3          = 0x04,   // resolve real CR3 for a process
    HV_CMD_GET_BASE         = 0x05,   // get process base address
    HV_CMD_SET_TARGET       = 0x06,   // set target PID + cache its CR3
    HV_CMD_WRITE_VIRT       = 0x07,   // write virtual memory
    HV_CMD_REGISTER_PAGE    = 0x10,   // register shared memory page for bulk ops
    HV_CMD_BULK_READ        = 0x11,   // read up to ~3800 bytes via shared page
};

// Status codes returned in RAX
enum hv_status : uint64_t
{
    HV_STATUS_SUCCESS       = 0,
    HV_STATUS_INVALID_MAGIC = 1,
    HV_STATUS_INVALID_CMD   = 2,
    HV_STATUS_FAILED        = 3,
    HV_STATUS_NOT_INIT      = 4,
};

//
// Register usage for CPUID hypercalls:
//
// INPUT  (usermode sets before CPUID):
//   EAX = HV_CPUID_LEAF
//   RCX = command (hv_command) | (HV_MAGIC in upper bits for auth)
//   RDX = param1 (varies by command)
//   RBX = param2 (varies by command)
//
// OUTPUT (hypervisor writes to guest regs):
//   RAX = status (hv_status)
//   RBX = result1
//   RCX = result2
//   RDX = result3
//
// Per-command register layout:
//
// HV_CMD_PING:
//   IN:  nothing
//   OUT: rax = HV_STATUS_SUCCESS, rbx = HV_MAGIC (confirmation)
//
// HV_CMD_SET_TARGET:
//   IN:  rdx = pid
//   OUT: rax = status, rbx = resolved CR3, rcx = base address
//
// HV_CMD_READ_VIRT:
//   IN:  rdx = virtual address to read, rbx = size (max 8 bytes for register return)
//   OUT: rax = status, rbx = data (for reads <= 8 bytes)
//
// HV_CMD_READ_PHYS:
//   IN:  rdx = physical address, rbx = size (max 8 bytes)
//   OUT: rax = status, rbx = data
//
// HV_CMD_GET_CR3:
//   IN:  rdx = pid
//   OUT: rax = status, rbx = CR3
//
// HV_CMD_GET_BASE:
//   IN:  rdx = pid
//   OUT: rax = status, rbx = base address
//
// HV_CMD_WRITE_VIRT:
//   IN:  rdx = virtual address, rbx = value to write (up to 8 bytes), r8 = size
//   OUT: rax = status
//

// Shared memory page layout for bulk reads
// Usermode writes the request header, hypervisor writes data payload
constexpr uint32_t SHARED_PAGE_HEADER_SIZE = 0x100;
constexpr uint32_t SHARED_PAGE_MAX_DATA    = 0x1000 - SHARED_PAGE_HEADER_SIZE; // ~3840 bytes

struct bulk_read_request
{
    uint64_t target_va;    // virtual address to read in target process
    uint32_t size;         // bytes to read (max SHARED_PAGE_MAX_DATA)
    uint32_t status;       // hypervisor writes: 0=success, 1=failed
};
// Data payload starts at offset SHARED_PAGE_HEADER_SIZE in the shared page

// Shared state cached per hypervisor instance (not per-vcpu since all cores share target)
struct hv_target_state
{
    volatile uint32_t pid;
    volatile uint64_t cr3;
    volatile uint64_t base_address;
    volatile bool     initialized;
};

#endif // COMMS_H
