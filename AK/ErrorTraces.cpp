/*
 * Copyright (c) 2022, sin-ack <sin-ack@protonmail.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "ErrorTraces.h"
#include <AK/kmalloc.h>

namespace AK::Detail {

#ifdef ENABLE_ERROR_TRACES

ErrorTraceBuffer& get_error_trace_buffer()
{
    static thread_local ErrorTraceBuffer s_error_trace_buffer;
    return s_error_trace_buffer;
}

void record_error_trace(SourceLocation location)
{
    auto& buffer = get_error_trace_buffer();
    buffer.enqueue(location);
}

void clear_error_trace()
{
    auto& buffer = get_error_trace_buffer();
    buffer.clear();
}

#endif

}
