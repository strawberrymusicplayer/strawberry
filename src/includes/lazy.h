/* This file is part of Strawberry.
   Copyright 2016, John Maguire <john.maguire@gmail.com>

   Strawberry is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Strawberry is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Strawberry.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef LAZY_H
#define LAZY_H

#include <functional>
#include <type_traits>

#include "core/logging.h"

#include "shared_ptr.h"

// Helper for lazy initialization of objects.
// Usage:
//    Lazy<Foo> my_lazy_object([]() { return new Foo; });

template<typename T>
class Lazy {
 public:
  explicit Lazy(std::function<T*()> init) : init_(init) {}

  // Convenience constructor that will lazily default construct the object.
  Lazy() : init_([]() { return new T; }) {}

  T* get() const {
    CheckInitialized();
    return ptr_.get();
  }

  SharedPtr<T> ptr() const {
    CheckInitialized();
    return ptr_;
  }

  typename std::add_lvalue_reference<T>::type operator*() const {
    CheckInitialized();
    return *ptr_;
  }

  T* operator->() const { return get(); }

  // Returns true if the object is initialized.
  explicit operator bool() const { return ptr_ != nullptr; }

  // Deletes the underlying object and will re-run the initialization function if the object is requested again.
  void reset() { ptr_.reset(); }

 private:
  void CheckInitialized() const {
    if (!ptr_) {
      ptr_ = SharedPtr<T>(init_(), [](T*obj) { qLog(Debug) << obj << "deleted"; delete obj; });
      qLog(Debug) << &*ptr_ << "created";
    }
  }

  const std::function<T*()> init_;
  mutable SharedPtr<T> ptr_;
};

#endif  // LAZY_H
