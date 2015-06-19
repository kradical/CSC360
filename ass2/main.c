#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <unistd.h>

struct train{
    pthread_t thread;
    char number;
    char *direction;
    char loading_time;
    char crossing_time;
    char priority; //0 for low priority 1 for high priority
};

struct node{
    struct train *train;
    struct node *next;
};

void add_train_w(struct train *);
void add_train_e(struct train *);
struct timespec diff(struct timespec);
void print_time(struct timespec);
void parse_input_file(FILE *, struct train *, int);
void *TrainFunction(void *);

pthread_barrier_t barrier;
struct timespec ts_start;
struct node ehead;
struct node whead;


/**/
void add_train_w(struct train *train){
    printf("%d", train->number);
}

/**/
void add_train_e(struct train *train){
    printf("%d", train->number);
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
void print_time(struct timespec ts_current){
    struct timespec ts_diff;
    ts_diff = diff(ts_current); 
    int s  = (long long)ts_diff.tv_sec % 60;
    int m = ((long long)ts_diff.tv_sec / 60) % 60;
    int h = ((long long)ts_diff.tv_sec / 3600) % 24;
    long ms = ts_diff.tv_nsec/100000000;
    printf("%02d:%02d:%02d.%1ld ", h, m, s, ms);
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
    pthread_barrier_wait (&barrier);
    
    usleep(t->loading_time*100000);
    
    clock_gettime(CLOCK_MONOTONIC, &ts_current);
    print_time(ts_current);

    printf("Train %2d is ready to go %s\n", t->number, t->direction);
    if(t->direction == "West"){
        //THIS IS CRITICAL
        add_train_w(t);
    }else{
        //THIS IS CRITICAL
        add_train_e(t);
    }
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
        return_code = pthread_create(&temp->thread, NULL, TrainFunction, (void*)temp);
        if(return_code){
            fprintf(stderr, "ERROR: return code from pthread_create() is %d\n", return_code);
            exit(1);
        }
    }
    clock_gettime(CLOCK_MONOTONIC, &ts_start); 
    for(i=0; i < number_of_trains; i++){
        temp = &trains[i];
        pthread_join(temp->thread, NULL);
    }    
    return 0;
}
