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
#include <memory>

// Helper for lazy initialization of objects.
// Usage:
//    Lazy<Foo> my_lazy_object([]() { return new Foo; });

template <typename T>
class Lazy {
 public:
  explicit Lazy(std::function<T*()> init) : init_(init) {}

  // Convenience constructor that will lazily default construct the object.
  Lazy() : init_([]() { return new T; }) {}

  T* get() const {
    CheckInitialised();
    return ptr_.get();
  }

  typename std::add_lvalue_reference<T>::type operator*() const {
    CheckInitialised();
    return *ptr_;
  }

  T* operator->() const { return get(); }

  // Returns true if the object is not yet initialised.
  explicit operator bool() const { return ptr_; }

  // Deletes the underlying object and will re-run the initialisation function if the object is requested again.
  void reset() { ptr_.reset(); }

 private:
  void CheckInitialised() const {
    if (!ptr_) {
      ptr_.reset(init_(), [](T*obj) { obj->deleteLater(); });
    }
  }

  const std::function<T*()> init_;
  mutable std::shared_ptr<T> ptr_;
};

#endif  // LAZY_H
