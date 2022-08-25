/*
 * ARCHIVE with Compress
 */

#ifndef uchar
#define uchar unsigned char
#endif


#include <stdio.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

int    main(int, char **);					// 1
void   list(char *, char);					// 2
void   delete(int, char **);					// 3
void   replace(int, char **);					// 4
void   extent(int, char **);					// 5
long   get_name(FILE *, char *, time_t *);			// 6
void   skip(FILE *, long);					// 7
long   get_size(char *);					// 8
void   replace_main(FILE *, FILE *, char *, time_t, long);	// 9
void   copy(char *);						// 10
void   extent_main(FILE *, char *, long);			// 11
void   putender(FILE *);					// 12
int    cpress(char *);						// 13
char * strlwr(char *);					// 14
char * strupr(char *);					// 15

int    unlink(char *name);
/**************************************************************************
 1	main
 **************************************************************************/
main(int argc, char **argv) {

    if(argc < 3) {
	puts("Archive : ar command archive_file [[file_name]...]");
	exit(0);
    }
    switch(argv[1][0]) {
	case 'm' :
	case 'M' :
	    list(argv[2], 'm');
	    break;

	case 's' :
	case 'S' :
	    list(argv[2], 's');
	    break;

	case 'd' :
	case 'D' :
	    delete(argc, argv);
	    break;

	case 'r' :
	case 'R' :
	    replace(argc, argv);
	    break;

	case 'x' :
	case 'X' :
	    extent(argc, argv);
	    break;

	default :
	    puts("Command : M(list) S(long list) D(delete) R(replace) X(extend)");
	    exit(0);
    }
}

/**************************************************************************
 2	list
 **************************************************************************/
void list(char *name, char mode) {
    long   size,
           get_name();
    char   file_name[14],
           arch_name[14];
    FILE  *fp,
          *fopen();
    time_t date_time;

    strcpy(arch_name, name);
    strupr(arch_name);
    strcat(arch_name, ".AR");
    if(!(fp = fopen(arch_name, "rb"))) {
	puts("No Archive File");
	exit(0);
    }
    while((size = get_name(fp, file_name, &date_time)) > 0l) {
	skip(fp, size);
	if(mode == 'm')
	    printf("%20s\n", file_name);
	else
	    printf("%16s%10ld%30s", file_name, size, ctime(&date_time));
    }
    fclose(fp);
}

/**************************************************************************
 3	delete
 **************************************************************************/
void delete(int argc, char **argv) {
    int	    i;
    long    size,
            get_name();
    FILE   *fp_arch,
           *fp_temp,
           *fopen();
    time_t  date_time;
    char    flag,
            arch_name[14],
            file_name[14],
            name[40][14];

    argc -= 3;
    for(i = 0; i < argc; i++) {
	strcpy(name[i], argv[i + 3]);
	strlwr(name[i]);
    }
    strcpy(arch_name, argv[2]);
    strupr(arch_name);
    strcat(arch_name, ".AR");
    if(!(fp_arch = fopen(arch_name, "rb"))) {
	puts("No Archive File");
	exit(0);
    }
    fp_temp = fopen("AR.$$$", "wb");
    while((size = get_name(fp_arch, file_name, &date_time)) > 0l) {
	flag = 'N';
	for(i = 0; i < argc; i++)
	    if(!strcmp(file_name, name[i])) {
		skip(fp_arch, size);
		strcpy(name[i], "");
		flag = 'Y';
	    }
	    if(flag == 'N') {
		replace_main(fp_arch, fp_temp, file_name, date_time, size);
	    }
    }
    fclose(fp_arch);
    fclose(fp_temp);
    copy(arch_name);
}

/**************************************************************************
 4	replace
 **************************************************************************/
