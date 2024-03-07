#pragma once

#include <algorithm>
#include <array>
#include <compare>
#include <iterator>
#include <vector>
#include <stdexcept>


template<typename ValueType>
class Deque {
  static const size_t BLOCK_SIZE = std::max(size_t{1}, 4096 / sizeof(ValueType));

  using Block = std::array<ValueType, BLOCK_SIZE>;

  static inline std::allocator<Block> allocator{};


  template<bool IsConst>
  class BaseIterator {
  protected:
    template<typename T>
    friend
        class Deque;

    using vector_iterator_ty = std::conditional_t<IsConst, typename std::vector<Block *>::const_iterator, typename std::vector<Block *>::iterator>;
    using block_iterator_ty = std::conditional_t<IsConst, typename Block::const_iterator, typename Block::iterator>;

    vector_iterator_ty vector_iterator_;
    block_iterator_ty block_iterator_;

    BaseIterator(vector_iterator_ty vector_it, block_iterator_ty block_it) {
      this->vector_iterator_ = vector_it;
      this->block_iterator_ = block_it;
    }

  public:
    using iterator_category = std::random_access_iterator_tag;
    using difference_type = std::ptrdiff_t;
    using value_type = ValueType;
    using pointer = std::conditional_t<IsConst, const ValueType *, ValueType *>;
    using reference = std::conditional_t<IsConst, const ValueType &, ValueType &>;

    BaseIterator() {}

    operator BaseIterator<true>() const {
      return BaseIterator<true>(this->vector_iterator_, this->block_iterator_);
    }


    bool operator==(const BaseIterator &other) const {
      return block_iterator_ == other.block_iterator_;
    }

    std::weak_ordering operator<=>(const BaseIterator &other) const {
      return std::tie(vector_iterator_, block_iterator_) <=>
             std::tie(other.vector_iterator_, other.block_iterator_);
    }


    reference operator*() const {
      return *block_iterator_;
    }

    pointer operator->() const {
      return &**this;
    }

    BaseIterator<IsConst> &operator++() {
      ++this->block_iterator_;
      if (this->block_iterator_ == (*this->vector_iterator_)->end()) {
        ++this->vector_iterator_;
        this->block_iterator_ = (*this->vector_iterator_)->begin();
      }
      return *this;
    }

    BaseIterator<IsConst> operator++(int) {
      BaseIterator<IsConst> result(*this);
      ++*this;
      return result;
    }

    BaseIterator<IsConst> &operator--() {
      if (this->block_iterator_ == (*this->vector_iterator_)->begin()) {
        --this->vector_iterator_;
        this->block_iterator_ = (*this->vector_iterator_)->end();
      }
      --this->block_iterator_;
      return *this;
    }

    BaseIterator<IsConst> operator--(int) {
      BaseIterator<IsConst> result(*this);
      --*this;
      return result;
    }

    BaseIterator<IsConst> &operator+=(size_t shift) {
      size_t till_end_of_block = (*this->vector_iterator_)->end() - this->block_iterator_;
      if (shift < till_end_of_block) {
        this->block_iterator_ += shift;
      } else {
        shift += BLOCK_SIZE - till_end_of_block;
        this->vector_iterator_ += shift / BLOCK_SIZE;
        this->block_iterator_ = (*this->vector_iterator_)->begin() + shift % BLOCK_SIZE;
      }
      return *this;
    }

    BaseIterator<IsConst> &operator-=(size_t shift) {
      size_t till_start_of_block = this->block_iterator_ - (*this->vector_iterator_)->begin();
      if (shift <= till_start_of_block) {
        this->block_iterator_ -= shift;
      } else {
        shift += BLOCK_SIZE - till_start_of_block;
        this->vector_iterator_ -= shift / BLOCK_SIZE;
        this->block_iterator_ = (*this->vector_iterator_)->end() - shift % BLOCK_SIZE;
      }
      return *this;
    }

    BaseIterator<IsConst> operator+(size_t shift) const {
      BaseIterator<IsConst> result(*this);
      return result += shift;
    }

    BaseIterator<IsConst> operator-(size_t shift) const {
      BaseIterator<IsConst> result(*this);
      return result -= shift;
    }

    ptrdiff_t operator-(const BaseIterator<IsConst> &other) const {
      ptrdiff_t diff = (this->vector_iterator_ - other.vector_iterator_) * BLOCK_SIZE;
      diff += this->block_iterator_ - (*this->vector_iterator_)->begin();
      diff -= other.block_iterator_ - (*other.vector_iterator_)->begin();
      return diff;
    }

    reference operator[](ptrdiff_t offset) const {
      return *(*this + offset);
    }
  };

  BaseIterator<false> left_;
  BaseIterator<false> right_;

  std::vector<Block *> list_links_;


  void double_blocks() {
    std::vector<Block *> new_list_links(2 * list_links_.size(), nullptr);

    auto mid_block = new_list_links.begin() + (new_list_links.size() + 1) / 2;
    auto left_block = mid_block - (list_links_.size() + 1) / 2;
    std::copy(list_links_.begin(), list_links_.end(), left_block);

    left_.vector_iterator_ = left_block + (left_.vector_iterator_ - list_links_.begin());
    right_.vector_iterator_ = left_block + (right_.vector_iterator_ - list_links_.begin());
    list_links_ = std::move(new_list_links);
  }

