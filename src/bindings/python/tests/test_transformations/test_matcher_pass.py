# -*- coding: utf-8 -*-
# Copyright (C) 2022 Intel Corporation
# SPDX-License-Identifier: Apache-2.0
from openvino.runtime import opset8
from openvino.runtime.passes import Manager, Matcher, MatcherPass, WrapType
from openvino.runtime.utils import replace_node

from utils.utils import count_ops, get_test_function, PatternReplacement


def test_simple_pattern_replacement():
    # Simple: for Extensions. Without any classes and inheritance.
    def pattern_replacement():
        param = WrapType("opset8.Parameter")
        relu = WrapType("opset8.Relu", param.output(0))

        def callback(matcher: Matcher) -> bool:
            root = matcher.get_match_root()

            # Just to check that capturing works and we can
            # link pattern nodes with matched graph nodes.
            assert relu in matcher.get_pattern_value_map()

            new_relu = opset8.exp(root.input_value(0))  # ot root.input(0).get_source_output()
            replace_node(root, new_relu)
            return True

        return Matcher(relu, "SimpleReplacement"), callback

    model = get_test_function()

    manager = Manager()
    manager.register_pass(MatcherPass(*pattern_replacement()))
    manager.run_passes(model)

    assert count_ops(model, ("Relu", "Exp")) == [0, 1]


def test_matcher_pass():
    model = get_test_function()

    manager = Manager()
    # check that register pass returns pass instance
    pattern_replacement_pass = manager.register_pass(PatternReplacement())
    manager.run_passes(model)

    assert pattern_replacement_pass.model_changed
    assert count_ops(model, "Relu") == [2]


def test_matcher_pass_apply():
    model = get_test_function()

    pattern_replacement = PatternReplacement()
    pattern_replacement.apply(model.get_result().input_value(0).get_node())

    assert count_ops(model, "Relu") == [2]