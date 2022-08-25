# Make for UZI-180 and other useful programs for HiTech C v3.09 compiler

Eight-bit processors had rather low performance, and therefore a dispute arose among programmers about the optimality of writing programs. Some believe that programs should be written only in assembly language, which makes programs smaller and faster. Others are not so categorical and try (and not without success) to use high-level languages ​​for this purpose.

A good example of the ability to write fairly complex C programs for the Z80 processor is the HiTech C v3.09 compiler for CP/M. All programs of this compiler are written in the C language. The source code of the programs is restored from binary executable files and made available to the public in my repositories. Thanks to the recreated source code, it became possible to compile a cross compiler for modern operating systems. It is theoretically possible to create a compiler that runs directly on a unix-like UZI-180 operating system. Moreover, with the help of the HiTech C v3.09 compiler, the kernel and utilities of this operating system were written.

HiTech C v3.09 support of the ANSI standard allows you to compile programs created with other compilers and use them for your own purposes.

In this repository, I will post useful programs that compile successfully using the HiTech C v3.09 compiler.

# make

Hector Peraza adapted the make v2 program written for the minix operating system to run on the UZI-180 operating system. Thanks to this, writing and compiling programs in the UZI-180 environment has become more comfortable and familiar to a large number of programmers. The make program compiles only the most recently modified files, which reduces program development time.

The source code for make v2 is located in the /make directory. To create an executable file for the UZI-180 operating system, you must use the command

        zc3 -crmake.txt -mmake.map -o make.c archive.c archive1.c check.c docmd.c input.c macro.c main.c make1.c reader.c rules.c

In addition to creating the make executable file, a cross-reference file - make.txt and a program memory allocation file - make.map will be additionally created

# C LSI Compiler Preprocessor

The /cpp directory contains the source code for another preprocessor. Although this program does not have a free license, it is a good example of the capabilities of the HiTech C v3.09 compiler. The author of the program, Mori Koichiro, posted its source code along with a free version of his LSI compiler.

# ar archiver

HI-TECH SOFTWARE made the source code of some programs freely available as examples. One of them is the ar archiver. This archiver is not widely used and I posted it in the /ar directory as an example of the possibility of writing such programs using the HiTech C v3.09 compiler. In the same directory, there are several archives with programs created using this archiver.

# unix2cpm

A small but useful program that converts text files with source code from a unix operating system to CP/M is available in the /unix2cpm directory.
In the same directory is the source code for the U2D.C program that converts the Unix style end-of-line sequence 0AH to MS-DOS style end-of-line sequence 0DH 0AH.

The given examples of programs show that using a high-level compiler it is possible to write many useful and rather complex programs. The high-level language is more versatile and allows you to compile programs for different processors and operating systems. Using a high-level language reduces the time it takes to write programs and makes them easier to debug.

 Nikitin Andrey
 24-08-2022
 
