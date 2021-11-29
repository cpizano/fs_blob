
#include <vector>
#include <stdint.h>


using Data = std::vector<uint8_t>;
 
constexpr size_t MaxBlobSize = 256 * 1024;
constexpr int ErrOutofSpace = -1;
constexpr int ErrBadArgs = -2;
constexpr int ErrInternal = -3;
 
class Blob {
 public:
  virtual const Data& Get() =0;
  virtual int Put(const Data& data) = 0;
  virtual int Release() = 0;
};
 
class BlobStore {
 public:
  virtual Blob* GetBlob(uint64_t id) = 0; // Use Blob::Release() to free.
  virtual uint64_t GetFreeSpace() = 0;
};

BlobStore* GetBlobStore();
