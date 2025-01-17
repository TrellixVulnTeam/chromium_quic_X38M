# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import itertools


class ExtendedAttribute(object):
    """
    Represents a single extended attribute.
    https://heycam.github.io/webidl/#dfn-extended-attribute
    """

    # [Key]
    _FORM_NO_ARGS = 'NoArgs'
    # [Key=Value]
    _FORM_IDENT = 'Ident'
    # [Key=(Value1, Value2, ...)]
    _FORM_IDENT_LIST = 'IdentList'
    # [Key(Value1L Value1R, Value2L Value2R, ...)]
    _FORM_ARG_LIST = 'ArgList'
    # [Key=Name(Value1L Value1R, Value2L Value2R, ...)]
    _FORM_NAMED_ARG_LIST = 'NamedArgList'

    def __init__(self, key, values=None, arguments=None, name=None):
        assert isinstance(key, str)
        assert values is None or isinstance(values, str) or (isinstance(
            values,
            (list, tuple)) and all(isinstance(value, str) for value in values))
        assert arguments is None or (isinstance(
            arguments, (list, tuple)) and all(
                isinstance(left, str) and isinstance(right, str)
                for left, right in arguments))
        assert name is None or isinstance(name, str)

        self._format = None
        self._key = key
        self._values = None
        self._arguments = None
        self._name = name

        if name is not None:
            self._format = self._FORM_NAMED_ARG_LIST
            if values is not None or arguments is None:
                raise ValueError('Unknown format for ExtendedAttribute')
            self._arguments = tuple(arguments)
        elif arguments is not None:
            self._format = self._FORM_ARG_LIST
            if values is not None:
                raise ValueError('Unknown format for ExtendedAttribute')
            self._arguments = tuple(arguments)
        elif values is None:
            self._format = self._FORM_NO_ARGS
        elif isinstance(values, str):
            self._format = self._FORM_IDENT
            self._values = values
        else:
            self._format = self._FORM_IDENT_LIST
            self._values = tuple(values)

    def __eq__(self, other):
        if not isinstance(other, self.__class__):
            return NotImplemented
        return (self.key == other.key
                and self.syntactic_form == other.syntactic_form)

    def __ne__(self, other):
        return not self == other

    def __hash__(self):
        return hash((self._key, self._values, self._arguments))

    def make_copy(self):
        return ExtendedAttribute(
            key=self._key,
            values=self._values,
            arguments=self._arguments,
            name=self._name)

    @property
    def syntactic_form(self):
        if self._format == self._FORM_NO_ARGS:
            return self._key
        if self._format == self._FORM_IDENT:
            return '{}={}'.format(self._key, self._values)
        if self._format == self._FORM_IDENT_LIST:
            return '{}=({})'.format(self._key, ', '.join(self._values))
        args_str = '({})'.format(', '.join(
            ['{} {}'.format(left, right) for left, right in self._arguments]))
        if self._format == self._FORM_ARG_LIST:
            return '{}{}'.format(self._key, args_str)
        if self._format == self._FORM_NAMED_ARG_LIST:
            return '{}={}{}'.format(self._key, self._name, args_str)
        assert False, 'Unknown format: {}'.format(self._format)

    @property
    def key(self):
        return self._key

    @property
    def value(self):
        """
        Returns the value for format Ident.  Returns None for format NoArgs.
        Otherwise, raises a ValueError.
        """
        if self._format in (self._FORM_NO_ARGS, self._FORM_IDENT):
            return self._values
        raise ValueError('[{}] does not have a single value.'.format(
            self.syntactic_form))

    @property
    def values(self):
        """
        Returns a list of values for format Ident and IdentList.  Returns an
        empty list for format NorArgs.  Otherwise, raises a ValueError.
        """
        if self._format == self._FORM_NO_ARGS:
            return ()
        if self._format == self._FORM_IDENT:
            return (self._values, )
        if self._format == self._FORM_IDENT_LIST:
            return self._values
        raise ValueError('[{}] does not have a value.'.format(
            self.syntactic_form))

    @property
    def arguments(self):
        """
        Returns a list of value pairs for format ArgList and NamedArgList.
        Otherwise, raises a ValueError.
        """
        if self._format in (self._FORM_ARG_LIST, self._FORM_NAMED_ARG_LIST):
            return self._arguments
        raise ValueError('[{}] does not have an argument.'.format(
            self.syntactic_form))

    @property
    def name(self):
        """
        Returns |Name| for format NamedArgList.  Otherwise, raises a ValueError.
        """
        if self._format == self._FORM_NAMED_ARG_LIST:
            return self._name
        raise ValueError('[{}] does not have a name.'.format(
            self.syntactic_form))


class ExtendedAttributes(object):
    """
    ExtendedAttributes is a dict-like container for ExtendedAttribute instances.
    With a key string, you can get an ExtendedAttribute or a list of them.

    For an IDL fragment
      [A, A=(foo, bar), B=baz]
    an ExtendedAttributes instance will be like
      {
        'A': (ExtendedAttribute('A'),
              ExtendedAttribute('A', values=('foo', 'bar'))),
        'B': (ExtendedAttribute('B', value='baz')),
      }

    https://heycam.github.io/webidl/#idl-extended-attributes
    """

    def __init__(self, extended_attributes=None):
        assert extended_attributes is None or (isinstance(
            extended_attributes, (list, tuple)) and all(
                isinstance(attr, ExtendedAttribute)
                for attr in extended_attributes))
        sorted_ext_attrs = sorted(
            extended_attributes or [], key=lambda x: x.key)

        self._ext_attrs = {
            key: tuple(sorted(ext_attrs, key=lambda x: x.syntactic_form))
            for key, ext_attrs in itertools.groupby(
                sorted_ext_attrs, key=lambda x: x.key)
        }
        self._keys = tuple(sorted(self._ext_attrs.keys()))
        self._length = len(sorted_ext_attrs)

    def __eq__(self, other):
        if not isinstance(other, self.__class__):
            return NotImplemented
        if self.keys() != other.keys():
            return False
        if len(self) != len(other):
            return False
        for lhs, rhs in zip(self, other):
            if lhs != rhs:
                return False
        return True

    def __ne__(self, other):
        return not self == other

    __hash__ = None

    def __contains__(self, key):
        """Returns True if this has an extended attribute with the |key|."""
        return key in self._ext_attrs

    def __iter__(self):
        """Yields all ExtendedAttribute instances in a certain sorted order."""
        for key in self._keys:
            for ext_attr in self._ext_attrs[key]:
                yield ext_attr

    def __len__(self):
        return self._length

    def make_copy(self):
        return ExtendedAttributes(map(ExtendedAttribute.make_copy, self))

    @property
    def syntactic_form(self):
        return '[{}]'.format(', '.join(
            [ext_attr.syntactic_form for ext_attr in self]))

    def keys(self):
        return self._keys

    def get(self, key):
        """
        Returns an exnteded attribute whose key is |key|, or None if not found.
        If there are multiple extended attributes with |key|, raises an error.
        """
        values = self.get_list_of(key)
        if len(values) == 0:
            return None
        if len(values) == 1:
            return values[0]
        raise ValueError(
            'There are multiple extended attributes for the key "{}"'.format(
                key))

    def get_list_of(self, key):
        """
        Returns a list of extended attributes whose keys are |key|.
        """
        return self._ext_attrs.get(key, ())
