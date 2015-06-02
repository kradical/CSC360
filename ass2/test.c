#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

void *PrintHello(void *threadid){
	long tid;
	tid = (long)threadid;
	printf("Hello World! It's thread #%ld!\n", tid);
	pthread_exit(NULL);
}

int main(void){
	pthread_t threads[5];
	int rc;
	long t;
	for(t=0; t<5; t++){
		rc = pthread_create(&threads[t], NULL, PrintHello, (void*)t);
		if (rc){
			printf("ERROR");
			exit(-1);
		}
	}
	pthread_exit(NULL);
}
