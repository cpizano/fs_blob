// Interview problem: Given a Blob API that allows you to read and write
// to storage, specified in blob.h, write a filesystem which satisfies
// the C API specified in  filesys.h.

#include <string>
#include "filesys.h"

int main() {

  g::FILE* file = g::fopen("abcdef.txt", "rw");


  return 0;
}

