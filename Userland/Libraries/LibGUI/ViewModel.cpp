/*
 * Copyright (c) 2018-2020, Andreas Kling <kling@serenityos.org>
 * Copyright (c) 2021, sin-ack <sin-ack@protonmail.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "ViewModel.h"

namespace GUI {

ViewModel::Mapping* ViewModel::mapping_for_index(ModelIndex const& proxy_index) const
{
    if (!proxy_index.is_valid())
        return nullptr;

    VERIFY(proxy_index.model() == this);
    VERIFY(proxy_index.internal_data());

    auto mapping = static_cast<Mapping*>(proxy_index.internal_data());
    // FIXME: This is an extremely naive method of checking if the pointer we're
    // holding actually points to an existing mapping before dereferencing it.
    // A more performant but memory-heavy way would be to keep a HashMap of
    // active mapping pointers which we clean up after. I couldn't find a good
    // solution to this, so feel free to clean it up if you did. :^)
    for (auto& entry : m_mappings) {
        if (entry.key.ptr() == mapping) {
            return mapping;
        }
    }

    return nullptr;
}

Optional<ModelIndex> ViewModel::source_index_from_proxy(ModelIndex const& proxy_index) const
{
    auto mapping = mapping_for_index(proxy_index);
    if (mapping == nullptr)
        return {};

    auto target_persistent_index = mapping->proxy_rows.at(proxy_index.row());
    return target_persistent_index;
}

Optional<ModelIndex> ViewModel::proxy_index_from_source(ModelIndex const& source_index) const
{
    if (!source_index.is_valid())
        return {};

    auto mapping = const_cast<ViewModel*>(this)->get_or_create_mapping(source_index.parent());

    for (size_t i = 0; i < mapping->proxy_rows.size(); i++) {
        auto& proxy_index = mapping->proxy_rows[i];
        if (proxy_index.row() == source_index.row()) {
            VERIFY(proxy_index.is_valid());
            return index_from_source_parent(i, source_index.column(), mapping->source_parent);
        }
    }
}

ModelIndex ViewModel::index_from_source_parent(int row, int column, ModelIndex const& source_parent) const
{
    auto it = m_mappings.find(source_parent);
    if (it == m_mappings.end()) {
        return {};
    }

    auto& mapping = *it->value;
    return create_index(row, column, &mapping);
}

ViewModel::Mapping* ViewModel::get_or_create_mapping(const ModelIndex& source_parent, bool first_call)
{
    // Do we already have a mapping for this parent?
    auto it = m_mappings.find(source_parent);
    if (it != m_mappings.end()) {
        return it->value.ptr();
    }

    auto mapping = make<Mapping>();
    mapping->source_parent = source_parent;

    int row_count = m_source->row_count(source_parent);
    VERIFY(row_count >= 0);
    mapping->proxy_rows.ensure_capacity(row_count);

    // Column 0 is arbitrary, and we end up selecting the correct column in
    // ViewModel::index and friends.
    for (int i = 0; i < row_count; i++) {
        mapping->proxy_rows.empend(m_source->index(i, 0, source_parent));
    }

    filter_mapping(*mapping);
    sort_mapping(*mapping);

    // Do we have a mapping for this parent's parent? If not, we're most likely
    // going to end up needing it anyway, so build it as well.
    if (source_parent.is_valid()) {
        auto source_grandparent = source_parent.parent();
        get_or_create_mapping(source_grandparent, false);
    }

    // We only want to notify the view that we've updated at the top level of
    // the recursive call, because otherwise we waste time repeatedly notifying
    // clients about things they're most likely not interested in (you can't
    // hold indices to newly-created mappings :^).
    if (first_call)
        did_update(UpdateFlag::DontInvalidateIndices);

    auto ptr = mapping.ptr();
    m_mappings.set(source_parent, move(mapping));
    return ptr;
}

ViewModel::ViewModel(NonnullRefPtr<Model> source)
    : m_source(move(source))
{
    m_source->register_client(*this);
}

ViewModel::~ViewModel()
{
    m_source->unregister_client(*this);
}

int ViewModel::row_count(ModelIndex const& proxy_index) const
{
    auto mapping = mapping_for_index(proxy_index);
    if (mapping == nullptr)
        return 0;

    return mapping->proxy_rows.size();
}

int ViewModel::column_count(ModelIndex const& proxy_index) const
{
    auto mapping = mapping_for_index(proxy_index);
    if (mapping == nullptr)
        return 0;

    // Sorry, no filtering/sorting of columns. :^(
    return m_source->column_count(mapping->source_parent);
}

String ViewModel::column_name(int index) const
{
    return m_source->column_name(index);
}

Variant ViewModel::data(ModelIndex const& proxy_index, ModelRole role) const
{
    auto maybe_source_index = source_index_from_proxy(proxy_index);
    VERIFY(maybe_source_index.has_value());

    auto source_index = maybe_source_index.value();
    VERIFY(source_index.is_valid());

    return m_source->data(source_index, role);
}

TriState ViewModel::data_matches(ModelIndex const& proxy_index, Variant const& data) const
{
    auto maybe_source_index = source_index_from_proxy(proxy_index);
    VERIFY(maybe_source_index.has_value());

    auto source_index = maybe_source_index.value();
    VERIFY(source_index.is_valid());

    return m_source->data_matches(source_index, data);
}

ModelIndex ViewModel::parent_index(ModelIndex const& proxy_index) const
{
    auto mapping = mapping_for_index(proxy_index);
    if (mapping == nullptr)
        return {};

    auto maybe_parent_index = proxy_index_from_source(mapping->source_parent);
    if (!maybe_parent_index.has_value())
        return {};

    return maybe_parent_index.value();
}

ModelIndex ViewModel::index(int row, int column, ModelIndex const& proxy_parent) const
{
    if ((row < 0 || row >= row_count(proxy_parent))
        || (column < 0 || column >= column_count(proxy_parent))) {
        return {};
    }

    auto maybe_source_parent = source_index_from_proxy(proxy_parent);
    if (!maybe_source_parent.has_value())
        return {};

    return index_from_source_parent(row, column, maybe_source_parent.value());
}

bool ViewModel::is_editable(ModelIndex const& proxy_index) const
{
    auto maybe_source_index = source_index_from_proxy(proxy_index);
    if (!maybe_source_index.has_value())
        return false;

    return m_source->is_editable(maybe_source_index.value());
}

bool ViewModel::is_searchable() const
{
    return m_source->is_searchable();
}

void ViewModel::set_data(ModelIndex const& proxy_index, Variant const& data)
{
    auto maybe_source_index = source_index_from_proxy(proxy_index);
    VERIFY(maybe_source_index.has_value());

    m_source->set_data(maybe_source_index.value(), data);
}

int ViewModel::tree_column() const
{
    return m_source->tree_column();
}

bool ViewModel::accepts_drag(ModelIndex const& proxy_index, Vector<String> const& mime_types) const
{
    auto maybe_source_index = source_index_from_proxy(proxy_index);
    if (!maybe_source_index.has_value())
        return false;

    return m_source->accepts_drag(maybe_source_index.value(), mime_types);
}

Vector<ModelIndex, 1> ViewModel::matches(StringView const& term, unsigned flags, ModelIndex const& proxy_parent)
{
    auto maybe_source_parent = source_index_from_proxy(proxy_parent);
    if (!maybe_source_parent.has_value())
        return {};

    auto source_result = m_source->matches(term, flags, maybe_source_parent.value());
    Vector<ModelIndex, 1> proxy_result;

    for (auto& source_index : source_result) {
        // FIXME: This feels kind of slow... A way to improve it could be to
        // create a HashTable of rows that are available.
        auto maybe_proxy_index = proxy_index_from_source(source_index);
        if (maybe_proxy_index.has_value()) {
            proxy_result.append(maybe_proxy_index.value());
        }
    }

    return proxy_result;
}

void ViewModel::invalidate()
{
    Model::invalidate();
    m_source->invalidate();

    // FIXME: Any non-persistent model indices out there will point to a
    // non-existent Mapping after this!
    m_mappings.clear();
}

bool ViewModel::is_column_sortable(int column_index) const
{
    return m_source->is_column_sortable(column_index);
}

bool ViewModel::less_than(ModelIndex const& a, ModelIndex const& b)
{
    auto a_data = a.data(m_sort_role);
    auto b_data = b.data(m_sort_role);
    if (a_data.is_string() && b_data.is_string())
        return a_data.as_string().to_lowercase() < b_data.as_string().to_lowercase();
    return a_data < a_data;
}

void ViewModel::sort_mapping(Mapping& mapping)
{
    if (m_sort_column == -1) {
        // -1 means default order (no sorting).
        quick_sort(mapping.proxy_rows, [](auto a, auto b) -> bool {
            return a.row() < b.row();
        });
        return;
    }

    // TODO: granular updates!
    quick_sort(mapping.proxy_rows, [&](auto a, auto b) {
        bool is_less_than = less_than(m_source->index(a.row(), m_sort_column, mapping.source_parent), m_source->index(b.row(), m_sort_column, mapping.source_parent));
        return m_sort_order == SortOrder::Ascending ? is_less_than : !is_less_than;
    });
}

// NOTE: sort_impl/filter_impl functions do the relevant heavy lifting, while
// sort/filter provide an API to ViewModel users and automatically handle
// book-keeping.

void ViewModel::sort_impl()
{
    for (auto& mapping : m_mappings) {
        sort_mapping(mapping);
    }
}

void ViewModel::filter_impl()
{
    for (auto& mapping : m_mappings) {
        filter_mapping(mapping);
        // NOTE: After filtering, the list will be unsorted (because we do
        // filter->sort). Therefore we need to also sort.
        sort_mapping(mapping);
    }
}

void ViewModel::sort(int column, SortOrder sort_order)
{
    if (m_sort_column == column && m_sort_order == sort_order)
        return;
    m_sort_column = column;
    m_sort_order = sort_order;

    sort_impl();
    did_update(UpdateFlag::DontInvalidateIndices);
}

void ViewModel::filter(String term)
{
    if (m_filter_term == term)
        return;
    m_filter_term = term;

    filter_impl();
    did_update(UpdateFlag::DontInvalidateIndices);
}

}
