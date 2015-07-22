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

#define FILENAMELIM 31

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

struct datetime_t{
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t min;
    uint8_t sec;
};

struct superblock_t SB;
struct FAT_t FB;

void *try_malloc(unsigned long int size){
    void *p = malloc(size); 
    if(p == NULL){
        perror("Error allocating memory");
        exit(1);
    }
    return p;
}

uint32_t fourbfield(char *fp, int ndx){
    return ((fp[ndx]&0xFF)<<24) + ((fp[ndx+1]&0xFF)<<16) + ((fp[ndx+2]&0xFF)<<8) + (fp[ndx+3]&0xFF);
}

uint16_t twobfield(char *fp, int ndx){
    return ((fp[ndx]&0xFF)<<8) + (fp[ndx+1]&0xFF);
}

uint8_t onebfield(char *fp, int ndx){
    return fp[ndx]&0xFF;
}

#if defined(PART4)
void getCurrentTime(struct datetime_t *timeb){
    struct tm *UTCtime;
    time_t now = time(0);
    UTCtime = gmtime(&now);
    timeb->sec = (UTCtime->tm_sec);
    timeb->min = (UTCtime->tm_min);
    timeb->hour = ((UTCtime->tm_hour+17)%24);
    timeb->day = (UTCtime->tm_mday);
    timeb->month = (UTCtime->tm_mon+1);
    timeb->year = ((UTCtime->tm_year+1900));
}

void writeFileContents(char *disk, char *inputfile, uint32_t filesize){
    uint32_t numblocks = filesize/SB.block_size + 1;
    uint32_t i;
    uint32_t j = SB.FATstart*SB.block_size;
    uint32_t FATaddresses[numblocks-1];
    for(i=0;i<numblocks-1;i++){
        while(fourbfield(disk, j) != 0){
            j += 4;
        }
        FATaddresses[i] = j;
        uint32_t writeblock = (j-SB.FATstart*SB.block_size)/4*SB.block_size;
        printf("write block to %X\n", writeblock);
        memcpy(disk+writeblock, inputfile+i*SB.block_size, SB.block_size);
        j += 4;
    }
}

void putFile(char *ifile, char *olocation, char *fp){
    int ifp;
    struct stat sf;
    char *p;
    if((ifp = open(ifile, O_RDONLY)) >= 0){
        fstat(ifp, &sf);
        p = mmap(NULL, sf.st_size, PROT_READ, MAP_SHARED, ifp, 0);
    }else{
        fprintf(stderr, "Can't open file.\n");
        close(ifp);
        exit(1);
    }
    writeFileContents(fp, p, (uint32_t)sf.st_size);
    struct datetime_t *timeb = try_malloc(sizeof(struct datetime_t));
    getCurrentTime(timeb);
    printf("\nStatus %X\n", 0x3);
    printf("Starting Block \n");
    printf("Number of Blocks %d\n", (int)sf.st_size/SB.block_size+1);
    printf("File size %d\n", (int)sf.st_size);
    printf("Create/modify time %4d/%02d/%02d %2d:%02d:%02d\n", timeb->year, timeb->month, timeb->day, timeb->hour, timeb->min, timeb->sec);
    printf("Filename %s\n", ifile);
}
#endif

#if defined(PART3)
void transferFile(char *fp, char *filename, uint32_t filesizeblk, uint32_t filesize, uint32_t startblk){
    FILE *new;
    if((new = fopen(filename, "wb")) < 0){
        fprintf(stderr, "Can't open disk file.\n");
        fclose(new);
        exit(1);
    }
    int i;
    uint32_t nextblock = startblk;
    for(i=0; i<filesizeblk-1; i++){
        if(nextblock == 0xFFFFFFFF){
            fprintf(stderr, "End of file detected corrupt file.");
            exit(1);
        }
        fwrite(fp+nextblock*SB.block_size, SB.block_size, 1, new);
        nextblock = fourbfield(fp, SB.FATstart*SB.block_size + 4*(nextblock));
    }
    uint32_t remainingbytes = filesize%SB.block_size==0 ? SB.block_size : filesize%SB.block_size;
    fwrite(fp+nextblock*SB.block_size, remainingbytes, 1, new);
}

int fileNameMatch(char *fp, char *subdirname, int ndx){
    if((fp[ndx] & 3) != 3) return 0;
    return !strcmp(subdirname, fp+27+ndx);
}

void getFile(char* dirname, char* fp, int start, int numblocks, char *filename){
    if(dirname[0] == '/'){
        dirname++;
        if(dirname[0] == '\0'){
            fprintf(stderr, "Input format: /subdir/subdir/filename\n");
            return;
        }
        int i = 0;
        char tempbuf[FILENAMELIM];
        while(dirname[0]  != '/' && dirname[0] != '\0'){
            tempbuf[i++] = dirname[0];
            dirname++;
        }
        tempbuf[i] = '\0';
        uint32_t nextblock = start;
        for(i=0; i<numblocks*SB.block_size; i+=64){
            if(i%SB.block_size == 0 && i != 0){
                nextblock = fourbfield(fp, SB.FATstart*SB.block_size + 4*(nextblock));//removed a (nextblock-1) here, put it back if everything breaks
                if(nextblock == 0xFFFFFFFF){
                    fprintf(stderr, "File not found.\n");
                    return;
                }
            }
            if(fileNameMatch(fp, tempbuf, i+nextblock*SB.block_size)){
                uint32_t startblk = fourbfield(fp, i+nextblock*SB.block_size+1);
                uint32_t dirsizeblk = fourbfield(fp, i+nextblock*SB.block_size+5);
                uint32_t filesize = fourbfield(fp, i+nextblock*SB.block_size+9);
                if(dirname[0] == '\0'){
                    if((fp[i+nextblock*SB.block_size]&3)==3){
                        transferFile(fp, filename, dirsizeblk, filesize, startblk);
                        return;
                    }
                }else{
                    getFile(dirname, fp, startblk, dirsizeblk, filename);
                    return;
                }
            }
        }
    }else{
        fprintf(stderr, "Input format: /subdir/subdir/filename\n");
        exit(1);
    }
}
#endif

