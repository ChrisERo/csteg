# CSTEG

## Summary
CSTEG is a C implementation of the Jsteg algorithm, as described [here](https://pdfs.semanticscholar.org/8893/ba76f2e358e80ef5bd93e42b9c454cfb7770.pdf). In summary, it allows users to write and read hidden messages in a JPG image file using the leas significant bits of the color AC coeficients. The program assumes that the JPG being used is YCrCb and that the samplings of the Cr and Cb components are each 1x1. Although the image is altered, these changes should not be too vissible to an observer.

## Compilation
The file for executing this program should be in cpeg.bin, which is created by the Makefile. Here are the valid commands for using the Makefile:
- ```make clean``` Deletes the binary files associated with CSTEG
- ```make debug``` Compiles csteg.bin with its dependencies, but includes additional print statements for debugging and other debugging information, which can be used by a debugger like gdb.
- ```make``` Compiles production-ready version of csteg.bin

## Using CSTEG
CSTEG allows users to both write messages into and extract them from jpg images. csteg.bin is execbuted with at least 2, and at most 3 arguments. The first one specifies whether to write or extract a message. Use ```-w``` to write a message into an image and ```-r``` to read a message. The second argument simply specifies the image file to work with. 

The optional third argument must specify a text (.txt) file. When writing, this argument provides a message to write. If this argument is not provided, the prgram prompts the user to type a message. When reading, this argument specifies the name of the file in which to write the hidden message into. If this argument is not provided, the text is written to a file named ```extracted_messages.txt```.

As an example, the command ```./csteg.bin -w img.jpg mssg.txt``` writes the message stored in mssg.txt into the data of img.jpg. The command ```./csteg.bin -r img.jpg mssg2.txt``` extracts a hidden message from img.jpg and writes it into a new file named mssg2.txt.

## Important Notes
If one is reading a file into some text file, it is assumed that the directories of the file path (though not the actual file) already exist.
