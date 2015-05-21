#include <stdio.h>
#include <stdlib.h>
#include <readline/readline.h>
#include <readline/history.h>

int main(void){
	printf("LETS DYNAMICALLY ALLOCATE A STRING ARRAY!\n");
	char **commandArray = NULL;
	char *reply;
    char *token;	
    int n_spaces = 0, i;

	reply = readline(NULL);
	
    token = strtok(reply, " ");
	while(token != NULL){
        commandArray = realloc(commandArray, sizeof(char*)*++n_spaces);
        if(commandArray == NULL){
            exit(-1);
        }
        commandArray[n_spaces-1] = token;
        token = strtok(NULL, " ");
	}
    
    commandArray = realloc(commandArray, sizeof(char*) * (n_spaces+1));
    commandArray[n_spaces] = 0;

    for(i = 0; i < (n_spaces+1); i++)
        printf("commandArray[%d] = %s\n", i, commandArray[i]);

    free(commandArray);
    
    return 0;
}
