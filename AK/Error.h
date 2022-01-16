/*
 * Copyright (c) 2021, Andreas Kling <kling@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/ErrorTraces.h>
#include <AK/Optional.h>
#include <AK/StringView.h>
#include <AK/Try.h>
#include <AK/Variant.h>

#if defined(__serenity__) && defined(KERNEL)
#    include <LibC/errno_codes.h>
#else
#    include <errno.h>
#    include <string.h>
#endif

namespace AK {

#ifdef ENABLE_ERROR_TRACES
#    define LOCATION_PARAMETER , SourceLocation location = SourceLocation::current()
#    define LOCATION_CONSTRUCTOR_PARAMETER , SourceLocation location
#    define LOCATION_VALUE , location
#else
#    define LOCATION_PARAMETER
#    define LOCATION_CONSTRUCTOR_PARAMETER
#    define LOCATION_VALUE
#endif

class Error {
public:
    static Error from_errno(int code LOCATION_PARAMETER) { return Error(code LOCATION_VALUE); }
    static Error from_syscall(StringView syscall_name, int rc LOCATION_PARAMETER) { return Error(syscall_name, rc LOCATION_VALUE); }
    static Error from_string_literal(StringView string_literal LOCATION_PARAMETER) { return Error(string_literal LOCATION_VALUE); }

    bool is_errno() const { return m_code != 0; }
    bool is_syscall() const { return m_syscall; }

    int code() const { return m_code; }
    StringView string_literal() const { return m_string_literal; }

#ifdef ENABLE_ERROR_TRACES
    Error(Error const& error) = default;
    Error(Error&& error, SourceLocation location = SourceLocation::current())
        : m_code(error.m_code)
        , m_string_literal(error.m_string_literal)
        , m_syscall(error.m_syscall)
    {
        Detail::record_error_trace(location);
    }
#endif

protected:
    Error(int code LOCATION_PARAMETER)
        : m_code(code)
    {
#ifdef ENABLE_ERROR_TRACES
        Detail::record_error_trace(location);
#endif
    }

private:
    Error(StringView string_literal LOCATION_CONSTRUCTOR_PARAMETER)
        : m_string_literal(string_literal)
    {
#ifdef ENABLE_ERROR_TRACES
        Detail::record_error_trace(location);
#endif
    }

    Error(StringView syscall_name, int rc LOCATION_CONSTRUCTOR_PARAMETER)
        : m_code(-rc)
        , m_string_literal(syscall_name)
        , m_syscall(true)
    {
#ifdef ENABLE_ERROR_TRACES
        Detail::record_error_trace(location);
#endif
    }

    int m_code { 0 };
    StringView m_string_literal;
    bool m_syscall { false };
};

template<typename T, typename ErrorType>
class [[nodiscard]] ErrorOr final : public Variant<T, ErrorType> {
public:
    using Variant<T, ErrorType>::Variant;

    template<typename U>
    ALWAYS_INLINE ErrorOr(U&& value) requires(!IsSame<RemoveCVReference<U>, ErrorOr<T>>)
        : Variant<T, ErrorType>(forward<U>(value))
    {
    }

#ifdef __serenity__
    ErrorOr(ErrnoCode code)
        : Variant<T, ErrorType>(Error::from_errno(code))
    {
    }
#endif

    T& value()
    {
        return this->template get<T>();
    }
    T const& value() const { return this->template get<T>(); }
    ErrorType& error() { return this->template get<ErrorType>(); }
    ErrorType const& error() const { return this->template get<ErrorType>(); }

    bool is_error() const { return this->template has<ErrorType>(); }

    T release_value() { return move(value()); }
    ErrorType release_error() { return move(error()); }

    T release_value_but_fixme_should_propagate_errors() { return release_value(); }

private:
    // 'downcast' is fishy in this context. Let's hide it by making it private.
    using Variant<T, ErrorType>::downcast;
};

// Partial specialization for void value type
template<typename ErrorType>
class [[nodiscard]] ErrorOr<void, ErrorType> {
public:
    ErrorOr(ErrorType error)
        : m_error(move(error))
    {
    }

#ifdef __serenity__
    ErrorOr(ErrnoCode code)
        : m_error(Error::from_errno(code))
    {
    }
#endif

    ErrorOr() = default;
    ErrorOr(ErrorOr&& other) = default;
    ErrorOr(ErrorOr const& other) = default;
    ~ErrorOr() = default;

    ErrorOr& operator=(ErrorOr&& other) = default;
    ErrorOr& operator=(ErrorOr const& other) = default;

    ErrorType& error() { return m_error.value(); }
    bool is_error() const { return m_error.has_value(); }
    ErrorType release_error() { return m_error.release_value(); }
    void release_value() { }

private:
    Optional<ErrorType> m_error;
};

}

using AK::Error;
using AK::ErrorOr;
