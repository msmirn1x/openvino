// Copyright (C) 2018-2022 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#include "openvino/op/unique.hpp"

#include "itt.hpp"
#include "ngraph/validation_util.hpp"
#include "openvino/op/util/op_types.hpp"

namespace ov {
namespace {
int64_t extract_axis(const std::shared_ptr<op::v0::Constant>& axis_constant) {
    const auto axis_vec = axis_constant->cast_vector<int64_t>();
    return axis_vec.at(0);
}
}  // namespace

op::v10::Unique::Unique(const Output<Node>& data, const bool sorted, const element::Type& index_element_type)
    : op::Op{{data}},
      m_sorted{sorted},
      m_index_element_type{index_element_type} {
    constructor_validate_and_infer_types();
}

op::v10::Unique::Unique(const Output<Node>& data,
                        const Output<Node>& axis,
                        const bool sorted,
                        const element::Type& index_element_type)
    : op::Op{{data, axis}},
      m_sorted{sorted},
      m_index_element_type{index_element_type} {
    constructor_validate_and_infer_types();
}

bool op::v10::Unique::visit_attributes(AttributeVisitor& visitor) {
    OV_OP_SCOPE(v10_Unique_visit_attributes);
    visitor.on_attribute("sorted", m_sorted);
    visitor.on_attribute("index_element_type", m_index_element_type);
    return true;
}

void op::v10::Unique::validate_and_infer_types() {
    OV_OP_SCOPE(v10_Unique_validate_and_infer_types);
    NODE_VALIDATION_CHECK(this,
                          m_index_element_type == element::i32 || m_index_element_type == element::i64,
                          "The element type of the outputs containing indices can only be set to i32 or i64");

    const auto& input_shape = get_input_partial_shape(0);
    std::vector<PartialShape> output_shapes(4);

    int64_t input_tensor_capacity = -1;
    if (input_shape.is_static()) {
        input_tensor_capacity = static_cast<int64_t>(shape_size(input_shape.to_shape()));
    }

    output_shapes[0] = PartialShape::dynamic();
    output_shapes[1] =
        input_tensor_capacity > 0 ? PartialShape{{1, input_tensor_capacity}} : PartialShape{{Dimension::dynamic()}};
    output_shapes[2] = output_shapes[1];
    output_shapes[3] = output_shapes[1];

    if (get_input_size() == 2) {
        NODE_VALIDATION_CHECK(
            this,
            get_input_element_type(1) == element::i32 || get_input_element_type(1) == element::i64,
            "The allowed element types of the 'axis' input tensor of the Unique operator are i32 and i64.");

        NODE_VALIDATION_CHECK(
            this,
            get_input_partial_shape(1) == Shape{} || get_input_partial_shape(1) == Shape{1},
            "The 'axis' input tensor of the Unique operator must be a scalar or 1D tensor with 1 element.");

        NODE_VALIDATION_CHECK(this,
                              ov::op::util::is_constant(input_value(1).get_node()),
                              "The 'axis' input of the Unique operator must be connected to a Constant.");
        const int64_t axis =
            extract_axis(std::dynamic_pointer_cast<op::v0::Constant>(input_value(1).get_node_shared_ptr()));

        if (input_shape.rank().is_static()) {
            const auto normalized_axis = ngraph::normalize_axis(this, axis, input_shape.rank());
            const auto dim_at_axis = input_shape[normalized_axis];
            std::cout << dim_at_axis.get_min_length() << " " << dim_at_axis.get_max_length() << std::endl;

            Dimension output_dim_at_axis;
            if (dim_at_axis.is_dynamic()) {
                if (dim_at_axis == Dimension::dynamic()) {
                    output_dim_at_axis = dim_at_axis;
                } else {
                    output_dim_at_axis = Dimension{1, dim_at_axis.get_max_length()};
                }
            } else if (dim_at_axis.get_length() == 0) {
                output_dim_at_axis = Dimension{0};
                output_shapes[1] = PartialShape{{0}};
                output_shapes[2] = PartialShape{{0}};
                output_shapes[3] = PartialShape{{0}};
            } else {
                output_dim_at_axis = Dimension{1, dim_at_axis.get_max_length()};
            }

            auto output_shape = input_shape;
            output_shape[normalized_axis] = output_dim_at_axis;
            output_shapes[0] = output_shape;
        }
    } else {
        // no axis => flattened input tensor
        if (input_shape.is_static()) {
            // between 1 and the total number of input tensor's unique elements
            output_shapes[0] = PartialShape{{Dimension{1, input_tensor_capacity}}};
        } else {
            output_shapes[0] = PartialShape{{Dimension::dynamic()}};
        }
    }

    set_input_is_relevant_to_shape(0);

    set_output_type(0, get_input_element_type(0), output_shapes[0]);
    set_output_type(1, m_index_element_type, output_shapes[1]);
    set_output_type(2, m_index_element_type, output_shapes[2]);
    set_output_type(3, element::i64, output_shapes[3]);
}

std::shared_ptr<Node> op::v10::Unique::clone_with_new_inputs(const OutputVector& new_args) const {
    OV_OP_SCOPE(v10_Unique_clone_with_new_inputs);
    check_new_args_count(this, new_args);
    if (new_args.size() == 1) {
        return std::make_shared<op::v10::Unique>(new_args.at(0), this->get_sorted(), this->get_index_element_type());
    } else {
        return std::make_shared<op::v10::Unique>(new_args.at(0),
                                                 new_args.at(1),
                                                 this->get_sorted(),
                                                 this->get_index_element_type());
    }
}
}  // namespace ov
