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

#include <stdio.h>
#include <string.h>
#include "filesys.h"

#define TEST(c, v) { if (!(c)) { printf("failed (%d) at line %d.\n", (v), __LINE__); return -1; }}

int main() {
  g::finitialize();

  constexpr auto name = "abcdef.txt";
  char data_in[] = "hello disk!";

  auto file_1 = g::fopen(name, "rw");
  TEST(file_1 != nullptr, 0);

  long rc;

  rc = g::fwrite(file_1, data_in, sizeof(data_in));
  TEST(rc == sizeof(data_in), rc);

  auto pos = g::ftell(file_1);
  TEST(pos == sizeof(data_in), pos);

  rc = g::fclose(file_1);
  TEST(rc == 0, rc);

  auto file_2 = g::fopen(name, "rw");
  TEST(file_2 != nullptr, 0);

  char data_out[64];
  rc = g::fread(file_2, data_out, sizeof(data_out));
  TEST(rc == sizeof(data_in), rc);
  TEST(strcmp(data_out, data_in) == 0, 0);

  g::ffinalize();
  printf("succesful run\n");
  return 0;
}

