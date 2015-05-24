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
    int arguments;
    char *processname;
    char **processargsv;
    struct process *next;
}PROCESS;

PROCESS *head = NULL;

void delete_process(pid_t pid);
void add_process(pid_t pid, char **commands, int arguments);
void print_process_list();
void delete_list();

void *try_malloc(int size);
char *get_prompt();
int parse_command(char *command, char ***p);
void command_arbitrary(char **command_array, int arguments);
void command_change_directory(char **command_array, int arguments);
void child_sig_handler(int signal_number);
void command_signal_process(char **command_array, int arguments, int signum);

/*
    try_malloc, helpful function for mallocing with error checking
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
    delete_process is called by SGCHLD signal handler asynchronously. Is called 
    when a child terminates. This deletes the PID from the background list and
    frees the memory associated with it.
*/
void delete_process(pid_t pid){
    PROCESS *temp = head;
    PROCESS *prev = head;
    int i;
    while(temp != NULL){
        if(pid == temp->pid){
            printf("%d:\t%s\t", temp->pid, temp->processname);
            for(i=1; i < temp->arguments; i++){
                printf("%s ", temp->processargsv[i]);
            }		
            printf(" has terminated.\n");
            if(temp == head){
                free(head);
                head = NULL;
                break;
            }else{
                prev->next = temp->next;
                free(temp);
                temp = NULL;
                break;
            }
        }
        prev = temp;
        temp = temp->next;
    }   
    return;
}

/* 
    add_process adds a process to the process linked list. Adds it to the head 
    if null or iterates to the end of the list and adds it. A process stores 
    information about PID, executable name, and arguments passed.
*/
void add_process(pid_t pid, char **commands, int arguments){
    int i = 0;	
    if(head == NULL){	
        head = try_malloc(sizeof(PROCESS));
        head->pid = pid;
        head->arguments = arguments;
        
        head->processname = try_malloc(sizeof(commands[0]));
        strcpy(head->processname, commands[0]);
        
        head->processargsv = try_malloc(sizeof(commands));		
        for(i=0; i<arguments; i++){
            head->processargsv[i] = try_malloc(sizeof(commands[i]));
            strcpy(head->processargsv[i], commands[i]);
        }
        head->next = NULL;
    }else{
        PROCESS *temp = try_malloc(sizeof(PROCESS));
        temp->pid = pid;
        temp->arguments = arguments;
        
        temp->processname = try_malloc(sizeof(commands[0]));
        strcpy(temp->processname, commands[0]);
        
        temp->processargsv = try_malloc(sizeof(commands));		
        for(i=0; i<arguments; i++){
            temp->processargsv[i] = try_malloc(sizeof(commands[i]));
            strcpy(temp->processargsv[i], commands[i]);
        }
        temp->next = NULL;

        PROCESS *p = head;
        while(p->next != NULL){
            p=p->next;
        }
        p->next = temp;
    }
    return;
}

/*
    print_process_list prints a formatted list of proesses running in the 
    background. Lists paused processes too.
*/
void print_process_list(){
    PROCESS *temp = head;
    int i = 0;
    int jobs = 0;
    printf("PID:\tcommand\targuments\n");
    while(temp != NULL){
        printf("%d:\t%s\t", temp->pid, temp->processname);
        for(i=1; i < temp->arguments; i++){
            printf("%s ", temp->processargsv[i]);
        }		
        printf("\n");
        temp = temp->next;
        jobs++;
    }
    printf("Total Background jobs:\t%d\n", jobs);
    return;
}

