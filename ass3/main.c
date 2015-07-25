#include <arpa/inet.h>
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

//helper function to check the results of malloc
void *try_malloc(unsigned long int size){
    void *p = malloc(size); 
    if(p == NULL){
        perror("Error allocating memory");
        exit(1);
    }
    return p;
}

//helper function to check if directory name matches while scanning through dirblocks
int dirNameMatch(char *fp, char *subdirname, int ndx){
    if((fp[ndx] & 7) != 5) return 0;
    return !strcmp(subdirname, fp+27+ndx);
}

//helper function to check if filename matches while scanning through dirblocks
int fileNameMatch(char *fp, char *subdirname, int ndx){
    if((fp[ndx] & 3) != 3) return 0;
    return !strcmp(subdirname, fp+27+ndx);
}

//helper function to convert four bytes from disk into a number
uint32_t fourbfield(char *fp, int ndx){
    return ((fp[ndx]&0xFF)<<24) + ((fp[ndx+1]&0xFF)<<16) + ((fp[ndx+2]&0xFF)<<8) + (fp[ndx+3]&0xFF);
}

//helper function to convert two bytes from disk into a number
uint16_t twobfield(char *fp, int ndx){
    return ((fp[ndx]&0xFF)<<8) + (fp[ndx+1]&0xFF);
}

uint8_t onebfield(char *fp, int ndx){
    return fp[ndx]&0xFF;
}

#if defined(PART4)
//helper function to set a struct to the current time
void getCurrentTime(struct datetime_t *timeb){
    struct tm *UTCtime;
    time_t now = time(0);
    UTCtime = gmtime(&now);
    timeb->sec = (UTCtime->tm_sec);
    timeb->min = (UTCtime->tm_min);
    timeb->hour = (UTCtime->tm_hour);
    timeb->day = (UTCtime->tm_mday);
    timeb->month = (UTCtime->tm_mon+1);
    timeb->year = ((UTCtime->tm_year+1900));
}

//writes the file contents and tracks what should be changed in FAT after everything is successfully written
uint32_t writeFileContents(char *disk, char *inputfile, uint32_t filesize, uint32_t *FATaddresses, uint32_t *FATvalues){
    uint32_t numfullblocks = filesize/SB.block_size;
    uint32_t i;
    uint32_t writeblock;
    uint32_t j = SB.FATstart*SB.block_size;
    for(i=0;i<numfullblocks;i++){
        while(fourbfield(disk, j) != 0){
            j += 4;
        }
        writeblock = (j-SB.FATstart*SB.block_size)/4*SB.block_size;
        FATaddresses[i] = j;
        FATvalues[i] = htonl(writeblock/SB.block_size);
        memcpy(disk+writeblock, inputfile+i*SB.block_size, SB.block_size);
        j += 4;
    }
    writeblock = (j-SB.FATstart*SB.block_size)/4*SB.block_size;
    memcpy(disk+writeblock, inputfile+numfullblocks*SB.block_size, filesize%SB.block_size);
    if(filesize%SB.block_size != 0){
        FATaddresses[i] = j;
        FATvalues[i++] = htonl(writeblock/SB.block_size);
        j += 4;
    }
    int k;
    for(k=0; k<i;k++){
        FATvalues[k] = FATvalues[k+1];
    }
    FATvalues[i-1] = htonl(-1);
    return j;
}

