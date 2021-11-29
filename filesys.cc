// filesys.cc
//
// From the requirements in main.cc it is clear that we can't simply load
// a map<name_of_file, blob_id>.
//
// A blob is 2^18 bytes, so given space is 2^50 to 2^54 the number of
// blobs is 2^32 to 2^36.

#include "blob.h"

namespace g {

struct FILE {
  size_t position;
};


FILE* fopen(const char* filename, const char* mode) {
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
