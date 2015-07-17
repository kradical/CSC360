#include <errno.h>
#include <fcntl.h> 
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>  
#include <sys/types.h> 
#include <sys/stat.h> 
#include <time.h> 
#include <unistd.h> 
 

struct superblock{
    long blocksize;
    long blockcount;
    long FATstart;
    long FATblocks;
    long rootstart;
    long rootblocks;
};

struct superblock SB;

void printFileInfo(char* disk_name){
    int f_d = 0;
    struct stat st;

    f_d = open(disk_name, O_RDONLY);
    if(f_d == -1){
        fprintf(stderr, "Can't open disk file.\n");
        exit(1);
    }
    errno = 0;
    if(fstat(f_d, &st)){
        perror("fstat error: ");
        close(f_d);
        exit(1);
    }

    printf("File size:                %lld bytes\n", (long long)st.st_size);
    printf("Last status change:       %s", ctime(&st.st_ctime));
    printf("Last file access:         %s", ctime(&st.st_atime));
    printf("Last file modification:   %s", ctime(&st.st_mtime));
    close(f_d);
}

void printDiskInfo(void){
    printf("Super block information:\n");
    printf("Block size: %ld\n", SB.blocksize);
    printf("Block count: %ld\n", SB.blockcount);
    printf("FAT starts: %ld\n", SB.FATstart);
    printf("FAT blocks: %ld\n", SB.FATblocks);
    printf("Root directory start: %ld\n", SB.rootstart);
    printf("Root directory blocks: %ld\n\n", SB.rootblocks);
    printf("FAT information:\n");
    printf("Free Blocks: 0\n");
    printf("Reserved Blocks: 0\n");
    printf("Allocated Blocks: 0\n");
}

void readSuperBlock(FILE *fp){
    unsigned char superblock[512];
    fread(&superblock, 512, 1, fp);
    int i;
    for(i=0; i<512; i++){
        printf("%.2X ", superblock[i]);
    }
    SB.blocksize = superblock[8]*pow(16, 2)+superblock[9];
    SB.blockcount = (long)superblock[10]*pow(16, 8)+superblock[11]*pow(16, 4)+superblock[12]*pow(16, 2)+superblock[13];
    SB.FATstart = (long)superblock[14]*pow(16, 8)+superblock[15]*pow(16, 4)+superblock[16]*pow(16, 2)+superblock[17];
    SB.FATblocks = 0;
    SB.rootstart = 0;
    SB.rootblocks = 0;
}

int main(int argc, char* argv[]){
    char* disk_name;
    if(argc > 1){
        disk_name = argv[1];
    }else{
        fprintf(stderr, "Must specify a file.");
        exit(1);
    }
    FILE *fp = fopen(disk_name, "rb");
    if(fp == NULL){
        fprintf(stderr, "Can't open file.\n");
        exit(1);
    }
    readSuperBlock(fp);
    #if defined(PART1)
        if(argc == 2) printDiskInfo();
        else fprintf(stderr, "USAGE: ./diskinfo [disk img]\n");
    #elif defined(PART2)
    printf("Part 2: disklist\n");
    #elif defined(PART3)
    printf("Part 3: diskget\n");
    #elif defined(PART4)
    printf("Part 4: diskput\n");
    #endif

    return 0;
}