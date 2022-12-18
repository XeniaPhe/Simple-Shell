#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>

#define MAX_LINE 80

void executeCommand(char *args[]);
int exists(char* directory, char *program);

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

void parseinput(char *input,int length ,char *args[],int *background)
{
    int i,      /* loop index for accessing inputBuffer array */
        start,  /* index where beginning of next command parameter is */
        ct;     /* index of where to place the next parameter into args[] */
    
    ct = 0;

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
		        if(start != -1){
                    args[ct] = &input[start]; /* set up pointer */

		            ct++;
		        }

                input[i] = '\0'; /* add a null char; make a C string */
		        start = -1;
		    break;
            case '\n': /* should be the final char examined */
		        if (start != -1){
                    args[ct] = &input[start];     

		            ct++;
		        }

                input[i] = '\0';
                args[ct] = NULL; /* no more arguments to this command */
		    break;
	        default : /* some other character */
		        if (start == -1)
		            start = i;

                if (input[i] == '&'){
		            *background  = 1;

                    input[i-1] = '\0';
		        }
	    }
    }

    args[ct] = NULL; /* just in case the input line was > 80 */
}
 
int main(void)
{
    char *inputBuffer;
    int background;
    char *args[MAX_LINE/2 + 1];

    while (1){
        background = 0;
        printf("nutshell>> ");
        int length = readinput(stdin,MAX_LINE,&inputBuffer);
        parseinput(inputBuffer,length ,args, &background);
        executeCommand(args);

        /** the steps are:
        (1) fork a child process using fork()
        (2) the child process will invoke execv()
		(3) if background == 0, the parent will wait,
        otherwise it will invoke the setup() function again. */
    }
}


void executeCommand(char* args[]){
    char* program = args[0];

    if(program == NULL)
        return;

    const char* path = getenv("PATH");
    char* copy = (char*)malloc(strlen(path) + 1);
    strcpy(copy,path);

    char* token = strtok(copy,":");

    char* programPath;
    
    do
    {
        if(exists(token,program)){
            printf("%s/%s\n",token,program);
        }

    } while ((token = strtok(NULL,":")) != NULL);

    free(copy);
    
    if(programPath == NULL)
        fprintf(stdin,"Command not found!");
    //else
        //printf("%s",programPath);
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