#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>

#define MAX_LINE 80 /* 80 chars per line, per command, should be enough. */
 
/* The setup function below will not return any value, but it will just: read
in the next command line; separate it into distinct arguments (using blanks as
delimiters), and set the args array entries to point to the beginning of what
will become null-terminated, C-style strings. */

void executeCommand(char* args[]);
int exists(char* directory, char* program);

char *readinput(FILE* fp, size_t size){
//The size is extended by the input with the value of the provisional
    char *str;
    int ch;
    size_t len = 0;
    str = realloc(NULL, sizeof(*str)*size);//size is start size
    if(!str)return str;
    while(EOF!=(ch=fgetc(fp)) && ch != '\n'){
        str[len++]=ch;
        if(len==size){
            str = realloc(str, sizeof(*str)*(size+=16));
            if(!str)return str;
        }
    }
    str[len++]='\0';

    return realloc(str, sizeof(*str)*len);
}

void setup(char inputBuffer[], char *args[],int *background)
{
    int length, /* # of characters in the command line */
        i,      /* loop index for accessing inputBuffer array */
        start,  /* index where beginning of next command parameter is */
        ct;     /* index of where to place the next parameter into args[] */
    
    ct = 0;
        
    /* read what the user enters on the command line */
    //length = read(STDIN_FILENO,inputBuffer,MAX_LINE);
    inputBuffer = readinput(stdin,MAX_LINE);
    length = strlen(inputBuffer) + 1;
    strcat(inputBuffer,"\n");

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

	//printf(" %s\n",inputBuffer);
    for (i=0;i<length;i++){ /* examine every character in the inputBuffer */

        switch (inputBuffer[i]){
	        case ' ':
	        case '\t' : /* argument separators */
		        if(start != -1){
                    args[ct] = &inputBuffer[start]; /* set up pointer */

		            ct++;
		        }

                inputBuffer[i] = '\0'; /* add a null char; make a C string */
		        start = -1;
		    break;
            case '\n': /* should be the final char examined */
		        if (start != -1){
                    args[ct] = &inputBuffer[start];     

		            ct++;
		        }

                inputBuffer[i] = '\0';
                args[ct] = NULL; /* no more arguments to this command */
		    break;
	        default : /* some other character */
		        if (start == -1)
		            start = i;

                if (inputBuffer[i] == '&'){
		            *background  = 1;

                    inputBuffer[i-1] = '\0';
		        }
	    }
    }

    args[ct] = NULL; /* just in case the input line was > 80 */

	//for (i = 0; i < ct; i++)
	//	printf("args %d = %s\n",i,args[i]);
}
 
int main(void)
{
    char inputBuffer[MAX_LINE]; /*buffer to hold command entered */
    int background; /* equals 1 if a command is followed by '&' */
    char *args[MAX_LINE/2 + 1]; /*command line arguments */

    while (1){
        background = 0;
        printf("nutshell>> ");
        /*setup() calls exit() when Control-D is entered */
        setup(inputBuffer, args, &background);
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

    //note :creating strings as char pointers inside a function is bad

    char* path = getenv("PATH");
    char* programPath;

    char* token = strtok(path,":");

    do
    {
        //printf("%s\n",token);
        if(exists(token,program)){
            printf("Fucking exists!!!");
        }

    } while ((token = strtok(NULL,":")) != NULL);
    
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