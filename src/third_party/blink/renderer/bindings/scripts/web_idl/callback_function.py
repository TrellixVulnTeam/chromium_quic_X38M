# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import exceptions

from .function_like import FunctionLike
from .composition_parts import WithCodeGeneratorInfo
from .composition_parts import WithComponent
from .composition_parts import WithDebugInfo
from .composition_parts import WithExtendedAttributes
from .identifier_ir_map import IdentifierIRMap
from .user_defined_type import UserDefinedType


class CallbackFunction(UserDefinedType, FunctionLike, WithExtendedAttributes,
                       WithCodeGeneratorInfo, WithComponent, WithDebugInfo):
    """https://heycam.github.io/webidl/#idl-callback-functions"""

    class IR(IdentifierIRMap.IR, FunctionLike.IR, WithExtendedAttributes,
             WithCodeGeneratorInfo, WithComponent, WithDebugInfo):
        def __init__(self,
                     identifier,
                     arguments,
                     return_type,
                     extended_attributes=None,
                     code_generator_info=None,
                     component=None,
                     debug_info=None):
            IdentifierIRMap.IR.__init__(
                self,
                identifier=identifier,
                kind=IdentifierIRMap.IR.Kind.CALLBACK_FUNCTION)
            FunctionLike.IR.__init__(
                self, arguments=arguments, return_type=return_type)
            WithExtendedAttributes.__init__(self, extended_attributes)
            WithCodeGeneratorInfo.__init__(self, code_generator_info)
            WithComponent.__init__(self, component)
            WithDebugInfo.__init__(self, debug_info)

    def __init__(self, ir):
        assert isinstance(ir, CallbackFunction.IR)

        UserDefinedType.__init__(self, ir.identifier)
        FunctionLike.__init__(self, ir)
        WithExtendedAttributes.__init__(self,
                                        ir.extended_attributes.make_copy())
        WithCodeGeneratorInfo.__init__(self,
                                       ir.code_generator_info.make_copy())
        WithComponent.__init__(self, components=ir.components)
        WithDebugInfo.__init__(self, ir.debug_info.make_copy())

    # UserDefinedType overrides
    @property
    def is_callback_function(self):
        return True
