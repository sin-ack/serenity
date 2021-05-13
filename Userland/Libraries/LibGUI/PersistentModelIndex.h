/*
 * Copyright (c) 2021, the SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/RefCounted.h>
#include <AK/RefPtr.h>
#include <LibGUI/Model.h>
#include <LibGUI/ModelIndex.h>

namespace GUI {

/// A PersistentHandle is an internal data structure used to keep track of the
/// target of multiple PersistentModelIndex instances.
class PersistentHandle : public RefCounted<PersistentHandle> {
    friend Model;
    friend class PersistentModelIndex;

    PersistentHandle(ModelIndex const& index)
        : m_index(index)
    {
    }

    ModelIndex m_index;
};

class PersistentModelIndex {
public:
    PersistentModelIndex() { }
    PersistentModelIndex(ModelIndex const&);
    PersistentModelIndex(PersistentModelIndex&);
    PersistentModelIndex(PersistentModelIndex&&);

    PersistentModelIndex& operator=(PersistentModelIndex&);
    PersistentModelIndex& operator=(PersistentModelIndex&&);

    bool is_valid() const { return !m_handle.is_null() && m_handle->m_index.is_valid(); }

    int row() const;
    int column() const;
    ModelIndex parent() const;

    operator ModelIndex() const;

private:
    RefPtr<PersistentHandle> m_handle;
};

}