void replace(int argc, char **argv) {
    int	i;
    long size,
    get_name(),
    get_size();    FILE *fp_arch,
    *fp_file,
    *fp_temp,
    *fopen();
    time_t  date_time; 
    char flag,
    arch_name[14],
    file_name[14],
    name[40][14];

    argc -= 3;
    for(i = 0; i < argc; i++) {
	strcpy(name[i], argv[i + 3]);
	strlwr(name[i]);
    }
    strcpy(arch_name, argv[2]);
    strupr(arch_name);
    strcat(arch_name, ".AR");
    if(!(fp_arch = fopen(arch_name, "rb"))) {
	fp_arch = fopen(arch_name, "wb");
	putender(fp_arch);
	fclose(fp_arch);
	fp_arch = fopen(arch_name, "rb");
    }
    fp_temp = fopen("AR.$$$", "wb");
    while((size = get_name(fp_arch, file_name, &date_time)) > 0l) {
	flag = 'N';
	for(i = 0; i < argc; i++)
	    if(!strcmp(file_name, name[i])) {
		skip(fp_arch, size);
		strcpy(name[i], "");
		if((size = get_size(file_name)) > 0l) {
		    fp_file = fopen("AR1.$$$", "rb");
		    time(&date_time);
		    replace_main(fp_file, fp_temp, file_name, date_time, size);
		    fclose(fp_file);
		}
	        flag = 'Y';
	    }
	if(flag == 'N')
	    replace_main(fp_arch, fp_temp, file_name, date_time, size);
    }
    for(i = 0; i < argc; i++)
	if(strlen(name[i]))
	    if((size = get_size(name[i])) > 0l) {
		fp_file = fopen("AR1.$$$", "rb");
		time(&date_time);
		replace_main(fp_file, fp_temp, name[i], date_time, size);
		fclose(fp_file);
	    }
	    fclose(fp_arch);
	    fclose(fp_temp);
	    copy(arch_name);
}

/**************************************************************************
 5	extent
 **************************************************************************/
void extent(int argc, char **argv) {
    int	   i;
    long   size,
           get_name();
    FILE  *fp_arch,
          *fopen();
    time_t date_time;
    char   flag,
           all,
           arch_name[14],
           file_name[14],
           name[40][14];

    if(argc == 3)
	all = 'Y';
    else {
	all = 'N';
	argc -= 3;
	for(i = 0; i < argc; i++) {
	    strcpy(name[i], argv[i + 3]);
	    strlwr(name[i]);
	}
    }
    strcpy(arch_name, argv[2]);
    strupr(arch_name);
    strcat(arch_name, ".AR");
    if(!(fp_arch = fopen(arch_name, "rb"))) {
	puts("No Archive File");
	exit(0);
    }
    while((size = get_name(fp_arch, file_name, &date_time)) > 0l) {
	if(all == 'Y')
	    extent_main(fp_arch, file_name, size);
	else {
	    flag = 'N';
	    for(i = 0; i < argc; i++)
		if(!strcmp(file_name, name[i])) {
		    extent_main(fp_arch, file_name, size);
		    flag = 'Y';
		}
		if(flag == 'N')
		skip(fp_arch, size);
	    }
    }
}

/**************************************************************************
 6	get_name
 **************************************************************************/
long get_name(FILE *fp, char *name, time_t *date_time) {
    int	i;
    union {
	char c[4];
	long l;
    } u;

    for(i = 0; i < 14; i++) {
	name[i] = fgetc(fp);
    }
    for(i = 0; i < 4; i++) {
	u.c[i] = fgetc(fp);
    }
    *date_time = u.l;
    for(i = 0; i < 4; i++) {
	u.c[i] = fgetc(fp);
    }
    return u.l;
}

/**************************************************************************
 7	skip
 **************************************************************************/
void skip(FILE *fp, long size) {
    long l;

    for(l = 0l; l < size; l++)
	fgetc(fp);
}

/**************************************************************************
 8	get_size
 **************************************************************************/
long get_size(char *name) {
    long  l;
    FILE *fp,
         *fopen();

    if(cpress(name)) {
	fp = fopen("AR1.$$$", "rb");
	for(l = 0l; fgetc(fp) != EOF; l++) ;
	fclose(fp);
	return l;
    }
    return -1l;
}

/**************************************************************************
 9	replace_main
 **************************************************************************/
