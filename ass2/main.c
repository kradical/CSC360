#include <pthread.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <unistd.h>

struct train{
    pthread_t thread; //thread id
    struct train *next;
    char number; //id in range [0, 99]
    char *direction; //East or West
    char loading_time; //10ths of seconds in range [1, 99]
    char crossing_time; //10ths of seconds in range [1, 99]
    char priority; //0 for low priority 1 for high priority
};

void *try_malloc(int);
struct timespec diff(struct timespec);
void calculate_time(struct timespec, long *, int *, int *, int *);
struct train *pop_train(char *direction);
void add_in_place(struct train *, struct train *);
void add_train(struct train *);
void *TrainFunction(void *);
void dispatcher(int);
void parse_input_file(FILE *, struct train *, int);

pthread_mutex_t eastbound_lock;
pthread_mutex_t westbound_lock;
pthread_mutex_t main_track_lock;
pthread_cond_t added_cv;
pthread_barrier_t initial_barrier;
struct timespec ts_start;
struct train *eastbound_head = NULL;
struct train *westbound_head = NULL;

/* helper function to error check calls to malloc */
void *try_malloc(int size){
    void *p = malloc(size); 
    if(p == NULL){
        perror("Error allocating memory");
        exit(1);
    }
    return p;
}

/* helper function to compute the difference between 2 timespec structs */
struct timespec diff(struct timespec end){
    struct timespec temp;
    if ((end.tv_nsec-ts_start.tv_nsec)<0) {
        temp.tv_sec = end.tv_sec-ts_start.tv_sec-1;
        temp.tv_nsec = 1000000000+end.tv_nsec-ts_start.tv_nsec;
    } else {
        temp.tv_sec = end.tv_sec-ts_start.tv_sec;
        temp.tv_nsec = end.tv_nsec-ts_start.tv_nsec;
    }
    return temp;
}

/* helper function to convert a timespec struct into human readable time */
void calculate_time(struct timespec ts_current, long *msec, int *sec, int *min, int *hour){
    struct timespec ts_diff;
    ts_diff = diff(ts_current); 
    *sec = (int)ts_diff.tv_sec % 60;
    *min = ((int)ts_diff.tv_sec / 60) % 60;
    *hour = ((int)ts_diff.tv_sec / 3600) % 24;
    *msec = ts_diff.tv_nsec/100000000;
}

/* dispatcher uses this function to get the head of the list, direction is
   passed to ensure the proper list, both mutexes are locked in the
   dispatcher and unlocked here */
struct train *pop_train(char *direction){
    struct train *temp;
    if(direction == "West"){
        pthread_mutex_unlock(&eastbound_lock);
        temp = westbound_head;
        westbound_head = temp->next;
        pthread_mutex_unlock(&westbound_lock);
    }else{
        pthread_mutex_unlock(&westbound_lock);
        temp = eastbound_head;
        eastbound_head = temp->next;
        pthread_mutex_unlock(&eastbound_lock);
    }
    return temp;
}

/* helper function to add a train where it's supposed to go in it's list */
void add_in_place(struct train *prev, struct train *t){
    struct train * temp;
    if(prev == NULL && t->direction == "West"){
        temp = westbound_head;
        westbound_head = t;
        t->next = temp;
    }else if(prev == NULL && t->direction == "East"){
        temp = eastbound_head;
        eastbound_head = t;
        t->next = temp;
    }else{
        temp = prev->next;
        prev->next = t;
        t->next = temp;
    }
}

/* takes a train and adds it to the correct list in the correct place 
   priority > load time > number 
    1 > 0      asc.        asc. */
void add_train(struct train *t){
    if(t->direction == "West" && westbound_head == NULL){
        westbound_head = t;
    }else if(t->direction == "East" && eastbound_head == NULL){
        eastbound_head = t;
    }else{
        struct train *temp = NULL;
        struct train *prev = NULL;
        if(t->direction == "West"){
            temp = westbound_head;
        }else{
            temp = eastbound_head;
        }
        while(temp != NULL){
            if(t->priority > temp->priority){
                add_in_place(prev, t);
                return;
            }else if(t->priority == temp->priority){
                if(t->loading_time < temp->loading_time){
                    add_in_place(prev, t);
                    return;
                }else if(t->loading_time == temp->loading_time){
                    if(t->number < temp->number){
                        add_in_place(prev, t);
                        return;
                    }
                }
            }
            prev = temp;
            temp = temp->next;
        }
        prev->next = t;
    }
}

