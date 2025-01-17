# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from .composition_parts import WithIdentifier
from .composition_parts import WithOwner
from .idl_type import IdlType
from .values import DefaultValue


class Argument(WithIdentifier, WithOwner):
    class IR(WithIdentifier):
        def __init__(self, identifier, index, idl_type, default_value=None):
            assert isinstance(index, int)
            assert isinstance(idl_type, IdlType)
            assert (default_value is None
                    or isinstance(default_value, DefaultValue))

            WithIdentifier.__init__(self, identifier)

            self.index = index
            self.idl_type = idl_type
            self.default_value = default_value

        def make_copy(self):
            return Argument.IR(
                identifier=self.identifier,
                index=self.index,
                idl_type=self.idl_type,
                default_value=self.default_value)

    def __init__(self, ir, owner):
        assert isinstance(ir, Argument.IR)

        WithIdentifier.__init__(self, ir.identifier)
        WithOwner.__init__(self, owner)

        self._index = ir.index
        self._idl_type = ir.idl_type
        self._default_value = ir.default_value

    @property
    def index(self):
        """Returns the argument index."""
        return self._index

    @property
    def idl_type(self):
        """Returns the type of the argument."""
        return self._idl_type

    @property
    def is_optional(self):
        """Returns True if this is an optional argument."""
        return self.idl_type.is_optional

    @property
    def is_variadic(self):
        """Returns True if this is a variadic argument."""
        return self.idl_type.is_variadic

    @property
    def default_value(self):
        """Returns the default value or None."""
        return self._default_value
