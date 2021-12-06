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

#include "filesys.h"

#include <cstring>
#include <string>
#include <type_traits>
#include <unordered_map>

#include "blob.h"

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
// are typed structure on top of a Blob. Each block has a previous and a next
// pointer so they form a BlockStream.
//
// Each file is comprised of A |FileEntry| on a |DirBlock|, which points to
// the |ControlBlock| for the file, which has the metadata
// and the set of |DataBlock| that points to the blobs that contain the data
//
// So:
//  Blob --> Block --> BlockStream
//
// The |FileSystem| is in charge of implementing caching, garbage collection
// and find free blobs (fast).
//
// Free blocks are stored as a bitmap in the blocks 0 to BITMAP_RESERVED -1
// where BITMAP_RESERVED is 2^34 / (2^18 * 2^3) = 2^11. at runtime they

constexpr uint32_t BITMAP_RESERVED = 1u << 11;
constexpr uint32_t MODULUS_BLOB = 1u << 21;

uint32_t name_to_dir_id(const std::string& name) {
  return (fnv32()(name)% MODULUS_BLOB) + BITMAP_RESERVED;
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

struct FileEntry {
  char name[MAX_PATH];
  uint64_t control_blob;
};

struct Block {
  uint32_t type;
  uint32_t flags;
  uint64_t prev;
  uint64_t next;
};

struct ControlBlock : public Block {
  static constexpr uint32_t type = (uint32_t)BlocTypes::Control;
  using entry = uint64_t;
  uint64_t last_mod;
  uint64_t blobs[0];
};

static_assert(sizeof(ControlBlock) == (4 * 8u));

struct DirBlock : public Block {
  static constexpr uint32_t type = (uint32_t)BlocTypes::Dir;
  using entry = FileEntry;
  FileEntry entries[0];
};

static_assert(sizeof(DirBlock) == (3 * 8u));

struct DataBlock : public Block {
  static constexpr uint32_t type = (uint32_t)BlocTypes::Data;
  char data[0];
};

static_assert(sizeof(DirBlock) == (3 * 8u));

const Block* GetHeader(Blob* blob) {
  if (blob->Get().size() < sizeof(Block)) {
    return nullptr;
  }
  return reinterpret_cast<const Block*>(&(blob->Get()[0]));
}

template <typename TBlock>
const TBlock* GetBlock(Blob* blob) {
  if (blob->Get().size() < sizeof(Block)) {
    TBlock block {TBlock::type, (uint32_t)Flags::New, 0, 0};
    Data data(sizeof(block));
    memcpy(&data[0], &block, sizeof(block));
    if (blob->Put(data) != 0) {
      return nullptr;
    }
  }

  auto block = reinterpret_cast<const TBlock*>(&(blob->Get()[0]));
  if (block->type != TBlock::type) {
    return nullptr;
  }

  return block;
}

template <typename TBlock>
int ChainBlock(TBlock* last, Blob* next, uint64_t next_id) {
  // $fixme
  return 0;
}

template <typename TBlock>
int AppendToBlock(const typename TBlock::entry& entry, Blob* blob) {
  Data new_data = blob->Get();
  auto old_size = new_data.size();
  auto new_size = old_size + sizeof(typename TBlock::entry);
  if (new_size > MaxBlobSize) {
    return false;
  }
  new_data.resize(new_size);
  memcpy(&new_data[old_size], &entry, sizeof(entry));
  return blob->Put(new_data);
}

bool IsEoB(Blob* blob, const void* addr) {
  auto last = &blob->Get()[blob->Get().size()];
  return addr >= reinterpret_cast<const void*>(last);
}

class FileSystem;

// A chained set of blocks.
class BlockStream {
  public:
  BlockStream(Blob* blob, FileSystem* fs) : blob_(blob), fs_(fs) {}
  
  bool IsValid() const { return blob_ != nullptr; }
  Blob* operator()() const { return blob_; }
  bool next();
  int append();

  private:
  Blob* blob_;
  FileSystem* const fs_;
};


class FileSystem {
 public:
  BlockStream GetBlockIter(uint64_t id) {
    return BlockStream(GetBlob(id), this);
  }

  auto FreeBlob() {
    // $fixme, serialize used ids back to storage.
    struct _ {
      Blob* blob;
      uint64_t id;
    };

    auto next = used_ids_.empty() ? BITMAP_RESERVED : used_ids_.back();
    do {
      auto blob = GetBlob(next);
      if (blob->Get().empty()) {
        // it is free.
        used_ids_.push_back(next);
        return _{blob, next};
      }

      next++;
    } while (true);
  }

 private:
  friend class BlockStream;

  Blob* GetBlob(uint64_t id) {
    auto it = ws_.find(id);
    if (it == ws_.end()) {
      auto blob = GetBlobStore()->GetBlob(id);
      if (blob == nullptr) {
        return nullptr;
      }
      live_blobs_++;
      VBlob vb{0, blob};

      it = ws_.insert({id, vb}).first;
    }

    it->second.hit_count++;
    return it->second.blob;
  }

  struct VBlob {
    uint32_t hit_count;
    Blob* blob;
  };

  std::unordered_map<uint64_t, VBlob> ws_;  // working set.
  std::vector<uint64_t> used_ids_;

  uint32_t live_blobs_ = 0;
} fs;


bool BlockStream::next() {
  auto hdr = GetHeader(blob_);
  if (hdr == nullptr) {
    return false;
  }

  auto next = fs_->GetBlob(hdr->next);
  if (next == nullptr) {
    return false;
  }
  blob_ = next;
  return true;
}

int BlockStream::append() {
  auto hdr = GetHeader(blob_);
  if (hdr->next != 0) {
    return -1;
  }

  auto [new_blob, id] = fs_->FreeBlob();

  // $fixme
  return 0;
}


uint64_t DirToControlBlock(BlockStream blobit, const std::string name, bool create) {
  const DirBlock* dir;

  do {
    dir = GetBlock<DirBlock>(blobit());
    if (dir == nullptr) {
      return 0;  // $fixme
    }

    size_t ix = 0;
    do {
      if (dir->entries[ix].name == name) {
        return dir->entries[ix].control_blob;
      }
      ++ix;
    } while (!IsEoB(blobit(), &dir->entries[ix]));

  } while (blobit.next());

  // No next block. If the file needs to exits we fail here.
  if (!create) {
    return 0u;
  }

  // We can create the file.
  auto [_, id] = fs.FreeBlob();

  FileEntry entry{0};
  entry.control_blob = id;
  name.copy(entry.name, sizeof(entry.name));

  // Can we append to the last one?
  
  if (AppendToBlock<DirBlock>(entry, blobit()) == 0) {
    return entry.control_blob;
  }

  // We need to chain a new blob.


  return 0;
}


struct FILE {
  size_t position;
};

FILE* fopen(const char* filename, const char* mode) {
  std::string name(filename);
  auto directory = fs.GetBlockIter(name_to_dir_id(name));
  if (!directory.IsValid()) {
    return nullptr;
  }

  auto cb = DirToControlBlock(directory, name, true);
  if (cb == 0) {


  }

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


