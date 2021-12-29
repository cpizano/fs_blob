# fs_blob  (aka the petabyte store question)

This repository documents an interview question I've used a few times. It goes like this:
"Given a blob API that allows you to read and write to storage, write a filesystem which satisfies a file IO C API"

The blob API is in `blob.h` and the API you need to implement is in `filesys.h` DON'T look at the `answer_1.cc` if you want
do this by yourself.

In the interview format (1 hour) the question is about designing the system, the data structures, not actually typing the
code that implements it. There is simply not enough time to write the code. In a 3+ hour format then you can expect the
candidate to implement fopen, fclose fread and fwrite.

So what are the other files?
* `blob_impl.cc` : a fake blob implementation to aid debugging, you might want to do your own flavor of this.
* `main.cc` : a very simple test driver, you probably want your own flavor of this.
* `answer_1.cc` : my basic solution to the question, with minimal ammount of code.

Normally I don't give the specifications of the filesystem to be created. Yes, the question is really about creating
a new filesystem (or if you have one memorized then I guess type that one :)) so I wait for the canidate to ask good
questions about it, like how many files it can store, how big each file, etc. Any candidate that starts designing or
implementing before the information below will inevitably design the wrong thing.

Here are the specs:
* Filesystem specs (L6 and L7 candidates):
* Max file size: 2^40 bytes
* Max number of files: 2^33
* Max filename: 512 characters (printable ascii only)
* Blob free space: 2^52 bytes (fixed)

Assume the client program is single threaded and its only file IO is via the filesys.h API. Your implementation
of the API can use threads but the client is strictly single threaded.

Don't assume the blob API has a single implementation, in fact given the size of the storage in a practical setting
it would be remote/distributed in some cases.

Note that there is still a lot that is underspecified. It is meant to spur interesting conversations with the
candidate and to let you tailor the problem to different skillsets. Importantly the Blob API is underspecified;
for example, it is not clear what operation is synchronous and which are asynchronous: say Put() is async but
Release() is not. You can start with all synchronous ops to beging with.