#if defined(PART2)
void printFileInfo(char* fp, int start, int numblocks){
    int i;
    uint32_t nextblock = start;
    for(i=0;i<numblocks*SB.block_size;i+=64){
        if(i%SB.block_size == 0 && i != 0){
            nextblock = fourbfield(fp, SB.FATstart*SB.block_size + 4*(nextblock-1));
            if(nextblock == 0xFFFFFFFF)return;
        }
        if((fp[i+nextblock*SB.block_size] & 3) == 3){
            printf("F ");
        }else if((fp[i+nextblock*SB.block_size] & 7) == 5){
            printf("D ");
        }else{
            continue;
        }
        uint32_t filesize = fourbfield(fp, i+nextblock*SB.block_size+9);
        char filename[FILENAMELIM];
        int x = 0;
        for(x=0; x<FILENAMELIM; x++){
            filename[x] = fp[i+27+nextblock*SB.block_size+x];
        }
        uint16_t year = twobfield(fp, i+nextblock*SB.block_size+20);
        uint8_t month = onebfield(fp, i+nextblock*SB.block_size+22);
        uint8_t day = onebfield(fp, i+nextblock*SB.block_size+23);
        uint8_t hour = onebfield(fp, i+nextblock*SB.block_size+24);
        uint8_t minute = onebfield(fp, i+nextblock*SB.block_size+25);
        uint8_t second = onebfield(fp, i+nextblock*SB.block_size+26);
        printf("%10d %30s ", filesize, filename);
        printf("%4d/%02d/%02d %2d:%02d:%02d\n", year, month, day, hour, minute, second);
    }
}

int dirNameMatch(char *fp, char *subdirname, int ndx){
    if((fp[ndx] & 7) != 5) return 0;
    return !strcmp(subdirname, fp+27+ndx);
}

void printDirInfo(char* dirname, char* fp, int start, int numblocks){
    if(dirname[0] == '/'){
        dirname++;
        if(dirname[0] == '\0'){
            printFileInfo(fp, start, numblocks);
            return;
        }
        int i = 0;
        char tempbuf[FILENAMELIM];
        while(dirname[0]  != '/' && dirname[0] != '\0'){
            tempbuf[i++] = dirname[0];
            dirname++;
        }
        tempbuf[i] = '\0';
        uint32_t nextblock = start;
        for(i=0; i<numblocks*SB.block_size; i+=64){
            if(i%SB.block_size == 0 && i != 0){
                nextblock = fourbfield(fp, SB.FATstart*SB.block_size + 4*(nextblock-1));
                if(nextblock == 0xFFFFFFFF){
                    fprintf(stderr, "Directory not found.\n");
                    return;
                }
            }
            if(dirNameMatch(fp, tempbuf, i+nextblock*SB.block_size)){
                uint32_t startb = fourbfield(fp, i+nextblock*SB.block_size+1);
                uint32_t dirsizeb = fourbfield(fp, i+nextblock*SB.block_size+5);
                if(dirname[0] == '\0'){
                    printFileInfo(fp, startb, dirsizeb);
                }else{
                    printDirInfo(dirname, fp, startb, dirsizeb);
                }
            }
        }
    }else{
        fprintf(stderr, "Input format: /subdir/subdir/subdir\n");
        exit(1);
    }
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
    if((fp = open(disk_name, O_RDWR)) >= 0){
        fstat(fp, &sf);
        p = mmap(NULL, sf.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fp, 0);
    }else{
        fprintf(stderr, "Can't open disk file.\n");
        close(fp);
        exit(1);
    }
    readSuperBlock(p);
    #if defined(PART1)
        if(argc == 2) printDiskInfo();
        else fprintf(stderr, "USAGE: ./diskinfo [disk img]\n");
    #elif defined(PART2)
        if(argc == 3) printDirInfo(argv[2], p, SB.rootstart, SB.root_block_count);
        else fprintf(stderr, "USAGE: ./disklist [disk img] [directory]\n");
    #elif defined(PART3)
        if(argc == 4) getFile(argv[2], p, SB.rootstart, SB.root_block_count, argv[3]);
        else fprintf(stderr, "USAGE: ./diskget [disk img] [file in disk] [local copy name]\n");
    #elif defined(PART4)
        if(argc == 4) putFile(argv[2], argv[3], p);
        else fprintf(stderr, "USAGE: ./diskput [disk img] [local filename] [disk directory]\n");
    #endif

    return 0;
}