/* function called by each spawned thread, takes a train struct, waits at
   barrier, sleeps for loading time, adds itself to the proper list then 
   signals the dispatcher */
void *TrainFunction(void *trainid){
    int return_code;
    struct train *t;
    t = (struct train *)trainid;
    struct timespec ts_current;
    long msec;
    int sec;
    int min;
    int hour;

    pthread_barrier_wait (&initial_barrier);
    return_code = usleep(t->loading_time*100000);
    if(return_code){
        perror("ERROR: return code from usleep() is -1");
        exit(1);
    } 
    return_code = clock_gettime(CLOCK_MONOTONIC, &ts_current);
    if(return_code){
        perror("ERROR: return code from clock_gettime() is -1");
        exit(1);
    } 
    calculate_time(ts_current, &msec, &sec, &min, &hour);
    if(t->direction == "West"){
        while(pthread_mutex_lock(&westbound_lock) != 0);
        add_train(t);
        printf("%02d:%02d:%02d.%1ld Train %2d is ready to go %4s\n", hour, min, sec, msec, t->number, t->direction);
        pthread_mutex_unlock(&westbound_lock);
    }else{
        while(pthread_mutex_lock(&eastbound_lock) != 0);
        add_train(t);
        printf("%02d:%02d:%02d.%1ld Train %2d is ready to go %4s\n", hour, min, sec, msec, t->number, t->direction);
        pthread_mutex_unlock(&eastbound_lock);
    }
    pthread_cond_signal(&added_cv);
}

/* function called by main after spawning all the train threads, ensures the 
   trains are sent in the right order, keeps count to ensure all the trains
   are sent */
void dispatcher(int numtrains){
    int return_code;
    int trains_finished = 0;
    char *prev_direction = "East";
    struct train *temp;
    struct timespec ts_current;
    long msec;
    int sec;
    int min;
    int hour;

    while(trains_finished < numtrains){
        while(pthread_mutex_lock(&main_track_lock) != 0);
        while(westbound_head == NULL && eastbound_head == NULL){ //wait if lists are both empty
            pthread_cond_wait(&added_cv, &main_track_lock);
        }
        pthread_mutex_unlock(&main_track_lock);
        while(pthread_mutex_lock(&westbound_lock) != 0);
        while(pthread_mutex_lock(&eastbound_lock) != 0);
        if(westbound_head != NULL && eastbound_head != NULL){
            if(westbound_head->priority == 1){
                if(eastbound_head->priority == 1){
                    if(prev_direction == "East"){ //both high priority last train east
                        temp = pop_train("West");
                    }else{ //both high priority last train west
                        temp = pop_train("East");
                    }
                }else{ //west high priority east low priority
                    temp = pop_train("West");
                }
            }else{
                if(eastbound_head->priority == 0){
                    if(prev_direction == "East"){ //both low priority last train east
                        temp = pop_train("West");
                    }else{ //both low priority last train west
                        temp = pop_train("East");
                    }
                }else{ //east high priority west low priority
                    temp = pop_train("East");
                }
            }
        }else if(westbound_head != NULL){ //only west trains
            temp = pop_train("West");
        }else{ //only east trains
            temp = pop_train("East");
        }
        
        prev_direction = temp->direction;
        
        clock_gettime(CLOCK_MONOTONIC, &ts_current);
        if(return_code){
            perror("ERROR: return code from clock_gettime() is -1");
            exit(1);
        } 
        calculate_time(ts_current, &msec, &sec, &min, &hour);
        printf("%02d:%02d:%02d.%1ld Train %2d is ON the main track going %s\n", hour, min, sec, msec, temp->number, temp->direction);
        
        usleep(temp->crossing_time*100000);
        
        clock_gettime(CLOCK_MONOTONIC, &ts_current);
        if(return_code){
            perror("ERROR: return code from clock_gettime() is -1");
            exit(1);
        } 
        calculate_time(ts_current, &msec, &sec, &min, &hour);
        printf("%02d:%02d:%02d.%1ld Train %2d is OFF the main track after going %s\n", hour, min, sec, msec, temp->number, temp->direction);
        
        trains_finished++;
    }
}

