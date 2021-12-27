// cpu@google.com  oct 2021.
// This is the PetaStore problem.
//
// Given a Blob API that allows you to read and write to storage
// specified in blob.h, write a filesystem library which satisfies
// the C API specified in  filesys.h.
//
// Filesystem specs (L6 and L7 candidates):
// Max file size: 2^40 bytes
// Max number of files: 2^33
// Max filename: 512 characters (printable ascii only)
// Blob free space: 2^52 bytes (fixed)
//
// Assume the client program (seen below) is single threaded and
// its only file IO is via the filesys.h API.

#include <string>
#include "filesys.h"

int main() {
  g::finitialize();

  g::FILE* file_1 = g::fopen("abcdef.txt", "rw");
  g::fwrite(file_1, "hello disk!", 11);
  g::fclose(file_1);

  g::ffinalize();
  return 0;
}

