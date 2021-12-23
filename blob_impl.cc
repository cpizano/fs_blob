#include "blob.h"
#include <unordered_map>

#include <ctype.h>
#include <stdio.h>

namespace {

void hexdump(const void *ptr, int buflen, int cols = 16) {
  auto *buf = (const unsigned char*)ptr;
  int i, j;
  for (i = 0; i < buflen; i+= cols) {
    printf("%06x: ", i);
    for (j = 0; j < cols; j++) 
      if (i + j < buflen)
        printf("%02x ", buf[i + j]);
      else
        printf("   ");
    printf(" ");
    for (j = 0; j < cols; j++) 
      if ( i + j < buflen)
        printf("%c", isprint(buf[i+j]) ? buf[i+j] : '.');
    printf("\n");
  }
}

}


class BlobStoreImpl;

class BlobImpl : public Blob {
 public:
  BlobImpl(uint64_t id, BlobStoreImpl* bs) : id_(id), bs_(bs) {}
  const Data& Get() const override;
  int Put(const Data& data) override;
  int Release() override;

 private:
  const uint64_t id_;
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

  int Store(const Data&, uint64_t id);

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

  auto bs = new BlobImpl(id, this);
  bmap_[id] = bs;
  return bs;
}

uint64_t BlobStoreImpl::GetFreeSpace() {
  return free_space_;
}

int BlobStoreImpl::Store(const Data& data, uint64_t id) {
  free_space_-= data.size();
  // $fixme: actually store? or do it at Release() time.
  printf(">> 0x%x  sz: %zu\n", id, data.size());
  hexdump(&data[0], data.size());

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
  return bs_->Store(data_, id_);
}

int BlobImpl::Release() {
  return 0;
}
