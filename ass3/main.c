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

struct FAT{
    long reserved;
    long available;
    long allocated;
};

struct superblock SB;
struct FAT FB;

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
    printf("Free Blocks: %ld\n", FB.available);
    printf("Reserved Blocks: %ld\n", FB.reserved);
    printf("Allocated Blocks: %ld\n", FB.allocated);
}

void readFATinfo(int fp){
    FB.reserved = 0;
    FB.available = 0;
    unsigned char FATblockbuf[4];
    int i;
    for(i=0; i<SB.FATblocks*SB.blocksize/4; i++){
        fread(&FATblockbuf, 4, 1, fp);
        if(FATblockbuf[0]+FATblockbuf[1]+FATblockbuf[2] == 0){
            if(FATblockbuf[3] == 1){
                FB.reserved++;
            }else if(FATblockbuf[3] == 0){
                FB.available++;
            }
        }
    }
    FB.allocated = SB.blockcount - FB.reserved - FB.available;
}

void readSuperBlock(int fp){
    unsigned char superblock[512];
    memcpy(superblock, (int*)fp, 512);
    SB.blocksize = (superblock[8]<<8)+superblock[9];
    SB.blockcount = (long)superblock[10]*pow(16, 8)+superblock[11]*pow(16, 4)+superblock[12]*pow(16, 2)+superblock[13];
    SB.FATstart = (long)superblock[14]*pow(16, 8)+superblock[15]*pow(16, 4)+superblock[16]*pow(16, 2)+superblock[17];
    SB.FATblocks = (long)superblock[18]*pow(16, 8)+superblock[19]*pow(16, 4)+superblock[20]*pow(16, 2)+superblock[21];
    SB.rootstart = (long)superblock[22]*pow(16, 8)+superblock[23]*pow(16, 4)+superblock[24]*pow(16, 2)+superblock[25];
    SB.rootblocks = (long)superblock[26]*pow(16, 8)+superblock[27]*pow(16, 4)+superblock[28]*pow(16, 2)+superblock[29];
    
    readFATinfo(fp);
}

int main(int argc, char* argv[]){
    char* disk_name;
    if(argc > 1){
        disk_name = argv[1];
    }else{
        fprintf(stderr, "Must specify a file.");
        exit(1);
    }
    int fp = open(disk_name, O_RDONLY);
    if(fp == 0){
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