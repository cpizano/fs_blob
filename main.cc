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

#define TEST(c, v) { if (!(c)) { printf("failed (%d) at line %d.\n", (v), __LINE__); return -1; }}

int main() {
  g::finitialize();

  constexpr auto name = "abcdef.txt";
  constexpr auto data = "hello disk!";

  auto file_1 = g::fopen(name, "rw");
  TEST(file_1 != nullptr, 0);

  long rc;

  rc = g::fwrite(file_1, data, sizeof(data));
  TEST(rc == sizeof(data), rc);

  auto pos = g::ftell(file_1);
  TEST(pos == sizeof(data), pos);

  rc = g::fclose(file_1);
  TEST(rc == 0, rc);

  auto file_2 = g::fopen(name, "rw");
  TEST(file_2 != nullptr, 0);

  g::ffinalize();
  printf("succesful run\n");
  return 0;
}

