// filesys.cc
//
// A specific design for the filesystem
//
// From the requirements in main.cc it is clear that we can't simply load
// a map<name_of_file, blob_id>. It can be at up to 2^42 bytes!
//
// A blob is 2^18 bytes, so given space is 2^52 the number of blobs is 2^34
//
// 2^40 div 2^18 is 2^22 = the maximum number of blobs a file can be.
// (2^22 * 8) div 2^18 is 2^7 = number of blobs to store all blob ids of a file!
// 2^33 files, 2^9 filename, so 2^42 div 2^18 = 2^24 blobs to just store all names!
//
// 2^18 div 2^9 = 2^9 -1 names per blob

#include "filesys.h"

#include <cstring>
#include <string>
#include <type_traits>
#include <cassert>
#include <unordered_map>

#include "blob.h"
#include "ref_counted.h"

// FNV-1a hash for 32 bits.
class fnv32 {
 public:
  static constexpr uint32_t FNV_INIT  = 0x811c9dc5UL;
  static constexpr uint32_t FNV_32_PRIME = 0x01000193UL;

  uint32_t operator()(const std::string &buf, uint32_t init = FNV_INIT) {
    return operator()(buf.c_str(), buf.length(), init);
  }

  uint32_t operator()(const char* buf, size_t len, uint32_t init = FNV_INIT) {
    auto bp = reinterpret_cast<const unsigned char *>(buf);
    const unsigned char *be = bp + len;                                     

    uint32_t hval = init;

    while (bp < be) {
      hval ^= static_cast<uint32_t>(*bp++);
      hval *= FNV_32_PRIME;
    }

    return hval;
  }
};

namespace g {

// The design is as follows. 
// Blobs are untyped data, as per problem statement. Blocks on the other hand
// are typed structure on top of a Blob. Each metadata block has a previous and
// a next pointer so they form a linked list.
//
// Each file is comprised of a |FileEntry| on a |DirBlock|, which points to
// the |ControlBlock| for the file, which has the set of blob_ids that point
// to the blobs that contain the data
//
// So:
//  Blob --> Block --> FSNode
//
// Blob #0 is special, not used elsewhere (META_RESERVED)
// Blob 1 to 2^10 are directory heads (DIR_HEADS)
// Blob DIR_HEADS to 2^34 -1 is free for data and metadata.
//
// meta block contains the next_block_id.

constexpr uint32_t META_RESERVED = 1u;
constexpr uint32_t DIR_HEADS = (1u << 10) - 1;

uint32_t name_to_dir_id(const std::string& name) {
  return (fnv32()(name)% DIR_HEADS) + META_RESERVED;
}

enum class BlocTypes : uint32_t {
  None,
  Control,
  Dir,
  Data
};

enum class Flags : uint32_t {
  None,
  New
};

struct BlockHeader {
  uint32_t type;
  uint32_t flags;
  uint64_t prev;
  uint64_t next;
};

struct ControlBlock : public BlockHeader {
  static constexpr uint32_t btype = (uint32_t)BlocTypes::Control;
  uint64_t blobs[0];
};

static_assert(sizeof(ControlBlock) == (3 * 8u));

struct FileEntry {
  char name[MAX_PATH];
  uint64_t control_blob;
};

struct DirBlock : public BlockHeader {
  static constexpr uint32_t btype = (uint32_t)BlocTypes::Dir;
  FileEntry entries[0];

  uint64_t find(const std::string& name, size_t blob_sz) const {
    auto count = (blob_sz - sizeof(*this)) / sizeof(FileEntry);
    for (size_t ix = 0; ix != count; ++ix) {
      if (name.compare(entries[ix].name) == 0) {
        return entries[ix].control_blob;
      }
    }
    return 0;
  }
};

static_assert(sizeof(DirBlock) == (3 * 8u));

static_assert(sizeof(DirBlock) == (3 * 8u));

template <typename T>
const T* Blob2Block(Blob* blob) {
  assert(blob->Get().size() >= sizeof(BlockHeader));
  auto hdr = reinterpret_cast<const BlockHeader*>(&blob->Get()[0]);
  assert(hdr->type == T::btype);
  return static_cast<const T*>(hdr);
}

template <typename T>
class FSNode : public RefCounted<FSNode<T>> {
 public:
  FSNode(uint64_t id) : id_(0u), blob_(nullptr) {
    set_blob(id);
    maybe_init();
  }

  const T* get_ro() {
    return Blob2Block<T>(blob_);
  }

  bool next() {
    if (get_ro()->next == 0) {
      return false;
    }
    set_blob(get_ro()->next);
    return true;
  }

  size_t size() const { return blob_->Get().size(); }

 private:
  void set_blob(uint64_t id) {
    if (blob_) {
      blob_->Release();
    }
    blob_ = GetBlobStore()->GetBlob(id);
    id_ = id;
  }

  void maybe_init() {
    if (blob_->Get().size() == 0) {
      T header = {};
      header.type = T::btype;
      Data data;
      data.resize(sizeof(header));
      memcpy(&data[0], &header, sizeof(header));
      blob_->Put(data);
    }
  }

  uint64_t id_;
  Blob* blob_;
};

uint64_t GetControlBlob(RefPtr<FSNode<DirBlock>> dir, const std::string& name) {
  do {
    auto cb_id = dir->get_ro()->find(name, dir->size());
    if (cb_id) {
      return cb_id;
    }

  } while (dir->next());

  // Not found. $$$
  return 0;
}

struct FILE {
  size_t position;
  ControlBlock* cb;
};

FILE* fopen(const char* filename, const char* mode) {
  std::string name(filename);
  auto dir_id = name_to_dir_id(name);
  auto dir = AdoptRef(new FSNode<DirBlock>(dir_id));
  auto cb_id = GetControlBlob(std::move(dir), name);

  return nullptr;
}
 
long fremove(const char* filename) {
  return -1;
}
 
long fclose(FILE* stream) {
  return -1;
}
 
long fread(FILE* stream, void *buffer, long count) {
  return -1;
}
 
long fwrite(FILE* stream, const void* buffer, long count) {
  return -1;
}

long ftell(FILE* stream) {
  return -1;
}
 
long fseek(FILE* stream, long offset, int origin) {
  return -1;
}

}  // namespace g


