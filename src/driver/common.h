#pragma once

#ifdef _KERNEL_MODE
#include <ntdef.h>
#else
#include <Windows.h>
#include <winioctl.h>
#endif




#define DEVICE_NAME 
#define SYMBOLIC_LINK_NAME 


#define SHARED_SECTION_NAME L"\\BaseNamedObjects\\



#define IOCTL_** CTL_CODE(FILE_DEVICE_ACPI, 0x40, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_** CTL_CODE(FILE_DEVICE_ACPI, 0x41, METHOD_BUFFERED, FILE_ANY_ACCESS)


#define MAX_DATA_SIZE 4096


typedef enum _COMMAND_TYPE {
    CMD_NONE = 0,
    CMD_READ_MEMORY = 1,
    CMD_GET_PROCESS_BASE = 3,
    CMD_GET_DTB = 4, 
    CMD_MOUSE_CLICK = 5,
    CMD_KEYBOARD_INPUT = 6,
    CMD_MOUSE_WHEEL = 7,
    CMD_MOUSE_MOVE = 8,
    
    
    
  
    
    CMD_UNMAP = 13        
} COMMAND_TYPE;

typedef enum _COMMAND_STATUS {
    CMD_STATUS_PENDING = 0,
    CMD_STATUS_COMPLETED = 1,
    CMD_STATUS_ERROR = 2
} COMMAND_STATUS;

typedef struct _MEMORY_REQUEST {
    ULONG32 ProcessId;
    unsigned __int64 Address;
    unsigned __int64 Buffer;
    ULONG32 Size;
} MEMORY_REQUEST;

typedef struct _SHARED_MEMORY {
    volatile ULONG32 Status;      
    volatile ULONG32 Command;     
    volatile ULONG32 Magic;       
    volatile ULONG32 Ready;       
    
    MEMORY_REQUEST Request;
    
    ULONG32 ResponseSize;
    UCHAR Data[MAX_DATA_SIZE];    
} SHARED_MEMORY, *PSHARED_MEMORY;
