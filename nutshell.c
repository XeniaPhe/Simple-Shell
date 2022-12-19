#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>

#define MAX_LINE 80
#define LEFT_REDIRECTION 0
#define LEFT_REDIRECTION_APPEND 1
#define RIGHT_REDIRECTION 2
#define RIGHT_REDIRECTION_APPEND 3

int read_input(FILE *fp, size_t size,char **input);
int parse_input(char *input,int length ,char *args[]);
void execute_command(char *args[],int background);
int exists(char* directory, char *program);
void exec_history(int index);
void update_history(char* inputBuffer, int length);
void catch_fgkill(int sig_num);
void add_process(pid_t process);
int remove_process(pid_t process);
void check_bgprocesses();
void move_bgtofg(pid_t pid);
void execute_program(char* program,char* args[], int background);
void execute_programs(char** args[], int redirectionCount, int background);

struct history
{
    char* log[10];
    int counter;
} history;

struct bg_process
{
    struct bg_process* next;
    pid_t pid;
} typedef bg_process;

bg_process* head = NULL;
pid_t foregroundProcess = -1;

int main(void){
    signal(SIGTSTP, catch_fgkill);

    char *inputBuffer;
    char *args[MAX_LINE/2 + 1];

    while (1){
        printf("nutshell>> ");
        int length = read_input(stdin,MAX_LINE,&inputBuffer);
        if(length == 0)
            exit(0);

        if(strncmp(inputBuffer,"\n",1) == 0){
            free(inputBuffer);
            continue;
        }
        
        update_history(inputBuffer, length);
        int background = parse_input(inputBuffer,length ,args);
        check_bgprocesses();
        execute_command(args,background);
        free(inputBuffer);
    }
}

int read_input(FILE *fp, size_t size,char **input){
    int ch;
    size_t len = 0;
    *input = realloc(NULL, sizeof(char)*size);

    if(!*input)
        return 0;

    while((ch=fgetc(fp)) != EOF){
        (*input)[len++]= ch;

        if(len==size){
            *input = realloc(*input, sizeof(char)*(size+=16));

            if(!*input)
                return len;
        }

        if(ch == '\n'){
            (*input)[len++] = '\0';
            break;
        }
    }

    *input = realloc(*input, sizeof(*input)*len);
    return len;
}

int parse_input(char *input,int length ,char *args[]){
    int i,      /* loop index for accessing inputBuffer array */
        start,  /* index where beginning of next command parameter is */
        ct,     /* index of where to place the next parameter into args[] */
        bg;
    
    ct = 0;
    bg = 0;

    start = -1;

    /* the signal interrupted the read system call */
    /* if the process is in the read() system call, read returns -1
    However, if this occurs, errno is set to EINTR. We can check this  value
    and disregard the -1 value */

    if ( (length < 0) && (errno != EINTR) ) {
        fprintf(stderr,"Error reading the command\n");
	    exit(-1); /* terminate with error code of -1 */
    }

    for (i=0;i<length;i++){ /* examine every character in the input */

        switch (input[i]){
	        case ' ':
	        case '\t' : /* argument separators */
		        if(start != -1)
                    args[ct++] = &input[start]; /* set up pointer */

                input[i] = '\0'; /* add a null char; make a C string */
		        start = -1;
		    break;
            case '\n': /* should be the final char examined */
		        if (start != -1)
                    args[ct++] = &input[start];     

                input[i] = '\0';
                args[ct] = NULL; /* no more arguments to this command */
		    break;
	        default : /* some other character */
                if (input[i] == '&'){
		            bg = 1;
                    
                    if(start == -1){
                        args[ct] = NULL;
                        break;
                    }

                    input[i] = '\0'; //the solution might be related to this
		        }

                if (start == -1)
		            start = i;
            break;
	    }
    }

    args[ct] = NULL; /* just in case the input line was > 80 */
    return bg;
}

