/*
 * Copyright (c) 2018-2021, Andreas Kling <kling@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <LibGUI/AbstractView.h>
#include <LibGUI/Model.h>
#include <LibGUI/PersistentModelIndex.h>

namespace GUI {

Model::Model()
{
}

Model::~Model()
{
}

void Model::register_view(Badge<AbstractView>, AbstractView& view)
{
    m_views.set(&view);
    m_clients.set(&view);
}

void Model::unregister_view(Badge<AbstractView>, AbstractView& view)
{
    m_views.remove(&view);
    m_clients.remove(&view);
}

void Model::for_each_view(Function<void(AbstractView&)> callback)
{
    for (auto* view : m_views)
        callback(*view);
}

void Model::invalidate()
{
    m_persistent_handles.clear();
    did_update();
}

void Model::did_update(unsigned flags)
{
    for (auto* client : m_clients)
        client->model_did_update(flags);
}

ModelIndex Model::create_index(int row, int column, const void* data) const
{
    return ModelIndex(*this, row, column, const_cast<void*>(data));
}

ModelIndex Model::index(int row, int column, const ModelIndex&) const
{
    return create_index(row, column);
}

bool Model::accepts_drag(const ModelIndex&, const Vector<String>&) const
{
    return false;
}

void Model::register_client(ModelClient& client)
{
    m_clients.set(&client);
}

void Model::unregister_client(ModelClient& client)
{
    m_clients.remove(&client);
}

WeakPtr<PersistentHandle> Model::register_persistent_index(Badge<PersistentModelIndex>, const ModelIndex& index)
{
    if (!index.is_valid())
        return {};

    dbgln("Got here 2");
    auto it = m_persistent_handles.find(index);
    // Easy modo: we already have a handle for this model index.
    dbgln("Got here 3");
    if (it != m_persistent_handles.end()) {
        return it->value->make_weak_ptr();
    }
    dbgln("Got here 4");

    // Hard modo: create a new persistent handle.
    auto handle = adopt_own(*new PersistentHandle(index));
    auto weak_handle = handle->make_weak_ptr();
    m_persistent_handles.set(index, move(handle));

    dbgln("Got here 5");
    return weak_handle;
}

RefPtr<Core::MimeData> Model::mime_data(const ModelSelection& selection) const
{
    auto mime_data = Core::MimeData::construct();
    RefPtr<Gfx::Bitmap> bitmap;

    StringBuilder text_builder;
    StringBuilder data_builder;
    bool first = true;
    selection.for_each_index([&](auto& index) {
        auto text_data = index.data();
        if (!first)
            text_builder.append(", ");
        text_builder.append(text_data.to_string());

        if (!first)
            data_builder.append('\n');
        auto data = index.data(ModelRole::MimeData);
        data_builder.append(data.to_string());

        first = false;

        if (!bitmap) {
            Variant icon_data = index.data(ModelRole::Icon);
            if (icon_data.is_icon())
                bitmap = icon_data.as_icon().bitmap_for_size(32);
        }
    });

    mime_data->set_data(drag_data_type(), data_builder.to_byte_buffer());
    mime_data->set_text(text_builder.to_string());
    if (bitmap)
        mime_data->set_data("image/x-raw-bitmap", bitmap->serialize_to_byte_buffer());

    return mime_data;
}

void Model::begin_insert_rows(ModelIndex const& parent, int first, int last)
{
    VERIFY(first >= 0);
    VERIFY(first <= last);
    m_operation_stack.empend(OperationType::Insert, Direction::Row, parent, first, last);
}

void Model::begin_insert_columns(ModelIndex const& parent, int first, int last)
{
    VERIFY(first >= 0);
    VERIFY(first <= last);
    m_operation_stack.empend(OperationType::Insert, Direction::Column, parent, first, last);
}

void Model::begin_move_rows(ModelIndex const& source_parent, int first, int last, ModelIndex const& target_parent, int target_index)
{
    VERIFY(first >= 0);
    VERIFY(first <= last);
    VERIFY(target_index >= 0);
    m_operation_stack.empend(OperationType::Move, Direction::Row, source_parent, first, last, target_parent, target_index);
}

void Model::begin_move_columns(ModelIndex const& source_parent, int first, int last, ModelIndex const& target_parent, int target_index)
{
    VERIFY(first >= 0);
    VERIFY(first <= last);
    VERIFY(target_index >= 0);
    m_operation_stack.empend(OperationType::Move, Direction::Column, source_parent, first, last, target_parent, target_index);
}

void Model::begin_delete_rows(ModelIndex const& parent, int first, int last)
{
    VERIFY(first >= 0);
    VERIFY(first <= last);
    m_operation_stack.empend(OperationType::Delete, Direction::Row, parent, first, last);
}

void Model::begin_delete_columns(ModelIndex const& parent, int first, int last)
{
    VERIFY(first >= 0);
    VERIFY(first <= last);
    m_operation_stack.empend(OperationType::Delete, Direction::Column, parent, first, last);
}

void Model::begin_model_reset()
{
    m_operation_stack.empend(OperationType::Reset);
}

void Model::end_insert_rows()
{
    auto operation = m_operation_stack.take_last();
    VERIFY(operation.type == OperationType::Insert);
    VERIFY(operation.direction == Direction::Row);
    handle_insert(operation);
}

void Model::end_insert_columns()
{
    auto operation = m_operation_stack.take_last();
    VERIFY(operation.type == OperationType::Insert);
    VERIFY(operation.direction == Direction::Column);
    handle_insert(operation);
}

void Model::end_move_rows()
{
    auto operation = m_operation_stack.take_last();
    VERIFY(operation.type == OperationType::Move);
    VERIFY(operation.direction == Direction::Row);
    handle_move(operation);
}

void Model::end_move_columns()
{
    auto operation = m_operation_stack.take_last();
    VERIFY(operation.type == OperationType::Move);
    VERIFY(operation.direction == Direction::Column);
    handle_move(operation);
}

void Model::end_delete_rows()
{
    auto operation = m_operation_stack.take_last();
    VERIFY(operation.type == OperationType::Delete);
    VERIFY(operation.direction == Direction::Row);
    handle_delete(operation);
}

void Model::end_delete_columns()
{
    auto operation = m_operation_stack.take_last();
    VERIFY(operation.type == OperationType::Delete);
    VERIFY(operation.direction == Direction::Column);
    handle_delete(operation);
}

void Model::end_model_reset()
{
    auto operation = m_operation_stack.take_last();
    VERIFY(operation.type == OperationType::Insert);
    invalidate();
}

void Model::handle_insert(Operation const& operation)
{
    bool is_row = operation.direction == Direction::Row;
    Vector<ModelIndex*> to_shift;

    for (auto& entry : m_persistent_handles) {
        if (entry.key.parent() == operation.source_parent) {
            if (is_row && entry.key.row() >= operation.first) {
                to_shift.append(&entry.key);
            } else if (!is_row && entry.key.column() >= operation.first) {
                to_shift.append(&entry.key);
            }
        }
    }

    int offset = operation.last - operation.first + 1;

    for (auto current_index : to_shift) {
        int new_row = is_row ? current_index->row() + offset : current_index->row();
        int new_column = is_row ? current_index->column() : current_index->column() + offset;
        auto new_index = index(new_row, new_column, current_index->parent());

        auto it = m_persistent_handles.find(*current_index);
        it->value->m_index = new_index;

        m_persistent_handles.remove(it);
        m_persistent_handles.set(move(new_index), move(it->value));
    }
}

void Model::handle_delete(Operation const& operation)
{
    bool is_row = operation.direction == Direction::Row;
    Vector<ModelIndex*> to_delete;
    Vector<ModelIndex*> to_shift;

    if (is_row) {
        VERIFY(operation.last < row_count(operation.source_parent));
    } else {
        VERIFY(operation.last < column_count(operation.source_parent));
    }

    for (auto& entry : m_persistent_handles) {
        if (entry.key.parent() == operation.source_parent) {
            if (is_row) {
                if (entry.key.row() > operation.last) {
                    to_shift.append(&entry.key);
                } else if (entry.key.row() >= operation.first) {
                    to_delete.append(&entry.key);
                }
            } else {
                if (entry.key.column() > operation.last) {
                    to_shift.append(&entry.key);
                } else if (entry.key.column() >= operation.first) {
                    to_delete.append(&entry.key);
                }
            }
        }
    }

    int offset = operation.last - operation.first + 1;

    for (auto index : to_delete) {
        // The weak pointers will automatically invalidate when we drop the OwnPtrs.
        m_persistent_handles.remove(*index);
    }

    for (auto current_index : to_shift) {
        int new_row = is_row ? current_index->row() - offset : current_index->row();
        int new_column = is_row ? current_index->column() : current_index->column() - offset;
        auto new_index = index(new_row, new_column, current_index->parent());

        auto it = m_persistent_handles.find(*current_index);
        it->value->m_index = new_index;

        m_persistent_handles.remove(it);
        m_persistent_handles.set(move(new_index), move(it->value));
    }
}

void Model::handle_move(Operation const& operation)
{
    bool is_row = operation.direction == Direction::Row;
    bool move_within = operation.source_parent == operation.target_parent;
    bool moving_down = operation.target > operation.first;

    if (move_within && operation.first == operation.target)
        return;

    if (is_row) {
        VERIFY(operation.target <= row_count(operation.target_parent));
        VERIFY(operation.last < row_count(operation.source_parent));
    } else {
        VERIFY(operation.target <= column_count(operation.target_parent));
        VERIFY(operation.last < column_count(operation.source_parent));
    }

    // NOTE: to_shift_down is used as a generic "to shift" when move_within is true.
    Vector<ModelIndex*> to_move;       // Items to be moved between the source and target
    Vector<ModelIndex*> to_shift_down; // Items to be shifted down after a move-to
    Vector<ModelIndex*> to_shift_up;   // Items to be shifted up after a move-from

    int count = operation.last - operation.first + 1;
    // [start, end)
    int work_area_start = min(operation.first, operation.target);
    int work_area_end = max(operation.last + 1, operation.target + count);

    for (auto& entry : m_persistent_handles) {
        int dimension = is_row ? entry.key.row() : entry.key.column();

        if (move_within) {
            if (entry.key.parent() == operation.source_parent) {
                if (dimension >= operation.first && dimension <= operation.last) {
                    to_move.append(&entry.key);
                } else if (moving_down && dimension > operation.last && dimension < work_area_end) {
                    to_shift_down.append(&entry.key);
                } else if (!moving_down && dimension >= work_area_start && dimension < operation.first) {
                    to_shift_down.append(&entry.key);
                }
            }
        } else {
            if (entry.key.parent() == operation.source_parent) {
                if (dimension >= operation.first && dimension <= operation.last) {
                    to_move.append(&entry.key);
                } else if (dimension > operation.last) {
                    to_shift_up.append(&entry.key);
                }
            } else if (entry.key.parent() == operation.target_parent) {
                if (dimension >= operation.target) {
                    to_shift_down.append(&entry.key);
                }
            }
        }
    }

    auto replace_handle = [&](ModelIndex const& current_index, int new_dimension, bool relative) {
        int new_row = is_row
            ? (relative
                    ? current_index.row() + new_dimension
                    : new_dimension)
            : current_index.row();
        int new_column = !is_row
            ? (relative
                    ? current_index.column() + new_dimension
                    : new_dimension)
            : current_index.column();
        auto new_index = index(new_row, new_column, operation.target_parent);

        auto it = m_persistent_handles.find(current_index);
        it->value->m_index = new_index;

        m_persistent_handles.remove(it);
        m_persistent_handles.set(move(new_index), move(it->value));
    };

    for (auto current_index : to_move) {
        int dimension = is_row ? current_index->row() : current_index->column();
        int target_offset = dimension - operation.first;
        int new_dimension = operation.target + target_offset;

        replace_handle(*current_index, new_dimension, false);
    }

    if (move_within) {
        for (auto current_index : to_shift_down) {
            int dimension = is_row ? current_index->row() : current_index->column();
            int target_offset = moving_down ? dimension - (operation.last + 1) : dimension - work_area_start + count;
            int new_dimension = work_area_start + target_offset;

            replace_handle(*current_index, new_dimension, false);
        }
    } else {
        for (auto current_index : to_shift_down) {
            replace_handle(*current_index, count, true);
        }

        for (auto current_index : to_shift_up) {
            replace_handle(*current_index, count, true);
        }
    }
}

}
