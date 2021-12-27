// cpu@google.com  oct 2021.
//
// This is the Blob service that is already implemented
// by somebody very smart. A toy implementation is included
// so that you can observe and debug your filesys
// implementation.

#include <vector>
#include <stdint.h>
#include <cstddef>


using Data = std::vector<uint8_t>;
 
constexpr size_t MaxBlobSize = 256 * 1024;
constexpr int ErrOutofSpace = -1;
constexpr int ErrBadArgs = -2;
constexpr int ErrInternal = -3;
 
class Blob {
 public:
  virtual const Data& Get() const = 0;
  virtual int Put(const Data& data) = 0;
  virtual int Release() = 0;
};
 
class BlobStore {
 public:
  virtual Blob* GetBlob(uint64_t id) = 0; // Use Blob::Release() to free.
  virtual uint64_t GetFreeSpace() = 0;
};

BlobStore* GetBlobStore();
