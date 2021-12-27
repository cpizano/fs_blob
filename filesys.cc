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
// to the blobs that contain the data.
// 
//  Both control blocks and directory blocks are chained (via prev, next).
//
// So code wise there is a hiearchy:
//
//                                | DirBlock
//  Blob --> FSNode  <-- Block  <-|
//                                | ControlBlock
//
//  File data is untyped, can only be found by being pointed
//  from control blocks.
//
//
// Disk layout:
// Blob #0 is special contains META_DISK.
// Blob 1 to 2^10 are directory heads (DIR_HEADS)
// Blob DIR_HEADS to 2^34 -1 is free for data and metadata.
//
// meta block contains the next_free_blob_id.
//
//
//  Structure traversal.
//  
//
//  name --> (hash_fvn % DIR_HEADS) --> DirBlock
//                                         |
//                                         v
//                                      DirBlock ---> ControlBlock
//                                         |               |
//                                         V               V            (data)
//                                                     ControlBlock |---> Blob
//                                                                  |---> Blob
//
//  1. Direct hashing points to the blob id (from 1 to DIR_HEADS - 1) that might
//     contain the {name, control-id} pair. This is a sequential search of all
//     chained directories.
//  2. Once found, the control block points to the data id of the blob that contains
//     the range of interest.

constexpr uint32_t META_RESERVED = 1u;
constexpr uint32_t DIR_HEADS = (1u << 10);

constexpr char magic[16] = "vdisk2021-00001";

struct META_DISK {
  char magic[16];
  uint64_t version;
  uint64_t next_free;
};

META_DISK* g_meta = nullptr;

uint64_t get_next_free_id() { return g_meta->next_free++; }

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
  BlocTypes type;
  Flags flags;
  uint64_t prev;
  uint64_t next;
};

struct ControlBlock : public BlockHeader {
  typedef uint64_t Record;
  static constexpr auto btype = BlocTypes::Control;
  uint64_t directory;
  uint64_t start;
  Record blobs[0];

  // Find data block starting at |pos|.
  uint64_t find(size_t pos, size_t blob_sz) const {
    auto count = (blob_sz - sizeof(*this)) / sizeof(Record);
    auto ix = pos / MaxBlobSize;
    if (ix >= count) {
      return 0;
    }
    return blobs[ix];
  }
};

static_assert(sizeof(ControlBlock) == (5 * 8u));
static constexpr size_t bytes_per_ctrl_block =
  MaxBlobSize * ((MaxBlobSize - sizeof(ControlBlock))/ sizeof(ControlBlock::Record));

struct FileEntry {
  char name[MAX_PATH];
  uint64_t control_blob;
};

struct DirBlock : public BlockHeader {
  typedef FileEntry Record;
  static constexpr auto btype = BlocTypes::Dir;
  Record entries[0];

  // Find control block for file |name|.
  uint64_t find(const std::string& name, size_t blob_sz) const {
    auto count = (blob_sz - sizeof(*this)) / sizeof(Record);
    for (size_t ix = 0; ix != count; ++ix) {
      if (name.compare(entries[ix].name) == 0) {
        return entries[ix].control_blob;
      }
    }
    return 0;
  }

};

static_assert(sizeof(DirBlock) == (3 * 8u));

template <typename T>
const T* Blob2Block(Blob* blob) {
  assert(blob->Get().size() >= sizeof(BlockHeader));
  auto hdr = reinterpret_cast<const BlockHeader*>(&blob->Get()[0]);
  assert(hdr->type == T::btype);
  return static_cast<const T*>(hdr);
}

template <typename THeader>
bool WriteHeader(Blob* blob, const THeader& hdr) {
  assert(blob->Get().size() >= sizeof(BlockHeader));
  Data data = blob->Get();
  auto old_hdr = reinterpret_cast<THeader*>(&data[0]);
  assert(old_hdr->type == hdr.type);
  *old_hdr = hdr;
  return blob->Put(data);
}

template <typename T>
class FSNode : public RefCounted<FSNode<T>> {
 public:
  FSNode(uint64_t id) : id_(0u), blob_(nullptr) {
    set_blob(id);
    maybe_init();
  }

  ~FSNode() {
    blob_->Release();
  }

  bool set_next(uint64_t id) {
    BlockHeader hdr = *Blob2Block<T>(blob_);
    hdr.next = id;
    return WriteHeader(blob_, hdr);
  }

  bool set_previous(uint64_t id) {
    BlockHeader hdr = *Blob2Block<T>(blob_);
    hdr.prev = id;
    return WriteHeader(blob_, hdr);
  }

  template <typename Func>
  bool update_header(Func fn) {
    auto new_header = fn(Blob2Block<T>(blob_));
    return WriteHeader<T>(blob_, new_header);
  }

  const T* get_ro() {
    return Blob2Block<T>(blob_);
  }

  bool append_record(const typename T::Record& rec) {
    if (size() > (MaxBlobSize - sizeof(rec))) {
      return false;
    }

    Data bytes = blob_->Get();
    auto old_sz = bytes.size();
    bytes.resize(old_sz + sizeof(rec));
    memcpy(&bytes[old_sz], &rec, sizeof(rec));
    return (blob_->Put(bytes) == 0);
  }

  bool next() {
    if (get_ro()->next == 0) {
      return false;
    }
    set_blob(get_ro()->next);
    return true;
  }

  bool prev() {
    if (get_ro()->prev == 0) {
      return false;
    }
    set_blob(get_ro()->prev);
    return true;
  }

