#include <errno.h>
#include <fcntl.h> 
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>  
#include <sys/mman.h>
#include <sys/types.h> 
#include <sys/stat.h> 
#include <time.h> 
#include <unistd.h> 
 

struct superblock_t{
    uint16_t block_size;
    uint32_t block_count;
    uint32_t FATstart;
    uint32_t FATblocks;
    uint32_t rootstart;
    uint32_t root_block_count;
};

struct FAT_t{
    uint32_t reserved;
    uint32_t available;
    uint32_t allocated;
};

struct superblock_t SB;
struct FAT_t FB;

uint32_t fourbfield(char *fp, int ndx){
    return ((fp[ndx]&0xFF)<<24) + ((fp[ndx+1]&0xFF)<<16) + ((fp[ndx+2]&0xFF)<<8) + (fp[ndx+3]&0xFF);
}

uint16_t twobfield(char *fp, int ndx){
    return ((fp[ndx]&0xFF)<<8) + (fp[ndx+1]&0xFF);
}

uint8_t onebfield(char *fp, int ndx){
    return fp[ndx]&0xFF;
}

#if defined(PART2)
void printFileInfo(char* fp, int start, int end){
    int i;
    for(i=start;i<end;i+=64){
        if((fp[i] & 3) == 3){
            printf("F ");
        }else if((fp[i] & 5) == 5){
            printf("D ");
        }else{
            continue;
        }
    uint32_t filesize = fourbfield(fp, i+9);
    char filename[31];
    int x = 0;
    for(x=0; x<31; x++){
        filename[x] = fp[i+27+x];
    }
    uint16_t year = twobfield(fp, i+20);
    uint8_t month = onebfield(fp, i+22);
    uint8_t day = onebfield(fp, i+23);
    uint8_t hour = onebfield(fp, i+24);
    uint8_t minute = onebfield(fp, i+25);
    uint8_t second = onebfield(fp, i+26);
    printf("%10d %30s ", filesize, filename);
    printf("%4d/%02d/%02d %2d:%02d:%02d\n", year, month, day, hour, minute, second);
    }
}

void printDirInfo(char* dirname, char* fp){
    if(dirname[0] == '/'){
        dirname++;
        int i = 0;
        char tempbuf[255];
        while(dirname[0]  != '/' && dirname[0] != '\0'){
            tempbuf[i++] = dirname[0];
            dirname++;
        }
        tempbuf[i] = '\0';
        printf("%s\n", tempbuf);
        printf("%s\n\n", dirname);
        printDirInfo(dirname, fp);
    }else if(dirname[0] == '\0'){
        printf("END OF DIRNAME\n");
    }else{
        fprintf(stderr, "Specify directory of format: /subdir/subdir/subdir\n");
        exit(1);
    }
    // int end = (SB.rootstart+SB.root_block_count)*SB.block_size;
    // int start = SB.rootstart*SB.block_size;
    // printFileInfo(fp, start, end);
}
#endif

#if defined(PART1)
void printDiskInfo(void){
    printf("Super block information:\n");
    printf("Block size: %d\n", SB.block_size);
    printf("Block count: %d\n", SB.block_count);
    printf("FAT starts: %d\n", SB.FATstart);
    printf("FAT blocks: %d\n", SB.FATblocks);
    printf("Root directory start: %d\n", SB.rootstart);
    printf("Root directory blocks: %d\n\n", SB.root_block_count);
    printf("FAT information:\n");
    printf("Free Blocks: %d\n", FB.available);
    printf("Reserved Blocks: %d\n", FB.reserved);
    printf("Allocated Blocks: %d\n", FB.allocated);
}
#endif

void readFATinfo(char* fp){
    FB.reserved = 0;
    FB.available = 0;
    long FS = SB.block_size*SB.FATstart;
    int i;
    uint32_t temp;
    for(i=0; i<SB.FATblocks*SB.block_size; i+=4){
        temp = fourbfield(fp, FS+i);
        if(temp == 0){
            FB.available++;
        }else if(temp == 1){
            FB.reserved++;
        }
    }
    FB.allocated = SB.block_count - FB.reserved - FB.available;
}

void readSuperBlock(char *fp){    
    SB.block_size = twobfield(fp, 8);
    SB.block_count = fourbfield(fp, 10);
    SB.FATstart = fourbfield(fp, 14);
    SB.FATblocks = fourbfield(fp, 18);
    SB.rootstart = fourbfield(fp, 22);
    SB.root_block_count = fourbfield(fp, 26);
    readFATinfo(fp);
}

int main(int argc, char* argv[]){
    char* disk_name;
    int fp;
    struct stat sf;
    char *p;
    if(argc > 1){
        disk_name = argv[1];
    }else{
        fprintf(stderr, "Must specify a disc image.");
        exit(1);
    }
    if((fp = open(disk_name, O_RDONLY)) >= 0){
        fstat(fp, &sf);
        p = mmap(NULL, sf.st_size, PROT_READ, MAP_SHARED, fp, 0);
    }else{
        fprintf(stderr, "Can't open file.\n");
        close(fp);
        exit(1);
    }
    readSuperBlock(p);
    #if defined(PART1)
        if(argc == 2) printDiskInfo();
        else fprintf(stderr, "USAGE: ./diskinfo [disk img]\n");
    #elif defined(PART2)
        if(argc == 3) printDirInfo(argv[2], p);
        else fprintf(stderr, "USAGE: ./disklist [disk img] [directory]\n");
    #elif defined(PART3)
        // int i;
        // for(i=0;i<512*6400;i+=1){
        //     if((unsigned long)p[i] > 255){
        //         printf("%d %.2X\n", i, p[i]);
        //     }
        // }
    #elif defined(PART4)
    printf("Part 4: diskput\n");
    #endif

    return 0;
}