void execute_command(char* args[], int background){
    char* program = args[0];

    if(program == NULL)
        return;

    if(strncmp(program,"exit",4) == 0){
        if(head != NULL){
            fprintf(stderr,"There is at least one background process still running!\n");
            return;
        }

        exit(0);
    }

    if(strncmp(program,"fg",2) == 0){
        char *pidArg = args[1];

        if(pidArg == NULL || pidArg[1] == '\0'){
            fprintf(stderr,"You must input a valid pid following a %% sign!\n");
            return;
        }

        pid_t pid = atoi(&pidArg[1]);

        if(pid == 0){
            fprintf(stderr,"You must input a valid pid following a %% sign!\n");
            return;
        }

        move_bgtofg(pid);
        return;
    }
    
    if(strncmp(program,"history",7) == 0){
        int number = -1;
        char* i = args[1];
        if(i != NULL && strncmp(i,"-i",2) == 0){
            char *numberArg = args[2];
            
            if(numberArg == NULL){
                fprintf(stderr,"You must input a number!\n");
                return;
            }

            char ch = numberArg[0];
            

            if(ch <= '9' && ch >= '0' && numberArg[1] == '\0'){
                number = ch - '0';
            }
            else{
                fprintf(stderr,"Invalid index for history!\n");
                return;
            }
        }

        exec_history(number);
        return;
    }
    
    //check if there is any redirection

    char*** args_separated = (char***)malloc(0);
    int* redirections = (int*)malloc(0);

    int counter = 0;
    int start = 0;
    int checker = 0;

    for(int i = 0; i < 41; i++){
        char* arg = args[i];

        if(arg == NULL){
            if(checker){
                fprintf(stderr,"Invalid command!\n");
                free(args_separated);
                free(redirections);
                return;
            }

            break;
        }

        int redirection = -1;

        if(strcmp(arg,"<") == 0)
            redirection = LEFT_REDIRECTION;
        else if(strcmp(arg, "<<") == 0)
            redirection = LEFT_REDIRECTION_APPEND;
        else if(strcmp(arg,">") == 0)
            redirection = RIGHT_REDIRECTION;
        else if(strcmp(arg,">>") == 0)
            redirection = RIGHT_REDIRECTION_APPEND;
        
        if(redirection != -1){
            if(checker){
                fprintf(stderr,"Invalid command!\n");
                free(args_separated);
                free(redirections);
                return;
            }

            counter++;
            checker = 1;
            args[i] = NULL;

            args_separated = (char***)realloc(args_separated,sizeof(char**) * (counter + 1));
            redirections = (int*)realloc(redirections,sizeof(int) * counter);

            args_separated[counter - 1] = &args[start++];
            redirections[counter - 1] = redirection;
            continue;
        }

        checker = 0;
    }

    if(checker){
        fprintf(stderr,"Invalid command!\n");
        free(args_separated);
        free(redirections);
        return;
    }

    if(counter == 0){
        execute_program(program,args,background);
        return;
    }

    execute_programs(args_separated,counter,background);
}

int exists(char* directory, char* program){
    DIR *d;
    struct dirent *dir;
    d = opendir(directory);
    if(d == NULL)
        return 0;
    while ((dir = readdir(d)) != NULL)
        if(strcmp(dir->d_name,program) == 0)
            return 1;

    closedir(d);
    return 0;
}

void exec_history(int index){
    if(index == -1){
        for(int i = 0; i < 10; i++){
            index = (history.counter - i - 1) % 10;
            char* log = history.log[index];
        
            if(log == NULL)
                break;

            printf("%d %s",i,log);
        }

        return;
    }

    char *args[MAX_LINE/2 + 1];

    index = (history.counter - index - 1) % 10;
    char* command = history.log[index];

    int length = strlen(command);
    update_history(command, length);
    int background = parse_input(command,length ,args);
    execute_command(args,background);
}

void update_history(char* inputBuffer, int length){    
    int index = (++history.counter) % 10;
    free(history.log[index]);
    char* copy = (char*)malloc(sizeof(char) * length);
    strcpy(copy,inputBuffer);
    history.log[index] = copy;
}

void catch_fgkill(int sig_num){
    signal(SIGTSTP, catch_fgkill);

    if(foregroundProcess == -1)
        return;

    kill(foregroundProcess,SIGKILL);
}

void add_process(pid_t process){
    bg_process* newprocess = (bg_process*)malloc(sizeof(bg_process));
    newprocess->pid = process;

    if(head == NULL){
        head = newprocess;
        return;
    }

    bg_process* walker = head;

    while(walker->next != NULL)
        walker = walker->next;

    walker->next = newprocess;
}

int remove_process(pid_t process){
    if(head == NULL)
        return 0;

    bg_process* prev = NULL;
    bg_process* curr = head;
    
    while(curr->pid != process){
        prev = curr;
        curr = curr->next;

        if(curr == NULL)
            return 0;
    }
    
    if(!prev)
        head = curr->next;
    else
        prev->next = curr->next;

    free(curr);
    return 1;
}

void check_bgprocesses(){
    bg_process* walker = head;

    while(walker != NULL){
        pid_t pid = walker->pid;
        pid_t return_pid = waitpid(pid,NULL,WNOHANG);

        walker = walker->next;

        if(pid == return_pid)
            remove_process(pid);
    }
}

void move_bgtofg(pid_t pid){
    if(remove_process(pid) == 0)
        return;

    foregroundProcess = pid;

    if (kill(pid, SIGCONT) < 0) 
        perror("");

    if (waitpid(pid, NULL, WUNTRACED) < 0)
        perror("");
}

void execute_program(char* program,char* args[], int background){
    const char* path = getenv("PATH");
    char* copy = (char*)malloc(strlen(path) + 1);
    strcpy(copy,path);

    char* token = strtok(copy,":");

    char* programPath = NULL;

    do
    {
        if(exists(token,program)){
            programPath = token;
            strcat(programPath,"/");
            strcat(programPath,program);
            break;
        }

    } while ((token = strtok(NULL,":")) != NULL);

    free(copy);
    
    if(programPath == NULL){
        fprintf(stderr,"Command not found!\n");
        return;
    }

    pid_t childpid = fork();

    if(childpid == 0){
        execv(programPath,args);
        fprintf(stderr,"Error executing the program\n");
    }

    int options = 0;

    if(background){
        printf("[%d]\n",childpid);

        add_process(childpid);
        foregroundProcess = -1;
        options |= WNOHANG;
    }
    else{
        foregroundProcess = childpid;
    }

    waitpid(childpid,NULL,options);
}

void execute_programs(char** args[], int redirectionCount, int background){
    printf("test\n");
}