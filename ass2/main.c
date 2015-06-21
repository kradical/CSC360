#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <unistd.h>

struct train{
    pthread_t thread;
    struct train *next;
    char number;
    char *direction;
    char loading_time;
    char crossing_time;
    char priority; //0 for low priority 1 for high priority
};

void print_both_queues(void);
void add_train_w(struct train *);
void add_train_e(struct train *);
struct timespec diff(struct timespec);
void print_time(struct timespec);
void parse_input_file(FILE *, struct train *, int);
void *TrainFunction(void *);

pthread_mutex_t elock;
pthread_mutex_t wlock;
pthread_cond_t main_track_cv;
pthread_barrier_t barrier;
struct timespec ts_start;
struct train *ehead = NULL;
struct train *whead = NULL;

/**/
void print_both_queues(void){
    struct train *temp;
    temp = whead;
    printf("West List:  \n\n");
    while(temp != NULL){
        printf("Train #%d %d %d\n", temp->number, temp->priority, temp->loading_time);
        temp = temp->next;
    }
    temp = ehead;
    printf("\nEast List:  \n\n");
    while(temp != NULL){
        printf("Train #%d %d\n", temp->number, temp->priority);
        temp = temp->next;
    }
}

/**/
struct train *remove_train(char *direction){
    struct train *temp;
    if(direction == "West"){
        temp = whead;
        if(temp->next != NULL){
            whead = temp->next;
        }else{
            whead = NULL;
        }
    }else{
        temp = ehead;
        if(temp->next != NULL){
            ehead = temp->next;
        }else{
            ehead = NULL;
        }
    }
    return temp;
}
/**/
void add_in_place(struct train *prev, struct train *t){
    struct train * temp;
    if(prev == NULL && t->direction == "West"){
        temp = whead;
        whead = t;
        t->next = temp;
    }else if(prev == NULL && t->direction == "East"){
        temp = ehead;
        ehead = t;
        t->next = temp;
    }else{
        temp = prev->next;
        prev->next = t;
        t->next = temp;
    }
}