//updates directory entries for inserting a new file. creates subdirectories if they don't exist. Extends parent directories if they are full.
void writeDirInfo(char *fp, char *dirname, uint32_t prevnumblocks, uint32_t start, uint32_t numblocks, uint32_t newstartblock, struct stat *filedata, uint32_t uncheckedFAT){
    if(dirname[0] == '/'){//get current directory or filename
        dirname++;
        if(dirname[0] == '\0'){
            fprintf(stderr, "Input format: /subdir/subdir/filename\n");
            exit(1);
        }
        int i = 0;
        char tempbuf[FILENAMELIM];
        while(dirname[0]  != '/' && dirname[0] != '\0'){
            tempbuf[i++] = dirname[0];
            dirname++;
        }
        while(i<31){
            tempbuf[i++] = '\0';
        };
        if(dirname[0] == '\0'){//file
            uint32_t nextblock = start;
            for(i=0; i<numblocks*SB.block_size; i+=64){//scan to check if the file exists
                if(i%SB.block_size == 0 && i != 0){
                    nextblock = fourbfield(fp, SB.FATstart*SB.block_size + 4*(nextblock));
                    if(nextblock == 0xFFFFFFFF){
                        fprintf(stderr, "Corrupt directory.\n");
                        exit(1);
                    }
                }
                if(fileNameMatch(fp, tempbuf, i%SB.block_size+nextblock*SB.block_size)){
                    fprintf(stderr, "file already exists.");
                    exit(1);
                }
            }
            nextblock = start;
            for(i=0; i<numblocks*SB.block_size; i+=64){//find an open directory entry slot
                if(i%SB.block_size == 0 && i != 0){
                    nextblock = fourbfield(fp, SB.FATstart*SB.block_size + 4*(nextblock));
                    if(nextblock == 0xFFFFFFFF){
                        fprintf(stderr, "Corrupt directory.\n");
                        exit(1);
                    }
                }
                if(fourbfield(fp, i%SB.block_size+nextblock*SB.block_size) == 0){
                //create new directory entry
                    uint8_t status = 3;
                    uint32_t startingblock = htonl((newstartblock-SB.FATstart*SB.block_size)/4);
                    uint32_t numberofblocks;
                    if((int)filedata->st_size/SB.block_size == 0){
                        numberofblocks = htonl((int)filedata->st_size/SB.block_size);
                    }else{
                        numberofblocks = htonl((int)filedata->st_size/SB.block_size + 1);
                    }
                    uint32_t filesize = htonl((int)filedata->st_size);
                    struct datetime_t *timeb = try_malloc(sizeof(struct datetime_t));
                    getCurrentTime(timeb);
                    uint16_t year = htons(timeb->year);
                    memcpy(fp+i%SB.block_size+nextblock*SB.block_size, &status, 1);
                    memcpy(fp+i%SB.block_size+nextblock*SB.block_size+1, &startingblock, 4);
                    memcpy(fp+i%SB.block_size+nextblock*SB.block_size+5, &numberofblocks, 4);
                    memcpy(fp+i%SB.block_size+nextblock*SB.block_size+9, &filesize, 4);
                    memcpy(fp+i%SB.block_size+nextblock*SB.block_size+13, &year, 2);
                    memcpy(fp+i%SB.block_size+nextblock*SB.block_size+15, &(timeb->month), 1);
                    memcpy(fp+i%SB.block_size+nextblock*SB.block_size+16, &(timeb->day), 1);
                    memcpy(fp+i%SB.block_size+nextblock*SB.block_size+17, &(timeb->hour), 1);
                    memcpy(fp+i%SB.block_size+nextblock*SB.block_size+18, &(timeb->min), 1);
                    memcpy(fp+i%SB.block_size+nextblock*SB.block_size+19, &(timeb->sec), 1);
                    memcpy(fp+i%SB.block_size+nextblock*SB.block_size+20, &year, 2);
                    memcpy(fp+i%SB.block_size+nextblock*SB.block_size+22, &(timeb->month), 1);
                    memcpy(fp+i%SB.block_size+nextblock*SB.block_size+23, &(timeb->day), 1);
                    memcpy(fp+i%SB.block_size+nextblock*SB.block_size+24, &(timeb->hour), 1);
                    memcpy(fp+i%SB.block_size+nextblock*SB.block_size+25, &(timeb->min), 1);
                    memcpy(fp+i%SB.block_size+nextblock*SB.block_size+26, &(timeb->sec), 1);
                    memcpy(fp+i%SB.block_size+nextblock*SB.block_size+27, &tempbuf, 31);
                    return;
                }
            }
            //extend full directory if all are occupied
            while(fourbfield(fp, uncheckedFAT) != 0){
                uncheckedFAT += 4;
            }
            uint32_t temp = 0xFFFFFFFF;
            memcpy(fp+uncheckedFAT, &temp, 4);//new file ending
            temp = htonl((uncheckedFAT-SB.FATstart*SB.block_size)/4);
            memcpy(fp + (SB.FATstart*SB.block_size + nextblock*4), &temp, 4);//rewrite old file ending
            temp = htonl(fourbfield(fp, prevnumblocks)+1);
            memcpy(fp + prevnumblocks, &temp, 4);//extend blocksize
            temp = htonl(fourbfield(fp, prevnumblocks+4)+SB.block_size);
            memcpy(fp + prevnumblocks+4, &temp, 4);//extend filesize
            uint8_t status = 3;
            uint32_t startingblock = htonl((newstartblock-SB.FATstart*SB.block_size)/4);
            uint32_t numberofblocks;
            if((int)filedata->st_size%SB.block_size == 0){
                numberofblocks = htonl((int)filedata->st_size/SB.block_size);
            }else{
                numberofblocks = htonl((int)filedata->st_size/SB.block_size + 1);
            }
            uint32_t filesize = htonl((int)filedata->st_size);
            struct datetime_t *timeb = try_malloc(sizeof(struct datetime_t));
            getCurrentTime(timeb);
            uint16_t year = htons(timeb->year);
            //create directory entry in newly created block (extended parent directory)
            memset(fp+(uncheckedFAT-SB.FATstart*SB.block_size)/4*SB.block_size, 0, SB.block_size);
            memcpy(fp+(uncheckedFAT-SB.FATstart*SB.block_size)/4*SB.block_size, &status, 1);
            memcpy(fp+(uncheckedFAT-SB.FATstart*SB.block_size)/4*SB.block_size+1, &startingblock, 4);
            memcpy(fp+(uncheckedFAT-SB.FATstart*SB.block_size)/4*SB.block_size+5, &numberofblocks, 4);
            memcpy(fp+(uncheckedFAT-SB.FATstart*SB.block_size)/4*SB.block_size+9, &filesize, 4);
            memcpy(fp+(uncheckedFAT-SB.FATstart*SB.block_size)/4*SB.block_size+13, &year, 2);
            memcpy(fp+(uncheckedFAT-SB.FATstart*SB.block_size)/4*SB.block_size+15, &(timeb->month), 1);
            memcpy(fp+(uncheckedFAT-SB.FATstart*SB.block_size)/4*SB.block_size+16, &(timeb->day), 1);
            memcpy(fp+(uncheckedFAT-SB.FATstart*SB.block_size)/4*SB.block_size+17, &(timeb->hour), 1);
            memcpy(fp+(uncheckedFAT-SB.FATstart*SB.block_size)/4*SB.block_size+18, &(timeb->min), 1);
            memcpy(fp+(uncheckedFAT-SB.FATstart*SB.block_size)/4*SB.block_size+19, &(timeb->sec), 1);
            memcpy(fp+(uncheckedFAT-SB.FATstart*SB.block_size)/4*SB.block_size+20, &year, 2);
            memcpy(fp+(uncheckedFAT-SB.FATstart*SB.block_size)/4*SB.block_size+22, &(timeb->month), 1);
            memcpy(fp+(uncheckedFAT-SB.FATstart*SB.block_size)/4*SB.block_size+23, &(timeb->day), 1);
            memcpy(fp+(uncheckedFAT-SB.FATstart*SB.block_size)/4*SB.block_size+24, &(timeb->hour), 1);
            memcpy(fp+(uncheckedFAT-SB.FATstart*SB.block_size)/4*SB.block_size+25, &(timeb->min), 1);
            memcpy(fp+(uncheckedFAT-SB.FATstart*SB.block_size)/4*SB.block_size+26, &(timeb->sec), 1);
            memcpy(fp+(uncheckedFAT-SB.FATstart*SB.block_size)/4*SB.block_size+27, &tempbuf, strlen(tempbuf)+1);
            int ndx;
            for(ndx=0; ndx<8; ndx++){
                memset(fp+(uncheckedFAT-SB.FATstart*SB.block_size)/4*SB.block_size+58+ndx*64, 0xFF, 6);
            }
        }else{//directory
            uint32_t nextblock = start;
            for(i=0; i<numblocks*SB.block_size; i+=64){
                if(i%SB.block_size == 0 && i != 0){
                    nextblock = fourbfield(fp, SB.FATstart*SB.block_size + 4*(nextblock));
                    if(nextblock == 0xFFFFFFFF){
                        fprintf(stderr, "Corrupt directory.\n");
                        exit(1);
                    }
                }
                if(dirNameMatch(fp, tempbuf, i%SB.block_size+nextblock*SB.block_size)){//directory exists
                    uint32_t startblk = fourbfield(fp, i%SB.block_size+nextblock*SB.block_size+1);
                    uint32_t dirsizeloc = i%SB.block_size+nextblock*SB.block_size+5;
                    uint32_t dirsizeblk = fourbfield(fp, dirsizeloc);
                    writeDirInfo(fp, dirname, dirsizeloc, startblk, dirsizeblk, newstartblock, filedata, uncheckedFAT);
                    return;
                }
            }
            while(fourbfield(fp, uncheckedFAT) != 0){//find a place to put new directory
                uncheckedFAT += 4;
            }

            uint32_t temp = 0xFFFFFFFF;
            memcpy(fp+uncheckedFAT, &temp, 4);
            uint8_t status = 5;
            uint32_t startblk = (uncheckedFAT-SB.FATstart*SB.block_size)/4;
            uint32_t startingblock = htonl((uncheckedFAT-SB.FATstart*SB.block_size)/4);
            uint32_t numberofblocks = htonl(1);
            uint32_t filesize = htonl(SB.block_size);
            struct datetime_t *timeb = try_malloc(sizeof(struct datetime_t));
            getCurrentTime(timeb);
            uint16_t year = htons(timeb->year);
            nextblock = start;
            //find where to insert in parent directory
            for(i=0; i<numblocks*SB.block_size; i+=64){
                if(i%SB.block_size == 0 && i != 0){
                    nextblock = fourbfield(fp, SB.FATstart*SB.block_size + 4*(nextblock));
                    if(nextblock == 0xFFFFFFFF){
                        fprintf(stderr, "Corrupt directory.\n");
                        exit(1);
                    }
                }
                if(fourbfield(fp, i%SB.block_size+nextblock*SB.block_size) == 0){//create directory entry
                    memset(fp+(uncheckedFAT-SB.FATstart*SB.block_size)/4*SB.block_size, 0, SB.block_size);
                    memcpy(fp+i%SB.block_size+nextblock*SB.block_size, &status, 1);
                    memcpy(fp+i%SB.block_size+nextblock*SB.block_size+1, &startingblock, 4);
                    memcpy(fp+i%SB.block_size+nextblock*SB.block_size+5, &numberofblocks, 4);
                    memcpy(fp+i%SB.block_size+nextblock*SB.block_size+9, &filesize, 4);
                    memcpy(fp+i%SB.block_size+nextblock*SB.block_size+13, &year, 2);
                    memcpy(fp+i%SB.block_size+nextblock*SB.block_size+15, &(timeb->month), 1);
                    memcpy(fp+i%SB.block_size+nextblock*SB.block_size+16, &(timeb->day), 1);
                    memcpy(fp+i%SB.block_size+nextblock*SB.block_size+17, &(timeb->hour), 1);
                    memcpy(fp+i%SB.block_size+nextblock*SB.block_size+18, &(timeb->min), 1);
                    memcpy(fp+i%SB.block_size+nextblock*SB.block_size+19, &(timeb->sec), 1);
                    memcpy(fp+i%SB.block_size+nextblock*SB.block_size+20, &year, 2);
                    memcpy(fp+i%SB.block_size+nextblock*SB.block_size+22, &(timeb->month), 1);
                    memcpy(fp+i%SB.block_size+nextblock*SB.block_size+23, &(timeb->day), 1);
                    memcpy(fp+i%SB.block_size+nextblock*SB.block_size+24, &(timeb->hour), 1);
                    memcpy(fp+i%SB.block_size+nextblock*SB.block_size+25, &(timeb->min), 1);
                    memcpy(fp+i%SB.block_size+nextblock*SB.block_size+26, &(timeb->sec), 1);
                    memcpy(fp+i%SB.block_size+nextblock*SB.block_size+27, &tempbuf, strlen(tempbuf)+1);
                    int ndx;
                    for(ndx=0; ndx<8; ndx++){
                        memset(fp+(uncheckedFAT-SB.FATstart*SB.block_size)/4*SB.block_size+58+ndx*64, 0xFF, 6);
                    }
                    writeDirInfo(fp, dirname, i%SB.block_size+nextblock*SB.block_size+5, startblk, 1, newstartblock, filedata, uncheckedFAT+4);
                    return;
                }
            }
            //extend if parent directory is full
            while(fourbfield(fp, uncheckedFAT) != 0){//find a place
                uncheckedFAT += 4;
            }
            temp = 0xFFFFFFFF;
            memcpy(fp+uncheckedFAT, &temp, 4);//set new endblock in FAT
            temp = htonl((uncheckedFAT-SB.FATstart*SB.block_size)/4);
            memcpy(fp + (SB.FATstart*SB.block_size + nextblock*4), &temp, 4);//rewrite old endblock in FAT
            temp = htonl(fourbfield(fp, prevnumblocks)+1);
            memcpy(fp + prevnumblocks, &temp, 4);//extend num blocks
            temp = htonl(fourbfield(fp, prevnumblocks+4)+SB.block_size);
            memcpy(fp + prevnumblocks+4, &temp, 4);//extend filesize
            memset(fp+(uncheckedFAT-SB.FATstart*SB.block_size)/4*SB.block_size, 0, SB.block_size);
            memcpy(fp+(uncheckedFAT-SB.FATstart*SB.block_size)/4*SB.block_size, &status, 1);
            memcpy(fp+(uncheckedFAT-SB.FATstart*SB.block_size)/4*SB.block_size+1, &startingblock, 4);
            memcpy(fp+(uncheckedFAT-SB.FATstart*SB.block_size)/4*SB.block_size+5, &numberofblocks, 4);
            memcpy(fp+(uncheckedFAT-SB.FATstart*SB.block_size)/4*SB.block_size+9, &filesize, 4);
            memcpy(fp+(uncheckedFAT-SB.FATstart*SB.block_size)/4*SB.block_size+13, &year, 2);
            memcpy(fp+(uncheckedFAT-SB.FATstart*SB.block_size)/4*SB.block_size+15, &(timeb->month), 1);
            memcpy(fp+(uncheckedFAT-SB.FATstart*SB.block_size)/4*SB.block_size+16, &(timeb->day), 1);
            memcpy(fp+(uncheckedFAT-SB.FATstart*SB.block_size)/4*SB.block_size+17, &(timeb->hour), 1);
            memcpy(fp+(uncheckedFAT-SB.FATstart*SB.block_size)/4*SB.block_size+18, &(timeb->min), 1);
            memcpy(fp+(uncheckedFAT-SB.FATstart*SB.block_size)/4*SB.block_size+19, &(timeb->sec), 1);
            memcpy(fp+(uncheckedFAT-SB.FATstart*SB.block_size)/4*SB.block_size+20, &year, 2);
            memcpy(fp+(uncheckedFAT-SB.FATstart*SB.block_size)/4*SB.block_size+22, &(timeb->month), 1);
            memcpy(fp+(uncheckedFAT-SB.FATstart*SB.block_size)/4*SB.block_size+23, &(timeb->day), 1);
            memcpy(fp+(uncheckedFAT-SB.FATstart*SB.block_size)/4*SB.block_size+24, &(timeb->hour), 1);
            memcpy(fp+(uncheckedFAT-SB.FATstart*SB.block_size)/4*SB.block_size+25, &(timeb->min), 1);
            memcpy(fp+(uncheckedFAT-SB.FATstart*SB.block_size)/4*SB.block_size+26, &(timeb->sec), 1);
            memcpy(fp+(uncheckedFAT-SB.FATstart*SB.block_size)/4*SB.block_size+27, &tempbuf, strlen(tempbuf)+1);
            int ndx;
            for(ndx=0; ndx<8; ndx++){
                memset(fp+(uncheckedFAT-SB.FATstart*SB.block_size)/4*SB.block_size+58+ndx*64, 0xFF, 6);
            }
            writeDirInfo(fp, dirname, (uncheckedFAT-SB.FATstart*SB.block_size)/4*SB.block_size+5, startblk, 1, newstartblock, filedata, uncheckedFAT+4);
        }
    }else{
        fprintf(stderr, "Input format: /subdir/subdir/filename\n");
        exit(1);
    }
}

