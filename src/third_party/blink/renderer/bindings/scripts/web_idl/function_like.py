# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from .argument import Argument
from .idl_type import IdlType


class FunctionLike(object):
    class IR(object):
        def __init__(self, arguments, return_type):
            assert isinstance(arguments, (list, tuple)) and all(
                isinstance(arg, Argument.IR) for arg in arguments)
            assert isinstance(return_type, IdlType)

            self.arguments = list(arguments)
            self.return_type = return_type

    def __init__(self, ir):
        assert isinstance(ir, FunctionLike.IR)

        self._arguments = tuple(
            [Argument(arg_ir, self) for arg_ir in ir.arguments])
        self._return_type = ir.return_type

    @property
    def arguments(self):
        """Returns a list of arguments."""
        return self._arguments

    @property
    def return_type(self):
        """Returns the return type."""
        return self._return_type
