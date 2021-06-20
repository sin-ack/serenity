/*
 * Copyright (c) 2021, sin-ack <sin-ack@protonmail.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include "AK/StringUtils.h"
#include <AK/NonnullOwnPtr.h>
#include <LibGUI/Model.h>

namespace GUI {

class ViewModel
    : public Model
    , private ModelClient {
public:
    static NonnullRefPtr<ViewModel> create(NonnullRefPtr<Model> source) { return adopt_ref(*new ViewModel(move(source))); }
    virtual ~ViewModel() override;

    virtual int row_count(ModelIndex const& = ModelIndex()) const override;
    virtual int column_count(ModelIndex const& = ModelIndex()) const override;
    virtual String column_name(int) const override;
    virtual Variant data(ModelIndex const&, ModelRole = ModelRole::Display) const override;
    virtual TriState data_matches(ModelIndex const&, Variant const&) const override;
    virtual ModelIndex parent_index(ModelIndex const&) const override;
    virtual ModelIndex index(int row, int column = 0, ModelIndex const& parent = ModelIndex()) const override;
    virtual bool is_editable(ModelIndex const&) const override;
    virtual bool is_searchable() const override;
    virtual void set_data(ModelIndex const&, Variant const&) override;
    virtual int tree_column() const override;
    virtual bool accepts_drag(ModelIndex const&, Vector<String> const& mime_types) const override;
    virtual Vector<ModelIndex, 1> matches(StringView const&, unsigned = MatchesFlag::AllMatching, ModelIndex const& parent = ModelIndex()) override;
    virtual void invalidate() override;

    virtual bool is_column_sortable(int column_index) const override;
    virtual void sort(int column, SortOrder) override;
    void filter(String term);

    ModelRole sort_role() const { return m_sort_role; }
    void set_sort_role(ModelRole);

    CaseSensitivity case_sensitivity() const { return m_case_sensitivity; }
    void set_case_sensitivity(CaseSensitivity);

private:
    explicit ViewModel(NonnullRefPtr<Model> source);

    // NOTE: The internal_data() of indices points to the corresponding
    // Mapping object for that index.
    struct Mapping {
        Vector<PersistentModelIndex> proxy_rows;
        ModelIndex source_parent;
    };

    Mapping* mapping_for_index(ModelIndex const&) const;
    Optional<ModelIndex> source_index_from_proxy(ModelIndex const&) const;
    Optional<ModelIndex> proxy_index_from_source(ModelIndex const&) const;
    ModelIndex index_from_source_parent(int row, int column, ModelIndex const& source_parent) const;
    Mapping* get_or_create_mapping(const ModelIndex& source_parent, bool first_call = true);

    bool less_than(ModelIndex const&, ModelIndex const&);

    void sort_mapping(Mapping&);
    void filter_mapping(Mapping&);

    void sort_impl();
    void filter_impl();

    // ^ModelClient
    virtual void model_did_update(unsigned) override;

    // NOTE: This maps from the source's parent indices to our mappings.
    HashMap<ModelIndex, NonnullOwnPtr<Mapping>> m_mappings;
    NonnullRefPtr<Model> m_source;

    // Filtering
    String m_filter_term;
    // Sorting
    int m_sort_column { -1 };
    ModelRole m_sort_role { ModelRole::Sort };
    SortOrder m_sort_order { SortOrder::Ascending };
    CaseSensitivity m_case_sensitivity { CaseSensitivity::CaseSensitive };
};

}