//writes the file contents and tracks which parts of fat should be updated. Creates directory entries. Updates FAT
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
    uint32_t filesize = (uint32_t)sf.st_size;
    uint32_t fileblocks = filesize/SB.block_size + (filesize%SB.block_size == 0 ? 0 : 1); 
    uint32_t FATaddresses[fileblocks];
    uint32_t FATvalues[fileblocks];
    uint32_t uncheckedFAT = writeFileContents(fp, p, filesize, FATaddresses, FATvalues);
    writeDirInfo(fp, olocation, 26, SB.rootstart, SB.root_block_count, FATaddresses[0], &sf, uncheckedFAT);
    int i;
    for(i=0; i<fileblocks;i++){
        memcpy(fp+FATaddresses[i], FATvalues+i, 4);
    }
}
#endif

#if defined(PART3)
//transfers file from disk to specified file/location on current linux machine
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
        fwrite(fp+nextblock*SB.block_size, SB.block_size, 1, new);
        nextblock = fourbfield(fp, SB.FATstart*SB.block_size + 4*(nextblock));
    }
    uint32_t remainingbytes = filesize%SB.block_size==0 ? SB.block_size : filesize%SB.block_size;
    fwrite(fp+nextblock*SB.block_size, remainingbytes, 1, new);
}