/* function takes the user supplied file and loads all the train information
   into the train structs, exits if the number supplied is greater than the
   number of trains in the input file */
void parse_input_file(FILE *fp, struct train *trains, int numtrains){
    char direction_buf;
    int loading_buf;
    int crossing_buf;
    int i = 0;
    while(fscanf(fp, "%c:%d,%d\n", &direction_buf, &loading_buf, &crossing_buf) != EOF){
        if(i > numtrains)break;

        trains[i].loading_time = (char)loading_buf;
        trains[i].crossing_time = (char)crossing_buf;
        trains[i].number = (char)i;
        if(direction_buf >= 'a'){ //lowercase
            trains[i].priority = 0;
        }else{ //uppercase
            trains[i].priority = 1;
        }
        if(direction_buf == 'W' || direction_buf == 'w'){           
            trains[i].direction = "West";
        }else{
            trains[i].direction = "East";
        }
        i++;
    }
    if(numtrains > i){
        fprintf(stderr, "There are less than %d trains in the input file\n", numtrains);
        exit(1);
    }   
}

int main(int argc, char *argv[]){
    if( argc != 3 ){
        fprintf(stderr, "Usage: %s [FILE] [INT]\n", argv[0]);
        exit(1);
    }
    
    int number_of_trains = atoi(argv[2]);
    if(number_of_trains < 1){
        fprintf(stderr, "Usage: %s [FILE] [INT]\nOnly use numbers in [1, %d]\n", argv[0], INT_MAX);
        exit(1);
    }
    
    int return_code;
    int i = 0;
    struct train *temp;
    struct train *trains = try_malloc(sizeof(struct train)*number_of_trains);
    FILE *input_file = NULL;

    input_file = fopen(argv[1], "r");
    if(input_file == NULL){
        fprintf(stderr, "Could not open file %s\n", argv[1]);
        exit(1);
    }else{
        parse_input_file(input_file, trains, number_of_trains);
    }
    fclose(input_file);

    return_code = pthread_barrier_init(&initial_barrier, NULL, number_of_trains);
    if(return_code){
        fprintf(stderr, "ERROR: return code from pthread_barrier_init() is %d\n", return_code);
        exit(1);
    }
    return_code = pthread_cond_init (&added_cv, NULL);
    if(return_code){
        fprintf(stderr, "ERROR: return code from pthread_cond_init() is %d\n", return_code);
        exit(1);
    }
    return_code = pthread_mutex_init(&westbound_lock, NULL);
    if(return_code){
        fprintf(stderr, "ERROR: return code from pthread_mutex_init() is %d\n", return_code);
        exit(1);
    }
    return_code = pthread_mutex_init(&eastbound_lock, NULL);
    if(return_code){
        fprintf(stderr, "ERROR: return code from pthread_mutex_init() is %d\n", return_code);
        exit(1);
    }
    return_code = pthread_mutex_init(&main_track_lock, NULL);
    if(return_code){
        fprintf(stderr, "ERROR: return code from pthread_mutex_init() is %d\n", return_code);
        exit(1);
    }

    for(i=0; i < number_of_trains; i++){
        temp = &trains[i];
        return_code = pthread_create(&temp->thread, NULL, TrainFunction, (void*)temp);
        if(return_code){
            fprintf(stderr, "ERROR: return code from pthread_create() is %d\n", return_code);
            exit(1);
        }
    }
    return_code = clock_gettime(CLOCK_MONOTONIC, &ts_start);
    if(return_code){
        perror("ERROR: return code from clock_gettime() is -1");
        exit(1);
    } 
    dispatcher(number_of_trains);

    pthread_barrier_destroy(&initial_barrier);
    pthread_cond_destroy(&added_cv);
    pthread_mutex_destroy(&westbound_lock);
    pthread_mutex_destroy(&eastbound_lock);
    pthread_mutex_destroy(&main_track_lock);
    free(trains);
    return 0;
}