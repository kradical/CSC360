#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>

#define INITIAL_PATH_SIZE 128
#define INITIAL_COMMAND_BUFFER_SIZE 8

typedef struct process{
    pid_t pid;
    int arguements;
    char *processname;
    char **processargsv;
    struct process *next;
}PROCESS;

void delete_process(pid_t pid);
void add_process(PROCESS **head, pid_t pid, char **commands, int arguements);
void print_process_list(PROCESS *head);

void *try_malloc(int size);
char *get_prompt();
int parse_command(char *command, char ***p);
void command_arbitrary(char **command_array, int arguements, PROCESS **head);
void command_change_directory(char **command_array, int arguements);
void child_sig_handler(int signal_number)

/*
    try_malloc, helpful function for mallocing char* with error checking
*/
void *try_malloc(int size){
    void *p = malloc(size); 
    if(p == NULL){
        perror("Error allocating memory");
        exit(1);
    }
    return p;
}

/*
   delete_process
*/
void delete_process(pid_t pid){
    printf("DELETE");
    return;
}

/*
   add_process
*/
void add_process(PROCESS **head, pid_t pid, char **commands, int arguements){
    int i = 0;	
    if(*head == NULL){	
        (*head) = try_malloc(sizeof(PROCESS));
        (*head)->pid = pid;
        (*head)->arguements = arguements;
        (*head)->processname = try_malloc(sizeof(commands[0]));
        strcpy((*head)->processname, commands[0]);
        (*head)->processargsv = try_malloc(sizeof(commands));		
        for(i=0; i<arguements; i++){
            (*head)->processargsv[i] = try_malloc(sizeof(commands[i]));
            strcpy((*head)->processargsv[i], commands[i]);
        }
        (*head)->next = NULL;
    }else{
        PROCESS *temp = try_malloc(sizeof(PROCESS));
        temp->pid = pid;
        temp->arguements = arguements;
        temp->next = NULL;
        temp->processname = try_malloc(sizeof(commands[0]));
        strcpy(temp->processname, commands[0]);
        temp->processargsv = try_malloc(sizeof(commands));		
        for(i=0; i<arguements; i++){
            temp->processargsv[i] = try_malloc(sizeof(commands[i]));
            strcpy(temp->processargsv[i], commands[i]);
        }
        PROCESS *p = *head;
        while(p->next != NULL){
        p=p->next;
        }
        p->next = temp;
    }
    return;
}

/*
    print_process_list
*/
void print_process_list(PROCESS *head){
    PROCESS *temp = head;
    int i = 0;
    int jobs = 0;
    while(temp != NULL){
        printf("%d:\t%s\t", temp->pid, temp->processname);
        for(i=1; i < temp->arguements; i++){
            printf("%s ", temp->processargsv[i]);
        }		
        printf("\n");
        temp = temp->next;
        jobs++;
    }
    printf("Total Background jobs:\t%d\n", jobs);
}

/*
    get_prompt creates a prompt for the RSI out of the current directory
    and RSI: "CWD" >. Dynamically allocates space for cwd doubling if size is
    filled. Allows for use on file systems without PATH_MAX limits.
*/
char *get_prompt(){
    int path_size = INITIAL_PATH_SIZE;
    char *current_directory = try_malloc(sizeof(char) * path_size);
    char *prompt = NULL;
    char *prompt1 = "RSI: ";
    char *prompt2 = " >";

    while(getcwd(current_directory, path_size) == NULL){
        path_size *= 2;
        current_directory = realloc(current_directory, sizeof(char) * path_size);
            
        if(current_directory == NULL){
            perror("Error allocating memory");
            exit(1);
        }
    }
    
    prompt = try_malloc(strlen(current_directory)+strlen(prompt1)+strlen(prompt2)+1);
    strcpy(prompt, prompt1);
    strcat(prompt, current_directory);
    strcat(prompt, prompt2);    

    free(current_directory);
    current_directory = NULL;            

    return prompt;
}