//locates where the file to retrieve is in disk
void getFile(char* dirname, char* fp, int start, int numblocks, char *filename){
    if(dirname[0] == '/'){
        dirname++;
        if(dirname[0] == '\0'){
            fprintf(stderr, "Input format: /subdir/subdir/filename\n");
            exit(1);
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
                nextblock = fourbfield(fp, SB.FATstart*SB.block_size + 4*(nextblock));
                if(nextblock == 0xFFFFFFFF){
                    fprintf(stderr, "File not found.\n");
                    exit(1);
                }
            }
            if(dirNameMatch(fp, tempbuf, i%SB.block_size+nextblock*SB.block_size) || fileNameMatch(fp, tempbuf, i%SB.block_size+nextblock*SB.block_size)){
                uint32_t startb = fourbfield(fp, i%SB.block_size+nextblock*SB.block_size+1);
                uint32_t dirsizeb = fourbfield(fp, i%SB.block_size+nextblock*SB.block_size+5);
                uint32_t filesize = fourbfield(fp, i%SB.block_size+nextblock*SB.block_size+9);
                if(dirname[0] == '\0'){//is a file
                    transferFile(fp, filename, dirsizeb, filesize, startb);
                    return;
                }else{//is a directory
                    getFile(dirname, fp, startb, dirsizeb, filename);
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
//prints information to screen about every file in a directory
void printFileInfo(char* fp, uint32_t start, uint32_t numblocks){
    int i;
    uint32_t nextblock = start;
    for(i=0;i<numblocks*SB.block_size;i+=64){
        if(i%SB.block_size == 0 && i != 0){
            nextblock = fourbfield(fp, SB.FATstart*SB.block_size + 4*(nextblock));
            if(nextblock == 0xFFFFFFFF)return;
        }
        if((fp[i%SB.block_size+nextblock*SB.block_size] & 3) == 3){
            printf("F ");
        }else if((fp[i%SB.block_size+nextblock*SB.block_size] & 7) == 5){
            printf("D ");
        }else{
            continue;
        }
        uint32_t filesize = fourbfield(fp, i%SB.block_size+nextblock*SB.block_size+9);
        char filename[FILENAMELIM];
        int x = 0;
        for(x=0; x<FILENAMELIM; x++){
            filename[x] = fp[i%SB.block_size+27+nextblock*SB.block_size+x];
        }
        uint16_t year = twobfield(fp, i%SB.block_size+nextblock*SB.block_size+20);
        uint8_t month = onebfield(fp, i%SB.block_size+nextblock*SB.block_size+22);
        uint8_t day = onebfield(fp, i%SB.block_size+nextblock*SB.block_size+23);
        uint8_t hour = onebfield(fp, i%SB.block_size+nextblock*SB.block_size+24);
        uint8_t minute = onebfield(fp, i%SB.block_size+nextblock*SB.block_size+25);
        uint8_t second = onebfield(fp, i%SB.block_size+nextblock*SB.block_size+26);
        printf("%10d %30s ", filesize, filename);
        printf("%4d/%02d/%02d %2d:%02d:%02d\n", year, month, day, (hour+17)%24, minute, second);
    }
}

//finds the directory to print information about.
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
                nextblock = fourbfield(fp, SB.FATstart*SB.block_size + 4*(nextblock));
                if(nextblock == 0xFFFFFFFF){
                    fprintf(stderr, "Directory corrupt.\n");
                    exit(1);
                }
            }
            if(dirNameMatch(fp, tempbuf, i%SB.block_size+nextblock*SB.block_size)){
                uint32_t startb = fourbfield(fp, i%SB.block_size+nextblock*SB.block_size+1);
                uint32_t dirsizeb = fourbfield(fp, i%SB.block_size+nextblock*SB.block_size+5);
                if(dirname[0] == '\0'){
                    printFileInfo(fp, startb, dirsizeb);
                    return;
                }else{
                    printDirInfo(dirname, fp, startb, dirsizeb);
                    return;
                }
            }
        }
        fprintf(stderr, "Directory not found.\n");
    }else{
        fprintf(stderr, "Input format: /subdir/subdir/subdir\n");
        exit(1);
    }
}
#endif

#if defined(PART1)
//prints information from the super block and FAT
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

//iterates through the FAT to count the allocated/reserved/free blocks
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

//reads the superblock to get information about the filesystem
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
        fprintf(stderr, "Must specify a disc image.\n");
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

//TODO 
//     -refactor into functions :( sorry person who marks this