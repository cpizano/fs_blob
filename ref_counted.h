#include <stdint.h>
#include <cassert>


template <typename T>
class RefCounted {
 public:
  RefCounted() : rc_(0) {}
  void Adopt() { rc_ = 1; }
  void AddRef() { 
    assert(rc_ > 0);
    ++rc_; 
  }
  int32_t Release() { return --rc_;}

  RefCounted(const RefCounted&) = delete;
  RefCounted(RefCounted&&) = delete;
  RefCounted& operator=(const RefCounted&) = delete;
  RefCounted& operator=(RefCounted&&) = delete;

 private:
  int32_t rc_ = 0;
};

template <typename T>
class RefPtr;
template <typename T>
RefPtr<T> AdoptRef(T* ptr);

template <typename T>
class RefPtr {
 public:
  constexpr RefPtr() : ptr_(nullptr) {}
  constexpr RefPtr(decltype(nullptr)) : RefPtr() {}
  explicit RefPtr(T* p) : ptr_(p) {
    if (ptr_)
      ptr_->AddRef();
  }

  ~RefPtr() {
    if (ptr_ && (ptr_->Release() == 0)) {
      delete ptr_;
    }
  }

  RefPtr& operator=(const RefPtr& r) {
    if (r.ptr_) {
      r.ptr_->AddRef();
    }
    T* old = ptr_;
    ptr_ = r.ptr_;
    if (old && old->Release()) {
      delete old;
    }
    return *this;
  }

  // Copy construction.
  RefPtr(const RefPtr& r) : RefPtr(r.ptr_) {}
  // Move construction.
  RefPtr(RefPtr&& r) : ptr_(r.ptr_) { r.ptr_ = nullptr; }

  T* get() const { return ptr_; }
  T& operator*() const { return *ptr_; }
  T* operator->() const { return ptr_; }
  explicit operator bool() const { return !!ptr_; }

  bool operator==(decltype(nullptr)) const { return (ptr_ == nullptr); }
  bool operator!=(decltype(nullptr)) const { return (ptr_ != nullptr); }
  bool operator==(const RefPtr<T>& other) const { return ptr_ == other.ptr_; }
  bool operator!=(const RefPtr<T>& other) const { return ptr_ != other.ptr_; }

 private:
  friend RefPtr<T> AdoptRef<T>(T*);
  enum AdoptTag { ADOPT };
  RefPtr(T* ptr, AdoptTag) : ptr_(ptr) {
    if (ptr_) {
      ptr_->Adopt();
    }
  }

  T* ptr_;
};


template <typename T>
inline RefPtr<T> AdoptRef(T* ptr) {
  return RefPtr<T>(ptr, RefPtr<T>::ADOPT);
}
