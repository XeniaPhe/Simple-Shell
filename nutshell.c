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


#define MAX_LINE 80

int readinput(FILE *fp, size_t size,char **input);
int parseinput(char *input,int length ,char *args[]);
void executeCommand(char *args[],int background);
int exists(char* directory, char *program);
void exec_history(int index);
void update_history(char* inputBuffer, int length);

struct history
{
    char* log[10];
    int counter;
} history;

int main(void){
    char *inputBuffer;
    char *args[MAX_LINE/2 + 1];

    while (1){
        printf("nutshell>> ");
        int length = readinput(stdin,MAX_LINE,&inputBuffer);
        update_history(inputBuffer, length);
        int background = parseinput(inputBuffer,length ,args);
        executeCommand(args,background);
        free(inputBuffer);
    }
}

int readinput(FILE *fp, size_t size,char **input){
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

int parseinput(char *input,int length ,char *args[]){
    int i,      /* loop index for accessing inputBuffer array */
        start,  /* index where beginning of next command parameter is */
        ct,     /* index of where to place the next parameter into args[] */
        bg;
    
    ct = 0;
    bg = 0;

    /* 0 is the system predefined file descriptor for stdin (standard input),
       which is the user's screen in this case. inputBuffer by itself is the
       same as &inputBuffer[0], i.e. the starting address of where to store
       the command that is read, and length holds the number of characters
       read in. inputBuffer is not a null terminated C-string. */

    start = -1;
    if (length == 0)
        exit(0);            /* ^d was entered, end of user command stream */

    /* the signal interrupted the read system call */
    /* if the process is in the read() system call, read returns -1
    However, if this occurs, errno is set to EINTR. We can check this  value
    and disregard the -1 value */

    if ( (length < 0) && (errno != EINTR) ) {
        perror("error reading the command");
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

void executeCommand(char* args[], int background){
    char* program = args[0];

    if(program == NULL)
        return;

    if(strncmp(program,"history",7) == 0){
        int number = -1;

        if(args[1] != NULL && strncmp(args[1],"-i",2) == 0){
            char *numberArg = args[2];
            
            if(numberArg == NULL){
                perror("You must input a number!");
                return;
            }

            char ch = numberArg[0];
            

            if(ch <= '9' && ch >= '0' && numberArg[1] == '\0'){
                number = ch - '0';
            }
            else{
                perror("Invalid index for history!");
                return;
            }
        }

        exec_history(number);
        return;
    }

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
        perror("Error executing the program");
    }

    int options = 0;

    if(background){
        printf("[%d]\n",childpid);
        options |= WNOHANG;
    }

    waitpid(childpid,NULL,options);
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
    int background = parseinput(command,length ,args);
    executeCommand(args,background);
}

void update_history(char* inputBuffer, int length){
    int index = (++history.counter) % 10;
    free(history.log[index]);
    char* copy = (char*)malloc(sizeof(char) * length);
    strcpy(copy,inputBuffer);
    history.log[index] = copy;
}