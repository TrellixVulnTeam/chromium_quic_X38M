// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/indexeddb/indexeddb_key.h"

#include <string>
#include <utility>

namespace blink {

namespace {

// Very rough estimate of minimum key size overhead.
const size_t kOverheadSize = 16;

size_t CalculateArraySize(const IndexedDBKey::KeyArray& keys) {
  size_t size(0);
  for (const auto& key : keys)
    size += key.size_estimate();
  return size;
}

template <typename T>
int Compare(const T& a, const T& b) {
  // Using '<' for both comparisons here is as generic as possible (for e.g.
  // objects which only define operator<() and not operator>() or operator==())
  // and also allows e.g. floating point NaNs to compare equal.
  if (a < b)
    return -1;
  return (b < a) ? 1 : 0;
}

}  // namespace

IndexedDBKey::IndexedDBKey()
    : type_(mojom::IDBKeyType::None), size_estimate_(kOverheadSize) {}

IndexedDBKey::IndexedDBKey(mojom::IDBKeyType type)
    : type_(type), size_estimate_(kOverheadSize) {
  DCHECK(type == mojom::IDBKeyType::None || type == mojom::IDBKeyType::Invalid);
}

IndexedDBKey::IndexedDBKey(double number, mojom::IDBKeyType type)
    : type_(type),
      number_(number),
      size_estimate_(kOverheadSize + sizeof(number)) {
  DCHECK(type == blink::mojom::IDBKeyType::Number ||
         type == blink::mojom::IDBKeyType::Date);
}

IndexedDBKey::IndexedDBKey(KeyArray array)
    : type_(blink::mojom::IDBKeyType::Array),
      array_(std::move(array)),
      size_estimate_(kOverheadSize + CalculateArraySize(array_)) {}

IndexedDBKey::IndexedDBKey(std::string binary)
    : type_(blink::mojom::IDBKeyType::Binary),
      binary_(std::move(binary)),
      size_estimate_(kOverheadSize +
                     (binary_.length() * sizeof(std::string::value_type))) {}

IndexedDBKey::IndexedDBKey(base::string16 string)
    : type_(mojom::IDBKeyType::String),
      string_(std::move(string)),
      size_estimate_(kOverheadSize +
                     (string_.length() * sizeof(base::string16::value_type))) {}

IndexedDBKey::IndexedDBKey(const IndexedDBKey& other) = default;
IndexedDBKey::~IndexedDBKey() = default;
IndexedDBKey& IndexedDBKey::operator=(const IndexedDBKey& other) = default;

bool IndexedDBKey::IsValid() const {
  if (type_ == mojom::IDBKeyType::Invalid || type_ == mojom::IDBKeyType::None)
    return false;

  if (type_ == blink::mojom::IDBKeyType::Array) {
    for (size_t i = 0; i < array_.size(); i++) {
      if (!array_[i].IsValid())
        return false;
    }
  }

  return true;
}

bool IndexedDBKey::IsLessThan(const IndexedDBKey& other) const {
  return CompareTo(other) < 0;
}

bool IndexedDBKey::Equals(const IndexedDBKey& other) const {
  return !CompareTo(other);
}

int IndexedDBKey::CompareTo(const IndexedDBKey& other) const {
  DCHECK(IsValid());
  DCHECK(other.IsValid());
  if (type_ != other.type_)
    return type_ > other.type_ ? -1 : 1;

  switch (type_) {
    case mojom::IDBKeyType::Array:
      for (size_t i = 0; i < array_.size() && i < other.array_.size(); ++i) {
        int result = array_[i].CompareTo(other.array_[i]);
        if (result != 0)
          return result;
      }
      return Compare(array_.size(), other.array_.size());
    case mojom::IDBKeyType::Binary:
      return binary_.compare(other.binary_);
    case mojom::IDBKeyType::String:
      return string_.compare(other.string_);
    case mojom::IDBKeyType::Date:
    case mojom::IDBKeyType::Number:
      return Compare(number_, other.number_);
    case mojom::IDBKeyType::Invalid:
    case mojom::IDBKeyType::None:
    case mojom::IDBKeyType::Min:
    default:
      NOTREACHED();
      return 0;
  }
}

}  // namespace blink
