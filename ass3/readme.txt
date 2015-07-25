CSC360 a3
Konrad Schultz
V00761540

Everything is according to p3 spec.
Also handles inserting into a full directory 
or subdirectory creation into a full directory.
It will extend the directory by one block and fill 
in the first entry! yay!

There is very minimal commenting, I will add a bit 
before submitting (sorry). Also the flow of part 4
isn't split into functions and is unreadable (sorry).

In order to run:
$ make
$ ./diskinfo [disk img]
$ ./disklist [disk img] [directory]
$ ./diskget [disk img] [file in disk] [local copy name]
$ ./diskput [disk img] [local filename] [disk directory]

Thanks!