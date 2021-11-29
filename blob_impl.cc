#include "blob.h"
#include <unordered_map>

class BlobImpl : public Blob {
 public:
  const Data& Get() override;
  int Put(const Data& data) override;
  int Release() override;

 private:
  Data data_;
};

using BlobMap = std::unordered_map<uint64_t, BlobImpl*>;

class BlobStoreImpl : public BlobStore {
 public:
  BlobStoreImpl();
  Blob* GetBlob(uint64_t) override;
  uint64_t GetFreeSpace() override;

 private:
  BlobMap bmap_;
};

BlobStoreImpl bs;

BlobStoreImpl::BlobStoreImpl() {
  // Initialization goes here.
}

Blob* BlobStoreImpl::GetBlob(uint64_t id) {
  auto item = bmap_.find(id);
  if (item == bmap_.end()) {
    auto bs = new BlobImpl();
    bmap_[id] = bs;
    return bs;
  }

  return item->second;
}

uint64_t BlobStoreImpl::GetFreeSpace() {
  return 1024l;
}

BlobStore* GetBlobStore() {
  return &bs;
}

const Data& BlobImpl::Get() {
  return data_;
}

int BlobImpl::Put(const Data& data) {
  if (data.size() > MaxBlobSize) {
    return ErrBadArgs;
  }

  data_ = data;
  return 0;
}

int BlobImpl::Release() {
  // We don't delete, so the persistence is in memory.
  return 0;
}