  void ensure_block(Block *&block) {
    if (block == nullptr) {
      block = allocator.allocate(1);
    }
  }

  void reserve_element_left() {
    if (left_.block_iterator_ != (*left_.vector_iterator_)->begin()) {
      return;
    }
    if (left_.vector_iterator_ == list_links_.begin()) {
      double_blocks();
    }
    ensure_block(left_.vector_iterator_[-1]);
  }

  void reserve_element_right() {
    if (right_.block_iterator_ + 1 != (*right_.vector_iterator_)->end()) {
      return;
    }
    if (right_.vector_iterator_ + 1 == list_links_.end()) {
      double_blocks();
    }
    ensure_block(right_.vector_iterator_[1]);
  }

  void reserve_init(size_t size) {
    for (size_t i = 0; i < size / BLOCK_SIZE + 2; i++) {
      list_links_.push_back(allocator.allocate(1));
    }
    left_ = {list_links_.begin() + 1, list_links_[1]->begin()};
    right_ = left_;
  }

  void deallocate() {
    std::destroy(left_, right_);
    for (Block *block: list_links_) {
      if (block != nullptr) {
        allocator.deallocate(block, 1);
      }
    }
  }

  void swap(Deque<ValueType> &other) {
    std::swap(left_, other.left_);
    std::swap(right_, other.right_);
    list_links_.swap(other.list_links_);
  }

public:
  using const_iterator = BaseIterator<true>;
  using iterator = BaseIterator<false>;
  using const_reverse_iterator = std::reverse_iterator<const_iterator>;
  using reverse_iterator = std::reverse_iterator<iterator>;


  Deque() {
    reserve_init(0);
  }

  explicit Deque(size_t size) {
    reserve_init(size);
    try {
      for (size_t i = 0; i < size; ++i) {
        new(&*right_) ValueType{};
        ++right_;
      }
    } catch (...) {
      deallocate();
      throw;
    }
  }

  Deque(size_t size, const ValueType &value) {
    reserve_init(size);
    try {
      for (size_t i = 0; i < size; ++i) {
        new(&*right_) ValueType{value};
        ++right_;
      }
    }
    catch (...) {
      deallocate();
      throw;
    }
  }

  Deque(const Deque<ValueType> &other) {
    reserve_init(other.size());
    try {
      for (const auto &value: other) {
        new(&*right_) ValueType{value};
        ++right_;
      }
    }
    catch (...) {
      deallocate();
      throw;
    }
  }

  Deque<ValueType> &operator=(const Deque<ValueType> &other) {
    Deque<ValueType> object(other);
    swap(object);
    return *this;
  }


  size_t size() const {
    return right_ - left_;
  }


  ValueType &operator[](size_t position) noexcept {
    return left_[position];
  }

  const ValueType &operator[](size_t position) const noexcept {
    return left_[position];
  }

  ValueType &at(size_t position) {
    if (position >= size()) {
      throw std::out_of_range("Deque index out of range.");
    }
    return (*this)[position];
  }

  const ValueType &at(size_t position) const {
    if (position >= size()) {
      throw std::out_of_range("Deque index out of range.");
    }
    return (*this)[position];
  }


  void push_back(const ValueType &new_element) {
    reserve_element_right();
    new(&*right_) ValueType{new_element};
    ++right_;
  }

  void pop_back() {
    std::destroy_at(&*--right_);
  }

  void push_front(const ValueType &new_element) {
    reserve_element_left();
    auto it = left_;
    --it;
    new(&*it) ValueType{new_element};
    left_ = it;
  }

  void pop_front() {
    std::destroy_at(&*left_++);
  }


  void insert(iterator it, const ValueType &new_element) {
    reserve_element_right();
    if (it == right_) {
      new(&*right_) ValueType{new_element};
    } else {
      auto last = right_;
      --last;
      new(&*right_) ValueType{std::move(*last)};
      std::move_backward(it, last, right_);
      *it = new_element;
    }
    ++right_;
  }

  void erase(iterator it) {
    std::move(it + 1, right_, it);
    pop_back();
  }


  const_iterator cbegin() const {
    return left_;
  }

  const_iterator cend() const {
    return right_;
  }

  iterator begin() {
    return left_;
  }

  iterator end() {
    return right_;
  }

  const_iterator begin() const {
    return cbegin();
  }

  const_iterator end() const {
    return cend();
  }

  const_reverse_iterator crbegin() const {
    return std::make_reverse_iterator(cend());
  }

  const_reverse_iterator crend() const {
    return std::make_reverse_iterator(cbegin());
  }

  reverse_iterator rbegin() {
    return std::make_reverse_iterator(end());
  }

  reverse_iterator rend() {
    return std::make_reverse_iterator(begin());
  }

  const_reverse_iterator rbegin() const {
    return crbegin();
  }

  const_reverse_iterator rend() const {
    return crend();
  }


  ~Deque() {
    deallocate();
  }
};