#include "blob.h"
#include <unordered_map>

class BlobStoreImpl;

class BlobImpl : public Blob {
 public:
  BlobImpl(BlobStoreImpl* bs) : bs_(bs) {}
  const Data& Get() const override;
  int Put(const Data& data) override;
  int Release() override;

 private:
  Data data_;
  BlobStoreImpl* const bs_;
};

using BlobMap = std::unordered_map<uint64_t, BlobImpl*>;

class BlobStoreImpl : public BlobStore {
 public:
  BlobStoreImpl();
  ~BlobStoreImpl();
  Blob* GetBlob(uint64_t) override;
  uint64_t GetFreeSpace() override;

  int Store(const Data&);

 private:
  BlobMap bmap_;
  uint64_t free_space_ = 1u << 24;
};

BlobStoreImpl bs;

BlobStoreImpl::BlobStoreImpl() {
  // Initialization goes here.
}

BlobStoreImpl::~BlobStoreImpl() {
  // Termination goes here.
}

Blob* BlobStoreImpl::GetBlob(uint64_t id) {
  auto item = bmap_.find(id);
  if (item != bmap_.end()) {
    return item->second;
  }

  auto bs = new BlobImpl(this);
  bmap_[id] = bs;
  return bs;
}

uint64_t BlobStoreImpl::GetFreeSpace() {
  return free_space_;
}

int BlobStoreImpl::Store(const Data& data) {
  free_space_-= data.size();
  // $fixme: actually store? or do it at Release() time.
  return 0;
}

BlobStore* GetBlobStore() {
  return &bs;
}

const Data& BlobImpl::Get() const {
  return data_;
}

int BlobImpl::Put(const Data& data) {
  if (data.size() > MaxBlobSize) {
    return ErrBadArgs;
  }

  data_ = data;
  return bs_->Store(data_);
}

int BlobImpl::Release() {
  return 0;
}
