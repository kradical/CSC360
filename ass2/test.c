#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

struct train{
    char direction;
    char loading_time;
    char crossing_time;
    char priority; //0 for low priority 1 for high priority
};

void parse_input_file(FILE *fp, struct train *trains, int numtrains){
    char direction_buf;
    int loading_buf;
    int crossing_buf;
    int i = 0;
    while(fscanf(fp, "%c:%d,%d\n", &direction_buf, &loading_buf, &crossing_buf) != EOF){
        trains[i].direction = direction_buf;
        trains[i].loading_time = (char)loading_buf;
        trains[i].crossing_time = (char)crossing_buf;
        if(direction_buf >= 'a'){
            trains[i].priority = 0;
        }else{
            trains[i].priority = 1;
        }
        i++;
        if(i >= numtrains)break;
    }   
}

void *PrintTrain(void *trainid){
    struct train *t;
    t = (struct train *)trainid;
    printf("%c:%d,%d    %d\n", t->direction, t->loading_time, t->crossing_time, t->priority);
    pthread_exit(NULL);
}

int main(int argc, char *argv[]){
    if( argc != 3 ){
        fprintf(stderr, "usage: %s filename numberoftrains\n", argv[0]);
        exit(1);
    }
    
    int number_of_trains = atoi(argv[2]);
    FILE *input_file = fopen(argv[1], "r");
    
    struct train trains[number_of_trains];
    pthread_t threads[number_of_trains];
    
    if(input_file == 0){
        fprintf(stderr, "Could not open file %s\n", argv[1]);
        exit(1);
    }else{
        parse_input_file(input_file, trains, number_of_trains);
    }

    int i = 0;
    int return_code;
    struct train *temp;
    for(i=0; i < number_of_trains; i++){
        temp = &trains[i];
        return_code = pthread_create(&threads[i], NULL, PrintTrain, (void*)temp);
        if(rc){
            fprintf(stderr, "ERROR: return code from pthread_create() is %d\n", rc);
            exit(1);
        }
    }

    fclose(input_file);
    return 0;
}
