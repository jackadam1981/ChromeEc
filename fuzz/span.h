// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef __FUZZ_SPAN_H
#define __FUZZ_SPAN_H

#define TO_SPAN(field) \
  (fuzz::span<uint8_t>(reinterpret_cast<uint8_t*>(&(field)), sizeof(field)))

#define TO_CONST_SPAN(field) \
  (fuzz::span<const uint8_t>(reinterpret_cast<const uint8_t*>(&(field)), \
                             sizeof(field)))

namespace fuzz {

template <typename T>
class span {
 public:
  span(std::nullptr_t dummy) : span<T>() {}
  span() : span<T>(nullptr, nullptr) {}
  span(T* begin, T* end) : begin_(begin), end_(end) {}
  span(T* begin, size_t size) : begin_(begin), end_(begin + size) {}

  T* begin() { return begin_; }
  T* end() { return end_; }

  span<T> slice(ptrdiff_t start) {
    size_t size_ = size();
    if (start > size_ || -start > size_) {
      return fuzz::span<T>(end_, end_);
    }
    if (start > 0) {
      return fuzz::span<T>(begin_ + start, end_);
    } else {
      return fuzz::span<T>(end_ + start, end_);
    }
  }

  size_t FillWith(span<const T> other, T fill_value) {
    if (other.size() >= size()) {
      std::copy(other.begin(), other.begin() + size(), begin_);
      return size();
    }
    std::copy(other.begin(), other.end(), begin_);
    std::fill(begin_ + other.size(), end_, fill_value);
    return other.size();
  }

  bool empty() const { return begin_ == end_; }
  size_t size() const { return end_ - begin_; }

 private:
  T* begin_;
  T* end_;
};

template <typename Value, typename Container>
span<Value> DataToSpan(const Container& container) {
  return fuzz::span<Value>(reinterpret_cast<Value*>(container.data()),
                           container.size());
};

}  // namespace fuzz

#endif  // __FUZZ_SPAN_H
