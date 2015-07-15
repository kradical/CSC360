#include <stdio.h>

int main(void){
    #if defined(PART1)
    printf("Part 1: diskinfo\n");
    #elif defined(PART2)
    printf("Part 2: disklist\n");
    #elif defined(PART3)
    printf("Part 3: diskget\n");
    #elif defined(PART4)
    printf("Part 4: diskput\n");
    #endif

    return 0;
}