  size_t size() const { return blob_->Get().size(); }
  uint64_t id() const { return id_; }

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

template <typename T>
RefPtr<FSNode<T>> ChainBlock(RefPtr<FSNode<T>> prev) {
  auto new_block = AdoptRef(new FSNode<T>(get_next_free_id()));
  new_block->set_previous(prev->id());
  prev->set_next(new_block->id());
  return new_block;
}

enum CbAction {
  FileMustExist,
  FileCreate,
};


RefPtr<FSNode<ControlBlock>> GetControlBlob(RefPtr<FSNode<DirBlock>> dir,
                                            const std::string& name,
                                            CbAction action) {
  do {
    auto cb_id = dir->get_ro()->find(name, dir->size());
    if (cb_id) {
      return AdoptRef(new FSNode<ControlBlock>(cb_id));;
    }

  } while (dir->next());

  // File entry not found.
  if (action == FileMustExist) {
    return 0;
  }

  // Create new control block and entry
  auto ctrl_block = AdoptRef(new FSNode<ControlBlock>(get_next_free_id()));

  ctrl_block->update_header([dir_id =  dir->id()](const ControlBlock* hdr){
    ControlBlock new_hdr = *hdr;
    new_hdr.directory = dir_id;
    return new_hdr;
  });

  FileEntry entry {};
  entry.control_blob = ctrl_block->id();
  name.copy(entry.name, sizeof(entry.name));

  // Try append to current directory
  if (dir->append_record(entry)) {
    return ctrl_block;
  }

  // Full dir blob. Chain a new dir entry.
  auto new_dir = ChainBlock(dir);

  // Append to new dir entry.
  if (new_dir->append_record(entry)) {
    return ctrl_block;
  }

  return 0;
}

void finitialize() {
  META_DISK* meta = nullptr;

  auto blob = GetBlobStore()->GetBlob(0u);
  if (blob->Get().size() < sizeof(META_DISK)) {
    // Init disk.
    meta = new META_DISK {{}, 1, DIR_HEADS + 1};
    memcpy(meta->magic, magic, sizeof(magic));
    Data bytes(sizeof(META_DISK));
    memcpy(&bytes[0], meta, sizeof(META_DISK));
    blob->Put(bytes);
  } else {
    // Validate disk.
    auto actual = reinterpret_cast<const META_DISK*>(&blob->Get()[0]);
    if (strcmp(actual->magic, magic) != 0) {
      assert(false);
    }
    assert(actual->version == 1);
    assert(actual->next_free > DIR_HEADS);
    meta = new META_DISK {*actual};
  }

  blob->Release();
  g_meta = meta;
}

void ffinalize() {
  Data data(sizeof(META_DISK));
  memcpy(&data[0], g_meta, sizeof(META_DISK));
  auto blob = GetBlobStore()->GetBlob(0u);
  blob->Put(data);
  blob->Release();
  delete g_meta;
}

struct FILE {
  size_t position;
  RefPtr<FSNode<ControlBlock>> cb;
};


FILE* fopen(const char* filename, const char* mode) {
  CbAction action = ((mode[0] == 'w') || (mode[1] == 'w')) ?
    FileCreate : FileMustExist;

  std::string name(filename);
  auto dir_id = name_to_dir_id(name);
  auto dir = AdoptRef(new FSNode<DirBlock>(dir_id));
  auto ctrl_block = GetControlBlob(std::move(dir), name, action);
  if (!ctrl_block) {
    return nullptr;
  }

  return new FILE { 0, std::move(ctrl_block) };
}

long fclose(FILE* stream) {
  delete stream;
  return 0;
}
 
long fread(FILE* stream, void *buffer, long count) {
  return -1;
}
 
long fwrite(FILE* stream, const void* buffer, long count) {
  uint64_t start_ctrl_block = stream->position / bytes_per_ctrl_block;
  size_t offset = stream->position % MaxBlobSize;
  auto& cb = stream->cb;

  if ((offset + count) > MaxBlobSize) {
    // $$ fixme. support split writes.
    return -1;
  }

  uint64_t data_blob_id = 0;
  while (true) {
    auto delta = start_ctrl_block - cb->get_ro()->start;

    if (delta == 0) {
      data_blob_id = cb->get_ro()->find(offset, cb->size());
      if (data_blob_id == 0) {
        data_blob_id =  get_next_free_id();
        cb->append_record(data_blob_id);
      }
      break;

    } else if (delta < 0) {
      if (!cb->prev()) {
        // Strange error! (until we support sparse files)
        return -1;
      }
    } else {
      if (!cb->next()) {
        // New control block for this data range.
        auto next_start = cb->get_ro()->start + 1;
        auto directory = cb->get_ro()->directory;

        cb = ChainBlock(cb);
        cb->update_header([&next_start, &directory](const ControlBlock* hdr) {
          ControlBlock new_header = *hdr;
          new_header.start = next_start;
          new_header.directory = directory;
          return new_header;
        });
      }
    }
  };

  auto blob = GetBlobStore()->GetBlob(data_blob_id);
  auto data = blob->Get();
  if (data.size() < (offset + count)) {
    data.resize(offset + count);
  }
  memcpy(&data[offset], buffer, count);
  if (blob->Put(data) != 0) {
    // Strange error as well.
    return -2;
  }

  stream->position += count;
  return count;
}

long ftell(FILE* stream) {
  return -1;
}
 
long fseek(FILE* stream, long offset, int origin) {
  return -1;
}

long fremove(const char* filename) {
  return -1;
}

}  // namespace g


