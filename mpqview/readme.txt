Utility to list and extract files from .mpq archives

Only file "Mpqview.c" has been modified from original source to allow compilation under mingw32 gcc.

Most .mpq file use the file named (listfile) as an index to all the files contained in the archive.
To view actual content of archive.

1. Extract the file (listfile) from the mpq
2. Rename the file (listfile) to list.txt and place in same folder as mpqview.exe
3. List file from mpg using `mpqview l <archive.mpq>`
4. EXtract required file using `mpqview e <archive.mpq> <index_number>`
