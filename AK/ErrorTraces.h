/*
 * Copyright (c) 2022, sin-ack <sin-ack@protonmail.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/CircularQueue.h>
#include <AK/SourceLocation.h>

#if !defined(NO_TLS) && defined(__serenity__) && !defined(KERNEL)
#    define ENABLE_ERROR_TRACES
#endif

namespace AK::Detail {

#ifdef ENABLE_ERROR_TRACES

using ErrorTraceBuffer = CircularQueue<SourceLocation, 32>;

ErrorTraceBuffer& get_error_trace_buffer();

void record_error_trace(SourceLocation location = SourceLocation::current());
void clear_error_trace();

#endif

}