/*
    parse_command takes a string array and places each arguement into one
    element. It does so doubling the size when it fills up and 
    dynamically allocating space for each string which means every arguement
    can be as long as the user likes.
*/
int parse_command(char *command, char ***p){ 
    char *token = NULL;
    int command_array_size = INITIAL_COMMAND_BUFFER_SIZE;
    int arguements = 0;   
    char **command_array = try_malloc(sizeof(char*) * command_array_size);

    token = strtok(command, " \t\n");

    while(token != NULL){  
        if(arguements == command_array_size - 1){
            command_array_size *= 2;
            command_array = realloc(command_array, sizeof(char*) * command_array_size);
            if(command_array == NULL){
                perror("Error allocating memory");
                exit(1);
            }
        }
        command_array[arguements] = try_malloc((strlen(token)+1) * sizeof(char));    

        strcpy(command_array[arguements], token);

        arguements++;
        token = strtok(NULL, " \t\n");
    }
    command_array[arguements] = NULL;
    *p = command_array;
    return arguements;
}

/*
    command_arbitrary takes the user provided arguements and searches the path
    for a binary with the same name. If fork() or execv() has an error it will 
    print to stderr.
*/
void command_arbitrary(char **command_array, int arguements, PROCESS **head){
    char **commands;
    int background = 0;
    if(strcmp(command_array[0], "bg") == 0){
        commands = command_array + 1;
        background = 1;   
    }else if(strcmp(command_array[0], "bglist") == 0){
        print_process_list(*head);
        return;
    }else{
        commands = command_array;
        background = 0;
    }    
    pid_t pid = fork();    
    if(pid == 0){
    if(execvp(commands[0], commands) == -1){
            perror("Error on execv");	
            exit(1);
    }else if(pid < 0){
        perror("Failed to fork process");
    }else if(background == 1){
        add_process(head, pid, commands, arguements-1);   
    }
	if(background == 0){
        wait(NULL);
    }
    return;
}

/*
    command_change_directory prints errors for more than 2 arguements, invalid 
    path and invalid HOME variable. Uses chdir() to change cwd
*/
void command_change_directory(char **command_array, int arguements){
    if(arguements > 2){
        fprintf(stderr, "cd:    usage:  cd [dir]\n");
    }else if(arguements == 2){
        if(strcmp(command_array[1], "~") == 0){
            char *home = getenv("HOME");

            if(chdir(home) == 0){
                return;
            }else{
                fprintf(stderr, "Could not change directory HOME environment variable error\n");
            }
        }else{           
            if(chdir(command_array[1]) == 0){
                return;
            }else{
                fprintf(stderr, "Could not change directory invalid path\n");
            }
        }        
    }else if(arguements == 1){
        char *home = getenv("HOME");
	    if(chdir(home) == 0){
            return;
        }else{
            fprintf(stderr, "Could not change directory HOME environment variable error\n");
        }
    }
    return;
}

/*
    child_sig_handler asynchronously handles child process termination and
    deletes the process from the background process list.
*/
void child_sig_handler(int signal_number){
    pid_t pid;	
    while((pid = waitpid((pid_t)(-1), NULL, WNOHANG)) > 0){
        printf("pid = %d\n", pid);
    }
}

int main(void){
    /*setup signal handler*/
    struct sigaction sigchld_action; 
    memset (&sigchld_action, 0, sizeof (sigchld_action)); 
    sigchld_action.sa_handler = &child_sig_handler; 
    sigaction (SIGCHLD, &sigchld_action, NULL); 
    
    int active = 1;
    int i = 0;  
    int arguements = 0;  
    char *command = NULL;
    char *prompt = NULL;
    char **command_array = NULL;
    PROCESS *head = NULL;
    while(active){
        prompt = get_prompt();
        command = readline(prompt);

        free(prompt);
        prompt = NULL;        

        if(strcmp(command, "quit") == 0){
            printf("RSI: exit success");			
            active = 0;
            break;
        }  
        arguements = parse_command(command, &command_array);

        free(command);
        command = NULL;

        if(strcmp(command_array[0], "cd") == 0){
            command_change_directory(command_array, arguements);
        }else{
            command_arbitrary(command_array, arguements, &head);
        }

        for(i=0; i<arguements; i++){
            free(command_array[i]);
            command_array[i] = NULL;
        }
        free(command_array);
        command_array = NULL;
    }    
    printf("\n");
    return 0;
}
//TODO FREE LL on program exit, signal handler, give a user a message that child terminated and remove from linked list (including free)