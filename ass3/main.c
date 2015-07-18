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

void readFATinfo(char* fp){
    FB.reserved = 0;
    FB.available = 0;
    long FS = SB.block_size*(SB.FATstart-1);
    int i;
    for(i=0; i<SB.FATblocks*SB.block_size;){
        if(fp[FS+i]+fp[FS+i+1]+fp[FS+i+2] == 0){
            if(fp[FS+i+3] == 1){
                FB.reserved++;
            }else if(fp[FS+i+3] == 0){
                FB.available++;
            }
        }
        i += 4;
    }
    FB.allocated = SB.block_count - FB.reserved - FB.available;
}

void readSuperBlock(char *fp){
    
    SB.block_size = (fp[8]<<8)+fp[9];
    SB.block_count = (fp[10]<<24)+(fp[11]<<16)+(fp[12]<<8)+fp[13];
    SB.FATstart = (fp[14]<<24)+(fp[15]<<16)+(fp[16]<<8)+fp[17];
    SB.FATblocks = (fp[18]<<24)+(fp[19]<<16)+(fp[20]<<8)+fp[21];
    SB.rootstart = (fp[22]<<24)+(fp[23]<<16)+(fp[24]<<8)+fp[25];
    SB.root_block_count = (fp[26]<<24)+(fp[27]<<16)+(fp[28]<<8)+fp[29];
    
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
        fprintf(stderr, "Must specify a file.");
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
    printf("Part 2: disklist\n");
    #elif defined(PART3)
    printf("Part 3: diskget\n");
    #elif defined(PART4)
    printf("Part 4: diskput\n");
    #endif

    return 0;
}