/*
    delete_list head pointer iterates through the list freeing each element
    ending when head = NULL. This is ran at the end of the program to free 
    up memory.
*/
void delete_list(){
    PROCESS *temp;
    int result = 0;
    while(head != NULL){
        temp = head;
        head = head->next;
        /*kills all background processes on quitting the shell*/
        result = kill(temp->pid, SIGKILL);        
        if(result == -1){
            perror("Error sending signal to process");
        }
        free(temp);
        temp = NULL;
    }    
    return;
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
    can be as long as the user likes and can accept as many arguments as the
    user allows.
*/
int parse_command(char *command, char ***p){ 
    char *token = NULL;
    int command_array_size = INITIAL_COMMAND_BUFFER_SIZE;
    int arguments = 0;   
    char **command_array = try_malloc(sizeof(char*) * command_array_size);

    token = strtok(command, " \t\n");

    while(token != NULL){  
        if(arguments == command_array_size - 1){
            command_array_size *= 2;
            command_array = realloc(command_array, sizeof(char*) * command_array_size);
            if(command_array == NULL){
                perror("Error allocating memory");
                exit(1);
            }
        }
        command_array[arguments] = try_malloc((strlen(token)+1) * sizeof(char));    

        strcpy(command_array[arguments], token);

        arguments++;
        token = strtok(NULL, " \t\n");
    }
    command_array[arguments] = NULL;
    *p = command_array;
    return arguments;
}

/*
    command_arbitrary takes the user provided arguments and searches the path
    for a binary with the same name. If fork() or execv() has an error it will 
    print to stderr. Also handles background execution using a background flag.
*/
void command_arbitrary(char **command_array, int arguments){
    char **commands;
    int background = 0;
    if(strcmp(command_array[0], "bg") == 0){
        if(arguments > 1){
            commands = command_array + 1;
            background = 1;   
        }else{
            fprintf(stderr, "bg:    usage:  bg [command]\n");
        }
    }else if(strcmp(command_array[0], "bglist") == 0){
        print_process_list();
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
        }
    }else if(pid < 0){
        perror("Failed to fork process");
    }else if(background == 1){
        add_process(pid, commands, arguments-1);   
    }
	if(background == 0){
        wait(NULL);
    }
    return;
}

/*
    command_change_directory prints errors for more than 2 arguments, invalid 
    path and invalid HOME variable. Uses chdir() to change cwd to the path 
    given. 
*/
void command_change_directory(char **command_array, int arguments){
    if(arguments > 2){
        fprintf(stderr, "cd:    usage:  cd [dir]\n");
    }else if(arguments == 2){
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
    }else if(arguments == 1){
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
    command_signal_process sends the signal provided (KILL/STOP/CONT) to the
    child process with PID provided. Also prevents user from affecting the 
    shell
*/
void command_signal_process(char **command_array, int arguments, int signum){
    if(arguments == 2){
        pid_t pid = (pid_t)strtol(command_array[1], NULL, 10);
        if(pid == 0){
            fprintf(stderr, "[SIGNAL]: usage: [kill|pause|resume] [PID]\n");
        }else{      
            int result = kill(pid, signum);        
            if(result == -1){
                perror("Error sending signal to process");
            }
        }
    }else{
        fprintf(stderr, "kill:  usage:  kill [PID]\n");
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
        delete_process(pid);
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
    int arguments = 0;  
    char *command = NULL;
    char *prompt = NULL;
    char **command_array = NULL;
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
	
        arguments = parse_command(command, &command_array);

        free(command);
        command = NULL;

        if(arguments > 0){
            if(strcmp(command_array[0], "cd") == 0){
                command_change_directory(command_array, arguments);
            }else if(strcmp(command_array[0], "kill") == 0){
                command_signal_process(command_array, arguments, SIGKILL);
            }else if(strcmp(command_array[0], "pause") == 0){
                command_signal_process(command_array, arguments, SIGSTOP);
            }else if (strcmp(command_array[0], "resume") == 0){
                command_signal_process(command_array, arguments, SIGCONT);
            }else{
                command_arbitrary(command_array, arguments);
            }
        }

        for(i=0; i<arguments; i++){
            free(command_array[i]);
            command_array[i] = NULL;
        }
        free(command_array);
        command_array = NULL;
    }  
    delete_list();  
    printf("\n");
    return 0;
}
