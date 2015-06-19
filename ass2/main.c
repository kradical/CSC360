#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

pthread_barrier_t barrier; //initial synchronization object

struct train{
    pthread_t thread;
    char number;
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
        trains[i].number = (char)i;
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
    pthread_barrier_wait (&barrier);
    struct train *t;
    t = (struct train *)trainid;
    char *direction;
    if(t->direction == 'W' || t->direction == 'w'){
        direction = "West";
    }else{
        direction = "East";
    }
    usleep(t->loading_time*100000);
    printf("Train %2d is ready to go %4s\n",t->number, direction);
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
    pthread_barrier_init(&barrier, NULL, number_of_trains);
    
    if(input_file == 0){
        fprintf(stderr, "Could not open file %s\n", argv[1]);
        exit(1);
    }else{
        parse_input_file(input_file, trains, number_of_trains);
    }
    fclose(input_file);

    int i = 0;
    int return_code;
    struct train *temp;
    for(i=0; i < number_of_trains; i++){
        temp = &trains[i];
        return_code = pthread_create(&temp->thread, NULL, PrintTrain, (void*)temp);
        if(return_code){
            fprintf(stderr, "ERROR: return code from pthread_create() is %d\n", return_code);
            exit(1);
        }
    }
    for(i=0; i < number_of_trains; i++){
        temp = &trains[i];
        pthread_join(temp->thread, NULL);
    }
    
    
    return 0;
}
