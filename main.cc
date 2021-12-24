// cpu@google.com  oct 2021.
//
// Interview problem: Given a Blob API that allows you to read and write
// to storage, specified in blob.h, write a filesystem which satisfies
// the C API specified in  filesys.h.
// This API is the only means for the client app to do file I/O.
//
// Filesystem specs (L6 and L7 candidates):
// Max file size: 2^40 bytes
// Max number of files: 2^33
// Max filename: 512 characters (printable ascii only)
// Blob free space: 2^52 bytes (fixed)


#include <string>
#include "filesys.h"

int main() {
  g::finitialize();

  g::FILE* file = g::fopen("abcdef.txt", "rw");

  g::fclose(file);

  g::ffinalize();
  return 0;
}