void replace_main(FILE *fp_in, FILE *fp_out, char *name, time_t  date_time, long size) {
    int  i;
    long l;
    union {
	char c[4];
	long l;
    } u;

    for(i = 0; i < 14; i++)
	fputc(name[i], fp_out);
    u.l = date_time;
    for(i = 0; i < 4; i++)
	fputc(u.c[i], fp_out);
    u.l = size;
    for(i = 0; i < 4; i++)
	fputc(u.c[i], fp_out);
    for(l = 0l; l < size; l++) {
	i = fgetc(fp_in);
	fputc(i, fp_out);
    }
}

/**************************************************************************
 10	copy
 **************************************************************************/
void copy(char *name) {
    int   c;
    FILE *fp_in,
         *fp_out,
         *fopen();

    fp_in  = fopen("AR.$$$", "rb");
    fp_out = fopen(name, "wb");
    while((c = fgetc(fp_in)) != EOF)
	fputc(c, fp_out);
    putender(fp_out);
    fclose(fp_in);
    fclose(fp_out);
    unlink("AR.$$$");
    unlink("AR1.$$$");
}

/**************************************************************************
 11	extent_main
 **************************************************************************/
void extent_main(FILE *fp, char *name, long size) {
    int   c,
          count;
    long  l;
    FILE *fp_out,
         *fopen();

    fp_out = fopen(name, "wb");
    for(l = 0l; l < size; l++)
	if((c = fgetc(fp)) == 0xff) {
	    count = fgetc(fp);
	    c = fgetc(fp);
	    l += 2l;
	    while(count--)
		fputc(c, fp_out);
	}
	else
	    fputc(c, fp_out);
	fclose(fp_out);
}

/**************************************************************************
 12	putender
 **************************************************************************/
void putender(FILE *fp) {
    int i;
    union {
	char c[4];
	long l;
    } u;

    for(i = 0; i< 14; i++)
	fputc(' ', fp);
    u.l = -1;
    for(i = 0; i < 4; i++)
	fputc(u.c[i], fp);
    for(i = 0; i < 4; i++)
	fputc(u.c[i], fp);
}

/**************************************************************************
 13	cpress
 **************************************************************************/
int cpress(char *name) {
    int   c1,
          c2,
          count = 1;
    FILE *fp_in,
         *fp_out,
         *fopen();

    if(fp_in = fopen(name, "rb")) {
	fp_out = fopen("AR1.$$$", "wb");
	c1 = fgetc(fp_in);
	while((c2 = fgetc(fp_in)) != EOF)
	    if(c1 != c2) {
		if(count > 3) {
		    fputc(0xff, fp_out);
		    fputc(count & 0xff, fp_out);
		    fputc(c1, fp_out);
		}
	    else
		while(count--)
		    fputc(c1, fp_out);
	count = 1;
	c1 = c2;
    }
    else
	count++;
    if(count > 3) {
	fputc(0xff, fp_out);
	fputc(count & 0xff, fp_out);
	fputc(c1, fp_out);
    }
    else
	while(count--)
	    fputc(c1, fp_out);
	fclose(fp_in);
	fclose(fp_out);
	return -1;
    }
    return 0;
}

/**************************************************************************
 14	strlwr
 **************************************************************************/
char *strlwr(char *buf) {
    uchar *buff,
           before;

    buff   = (uchar *)buf;
    before = 0x00;

    do {
        if(before < 0x80) {
            before = *buff;
            if(*buff > 0x40 && *buff < 0x5b)
                *buff += 0x20;
        }
        else {
            if(before == 0x82) {
                if(*buff > 0x5f && *buff < 0x7a)
                    *buff += 0x21;
            }
            before = 0x00;
        }
    } while(*(buff++));

    return buf;
}

/**************************************************************************
 15	strupr
 **************************************************************************/
char *strupr(char *buf) {
    uchar *buff,
           before;

    buff   = (uchar *)buf;
    before = 0x00;

    do {
        if(before < 0x80) {
            before = *buff;
            if(*buff > 0x60 && *buff < 0x7b)
                *buff -= 0x20;
        }
        else {
            if(before == 0x82) {
                if(*buff > 0x80 && *buff < 0x9b)
                    *buff -= 0x21;
            }
            before = 0x00;
        }
    } while(*(buff++));

    return buf;
}