/* priority > load time > inputfile order*/
void add_train_w(struct train *t){
    if(whead == NULL){
        whead = t;
    }else{
        struct train *temp = whead;
        struct train *prev = NULL;
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

/**/
void add_train_e(struct train *t){
    if(ehead == NULL){
        ehead = t;
    }else{
        struct train *temp = ehead;
        struct train *prev = NULL;
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

/**/
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

/**/
void calculate_time(struct timespec ts_current, long *msec, int *sec, int *min, int *hour){
    struct timespec ts_diff;
    ts_diff = diff(ts_current); 
    *sec = (int)ts_diff.tv_sec % 60;
    *min = ((int)ts_diff.tv_sec / 60) % 60;
    *hour = ((int)ts_diff.tv_sec / 3600) % 24;
    *msec = ts_diff.tv_nsec/100000000;
}

/**/
void parse_input_file(FILE *fp, struct train *trains, int numtrains){
    char direction_buf;
    int loading_buf;
    int crossing_buf;
    int i = 0;
    while(fscanf(fp, "%c:%d,%d\n", &direction_buf, &loading_buf, &crossing_buf) != EOF){
        trains[i].loading_time = (char)loading_buf;
        trains[i].crossing_time = (char)crossing_buf;
        trains[i].number = (char)i;
        if(direction_buf >= 'a'){
            trains[i].priority = 0;
        }else{
            trains[i].priority = 1;
        }
        if(direction_buf == 'W' || direction_buf == 'w'){           
            trains[i].direction = "West";
        }else{
            trains[i].direction = "East";
        }
        i++;
        if(i >= numtrains)break;
    }   
}

/**/
void *TrainFunction(void *trainid){
    struct train *t;
    t = (struct train *)trainid;
    struct timespec ts_current;
    long msec;
    int sec;
    int min;
    int hour;

    pthread_barrier_wait (&barrier);
    usleep(t->loading_time*100000);
    clock_gettime(CLOCK_MONOTONIC, &ts_current);
    calculate_time(ts_current, &msec, &sec, &min, &hour);
    printf("%02d:%02d:%02d.%1ld Train %2d is ready to go %s\n", hour, min, sec, msec, t->number, t->direction);
    if(t->direction == "West"){
        pthread_mutex_lock(&wlock);
        add_train_w(t);
        pthread_cond_signal(&main_track_cv);
        pthread_mutex_unlock(&wlock);
    }else{
        while(pthread_mutex_lock(&elock) != 0);
        add_train_e(t);
        pthread_cond_signal(&main_track_cv);
        pthread_mutex_unlock(&elock);
    }
    pthread_exit(NULL);
}

void dispatcher(int numtrains){
    int trains_finished = 0;
    char *prev_direction = "East";
    struct train *temp;
    struct timespec ts_current;
    long msec;
    int sec;
    int min;
    int hour;
    while(trains_finished < numtrains){
        //both locks aquired
        while(pthread_mutex_lock(&wlock) != 0);
        while(whead == NULL && ehead == NULL){
            pthread_cond_wait(&main_track_cv, &wlock);

        }
        if(whead != NULL && ehead != NULL){
            if(whead->priority == 1){
                if(ehead->priority == 1){
                    if(prev_direction == "East"){
                        //both high priority last train east
                        temp = remove_train("West");
                    }else{
                        //both high priority last train west
                        temp = remove_train("East");
                    }
                }else{
                    //west high priority east low priority
                    temp = remove_train("West");
                }
            }else{
                if(ehead->priority == 0){
                    if(prev_direction == "East"){
                        //both low priority last train east
                        temp = remove_train("West");
                    }else{
                        //both low priority last train west
                        temp = remove_train("East");
                    }
                }else{
                    //east high priority west low priority
                    temp = remove_train("East");
                }
            }
        }else if(whead != NULL){
            //only west trains
            temp = remove_train("West");
        }else{
            //only east trains
            temp = remove_train("East");
        }
        pthread_mutex_unlock(&wlock);
        prev_direction = temp->direction;
        clock_gettime(CLOCK_MONOTONIC, &ts_current);
        calculate_time(ts_current, &msec, &sec, &min, &hour);
        printf("%02d:%02d:%02d.%1ld Train %2d is ON the main track going %s\n", hour, min, sec, msec, temp->number, temp->direction);
        usleep(temp->crossing_time*100000);
        clock_gettime(CLOCK_MONOTONIC, &ts_current);
        calculate_time(ts_current, &msec, &sec, &min, &hour);
        printf("%02d:%02d:%02d.%1ld Train %2d is OFF the main track after going %s\n", hour, min, sec, msec, temp->number, temp->direction);
        trains_finished++;
    }
    return;
}

int main(int argc, char *argv[]){
    if( argc != 3 ){
        fprintf(stderr, "usage: %s filename numberoftrains\n", argv[0]);
        exit(1);
    }
    
    int number_of_trains = atoi(argv[2]);
    FILE *input_file = fopen(argv[1], "r");
    
    struct train *trains = malloc(sizeof(struct train)*number_of_trains);
    if(input_file == 0){
        fprintf(stderr, "Could not open file %s\n", argv[1]);
        exit(1);
    }else{
        parse_input_file(input_file, trains, number_of_trains);
    }
    fclose(input_file);
    
    pthread_barrier_init(&barrier, NULL, number_of_trains);
    pthread_cond_init (&main_track_cv, NULL);
    pthread_cond_init(&trainsw_cv, NULL);
    pthread_mutex_init(&wlock, NULL);
    pthread_mutex_init(&elock, NULL);
    pthread_mutex_init(&main_track_mx, NULL);

    int i = 0;
    int return_code;
    struct train *temp;
    for(i=0; i < number_of_trains; i++){
        temp = &trains[i];
        return_code = pthread_create(&temp->thread, NULL, TrainFunction, (void*)temp);
        if(return_code){
            fprintf(stderr, "ERROR: return code from pthread_create() is %d\n", return_code);
            exit(1);
        }
    }
    clock_gettime(CLOCK_MONOTONIC, &ts_start); 
    dispatcher(number_of_trains);
    for(i=0; i < number_of_trains; i++){
        temp = &trains[i];
        pthread_join(temp->thread, NULL);
    }
    pthread_mutex_destroy(&wlock);  
    print_both_queues();
    return 0;
}

//TODO check return of get time for error
//TODO check that number of trains == number lines read
//TODO remove print_both_queues
//TODO lock both mutexes when removing