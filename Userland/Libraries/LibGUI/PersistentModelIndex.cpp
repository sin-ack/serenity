/*
 * Copyright (c) 2021, the SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <LibGUI/PersistentModelIndex.h>

namespace GUI {

PersistentModelIndex::PersistentModelIndex(ModelIndex const& index)
{
    dbgln("Constructing a PersistentModelIndex");
    if (!index.is_valid())
        return;

    auto* model = const_cast<Model*>(index.model());
    m_handle = model->register_persistent_index({}, index);
}

PersistentModelIndex::PersistentModelIndex(PersistentModelIndex& other)
    : m_handle(other.m_handle)
{
}

PersistentModelIndex::PersistentModelIndex(PersistentModelIndex&& other)
    : m_handle(move(other.m_handle))
{
}

PersistentModelIndex& PersistentModelIndex::operator=(PersistentModelIndex& other)
{
    m_handle = other.m_handle;
    return *this;
}

PersistentModelIndex& PersistentModelIndex::operator=(PersistentModelIndex&& other)
{
    m_handle = move(other.m_handle);
    return *this;
}

int PersistentModelIndex::row() const
{
    if (m_handle.is_null())
        return -1;
    return m_handle->m_index.row();
}

int PersistentModelIndex::column() const
{
    if (m_handle.is_null())
        return -1;
    return m_handle->m_index.column();
}

ModelIndex PersistentModelIndex::parent() const
{
    if (m_handle.is_null())
        return {};
    return m_handle->m_index.parent();
}

PersistentModelIndex::operator ModelIndex() const
{
    if (m_handle.is_null())
        return ModelIndex();
    else
        return m_handle->m_index;
}

}
