#pragma once
#ifndef LOG_H
#define LOG_H
#include <ntifs.h>
#include <cstdint>
#include "resolver.h"


namespace log
{
    template<class... Args>
    void dbg_print( const char* format, Args... args )
    {
        if ( resolver::g_DbgPrint )
            resolver::g_DbgPrint( format, args... );
    }

}
#endif LOG_H