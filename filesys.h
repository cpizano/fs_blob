// filesys.h

namespace g {

struct FILE;
 
// opens or creates a file, returns NULL if error
// modes:
// r = read: file must exist to succeed.
// w = write: truncates if the file exists, otherwise creates it.
// a = append: appends if file exists, otherwise creates it.
FILE* fopen(const char* filename, const char* mode);
 
// deletes the file, returns negative if error.
long fremove(const char* filename);
 
// closes the file, returns 0 on success, negative if error.
long fclose(FILE* stream);
 
// reads the contents of the file, and moves the cursor.
// returns bytes read, or negative if error.
long fread(FILE* stream, void *buffer, long count);
 
// write contents to file, and moves the cursor.
// returns bytes written or negative if error.
long fwrite(FILE* stream, const void* buffer, long count);
 
// reads cursor position, returns negative if error.
long ftell(FILE* stream);
 
// moves cursor position, returns negative if error.
// origin: 0 = from start, 1 = from end, 2 = from current position.
long fseek(FILE* stream, long offset, int origin);

}  // namespace g
