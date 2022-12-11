# Basic-EXT2-File-Search
This program was made for a project in my 'Operating Systems' class at UofL.

The 'fsa.c' is meant to be called with 2 or 3 command line arguments:

./fsa <EXT2_img_file> -traverse
    This would show all files and directories on the given EXT2 .img file.

./fsa <EXT2_img_file> -read <absolute_file_path>
    This would read through the EXT2 .img file to find the specified file
using its absolute file path and then print the contents of that file to the
console.
