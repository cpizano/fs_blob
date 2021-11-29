// filesys.cc

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
