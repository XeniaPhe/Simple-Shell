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
void add_bgprocess(pid_t process);
int remove_process(pid_t process);
void check_bgprocesses();
void move_bgtofg(pid_t pid);
void execute_program(char* program,char* args[], int background);
void execute_programs(char*** args_separated, int* redirections,int redirectionCount, int background);

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

    // Run the shell indefinitely
    while (1){
        printf("nutshell>> ");
        // Read user input from stdin
        int length = read_input(stdin,MAX_LINE,&inputBuffer);
        // If the user entered an empty line, exit the shell
        if(length == 0)
            exit(0);
        // If the user entered only a newline, continue to the next iteration
        if(strncmp(inputBuffer,"\n",1) == 0){
            free(inputBuffer);
            continue;
        }
        // Add the user input to the shell's history
        update_history(inputBuffer, length);
        // Parse the user input
        int background = parse_input(inputBuffer,length ,args);
        // Check for any finished background processes
        check_bgprocesses();
        // Execute the command specified by the user input
        execute_command(args,background);
        // Free the memory allocated for the user input
        free(inputBuffer);
    }
}

// read_input reads a line from the specified file and stores it in the input buffer
// fp: pointer to the input file
// size: initial size of the input buffer
// input: pointer to the input buffer
int read_input(FILE *fp, size_t size, char **input) {
  // ch: character read from the file
  int ch;

  // len: length of the input string
  size_t len = 0;

  // allocate memory for the input buffer
  *input = realloc(NULL, sizeof(char) * size);

  // return 0 if memory allocation fails
  if (!*input) return 0;

  // read characters from the file until end of file or newline is reached
  while ((ch = fgetc(fp)) != EOF) {
    // store the character in the input buffer
    (*input)[len++] = ch;

    // increase the size of the input buffer if it is full
    if (len == size) {
      *input = realloc(*input, sizeof(char) * (size += 16));

      // return len if memory allocation fails
      if (!*input) return len;
    }

    // break the loop if a newline is encountered
    if (ch == '\n') {
      // add a null terminator to the input string
      (*input)[len++] = '\0';
      break;
    }
  }

  // shrink the size of the input buffer to the actual size of the input string
  *input = realloc(*input, sizeof(*input) * len);

  // return the length of the input string
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

    // Get the program name (first argument in the array).
    char* program = args[0];

    // If there is no program specified, return.
    if(program == NULL)
        return;

    // Check if the program is "exit". If it is, exit the program.
    // If there are any background processes still running, print an error message and return.
    if(strncmp(program,"exit",4) == 0){
        if(head != NULL){
            fprintf(stderr,"There is at least one background process still running!\n");
            return;
        }
        exit(0);
    }

    // Check if the program is "fg". If it is, move the specified background process to the foreground.
    // If the pid is not specified or is invalid, print an error message and return.
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

    // Check if the program is "history". If it is, execute the history command.
    // If the "-i" flag is specified, print the command with the specified index.
    // If the index is not specified or is invalid, print an error message and return.
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

    // Allocate memory for arrays to hold the arguments and redirections for each command.
    char*** args_separated = (char***)malloc(0);
    int* redirections = (int*)malloc(0);

    // Initialize variables to keep track of the current command being processed.
    int counter = 0;
    int start = 0;
    int checker = 0;

    // Iterate through the args array, looking for redirections.
    for(int i = 0; i < 41; i++){
        char* arg = args[i];
        // If we reach the end of the args array, break out of the loop.
        // If the checker variable is set, this means there was an invalid command, so print an error message and return.
        if(arg == NULL){
            if(checker){
                fprintf(stderr,"Invalid command!\n");
                free(args_separated);
                free(redirections);
                return;
            }
            break;
        }
        // Check if the current argument is a redirection operator.
        // If it is, set the redirection variable to the appropriate value.
        int redirection = -1;
        if(strcmp(arg,"<") == 0)
            redirection = LEFT_REDIRECTION;
        else if(strcmp(arg, "<<") == 0)
            redirection = LEFT_REDIRECTION_APPEND;
        else if(strcmp(arg,">") == 0)
            redirection = RIGHT_REDIRECTION;
        else if(strcmp(arg,">>") == 0)
            redirection = RIGHT_REDIRECTION_APPEND;
        
        // If the current argument is a redirection operator, set the checker variable and add the current command
        // to the args_separated and redirections arrays.
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
        // If the current argument is not a redirection operator, reset the checker variable.
        checker = 0;
    }

    // If the checker variable is still set after processing all of the arguments,
    // this means there was an invalid command, so print an error message and return.
    if(checker){
        fprintf(stderr,"Invalid command!\n");
        free(args_separated);
        free(redirections);
        return;
    }

    // If there are no redirections in the command, execute the program.
    if(counter == 0){
        execute_program(program,args,background);
        return;
    }

    // If there are redirections in the command, execute each command in the args_separated array
    // with the corresponding redirection in the redirections array.
    execute_programs(args_separated,redirections,counter,background);
}

int exists(char* directory, char* program){
    // Open the directory specified by the input directory argument
    DIR *d;
    struct dirent *dir;
    d = opendir(directory);
    // Return 0 if the directory cannot be opened
    if(d == NULL)
        return 0;
    // Iterate through the directory contents
    while ((dir = readdir(d)) != NULL)
        // Return 1 if a file with the same name as the input program argument is found
        if(strcmp(dir->d_name,program) == 0)
            return 1;

    // Close the directory and return 0 if no file with the same name as the input program argument is found
    closedir(d);
    return 0;
}

void exec_history(int index){
    // If the index argument is -1, print the last 10 commands in the history log
    if(index == -1){
        for(int i = 0; i < 10; i++){
            // Calculate the index of the current command in the history log
            index = (history.counter - i - 1) % 10;
            char* log = history.log[index];
        
            // Break the loop if the current command is NULL
            if(log == NULL)
                break;

            printf("%d %s",i,log);
        }

        return;
    }

    char *args[MAX_LINE/2 + 1];

    // Calculate the index of the command to be executed in the history log
    index = (history.counter - index - 1) % 10;
    char* command = history.log[index];

    // Parse the command into arguments
    int length = strlen(command);
    update_history(command, length);
    int background = parse_input(command,length ,args);
    // Execute the command
    execute_command(args,background);
}

void update_history(char* inputBuffer, int length){    
    // Increment the history counter and calculate the index for the new entry
    int index = (++history.counter) % 10;

    // Free the memory for the previous entry at this index
    free(history.log[index]);

    // Allocate memory for the new entry and copy the input buffer into it
    char* copy = (char*)malloc(sizeof(char) * length);
    strcpy(copy,inputBuffer);

    // Store the copy in the history log
    history.log[index] = copy;
}

void catch_fgkill(int sig_num){
    // Re-register the signal handler for SIGTSTP
    signal(SIGTSTP, catch_fgkill);

    // If there is no foreground process, return early
    if(foregroundProcess == -1)
        return;

    // Kill the foreground process
    kill(foregroundProcess,SIGKILL);
}

void add_bgprocess(pid_t process){
    // Allocate memory for the new background process
    bg_process* newprocess = (bg_process*)malloc(sizeof(bg_process));
    // Set the pid of the new process
    newprocess->pid = process;

    // If the linked list is empty, set the new process as the head of the linked list
    if(head == NULL){
        head = newprocess;
        return;
    }

    // Initialize a pointer to the head of the linked list
    bg_process* walker = head;

    // Iterate through the linked list until the end is reached
    while(walker->next != NULL)
        walker = walker->next;

    // Add the new process to the end of the linked list
    walker->next = newprocess;
}

int remove_process(pid_t process){
    // Return 0 if the linked list is empty
    if(head == NULL)
        return 0;

    // Initialize pointers to the previous and current nodes in the linked list
    bg_process* prev = NULL;
    bg_process* curr = head;
    
    // Iterate through the linked list until the node with the specified process pid is found
    while(curr->pid != process){
        // Update the previous node to be the current node
        prev = curr;
        // Move to the next node in the linked list
        curr = curr->next;

        // Return 0 if the end of the linked list is reached without finding the node with the specified process pid
        if(curr == NULL)
            return 0;
    }
    
    // Update the head of the linked list if the node to be removed is the head
    if(!prev)
        head = curr->next;
    // Update the next pointer of the previous node if the node to be removed is not the head
    else
        prev->next = curr->next;

    // Free the memory allocated for the node being removed
    free(curr);
    return 1;
}

void check_bgprocesses(){
    // Initialize a pointer to the head of the linked list of background processes
    bg_process* walker = head;

    // Iterate through the linked list
    while(walker != NULL){
        // Get the pid of the current background process
        pid_t pid = walker->pid;
        // Check if the process has terminated using waitpid
        pid_t return_pid = waitpid(pid,NULL,WNOHANG);

        // Move to the next node in the linked list
        walker = walker->next;

        // If the pid of the terminated process is returned by waitpid, remove the process from the linked list
        if(pid == return_pid)
            remove_process(pid);
    }
}

// This function moves a background process with the given process ID to the foreground
void move_bgtofg(pid_t pid){
    // If the process is not in the list of background processes, return
    if(remove_process(pid) == 0)
        return;

    // Set the foreground process ID to the given process ID
    foregroundProcess = pid;

    // Send the SIGCONT signal to the process to continue execution
    if (kill(pid, SIGCONT) < 0) 
        perror("");

    // Wait for the process to change state (e.g. terminated, stopped)
    if (waitpid(pid, NULL, WUNTRACED) < 0)
        perror("");
}

// This function executes a program with the given arguments, and has the option to run it in the background
void execute_program(char* program,char* args[], int background){
    // Get the PATH environment variable
    const char* path = getenv("PATH");
    // Make a copy of the PATH variable
    char* copy = (char*)malloc(strlen(path) + 1);
    strcpy(copy,path);

    // Tokenize the copy of the PATH variable by the ':' character
    char* token = strtok(copy,":");

    // Initialize a variable to store the full path of the program
    char* programPath = NULL;

    // Iterate through the tokenized PATH directories
    do
    {
        // Check if the program exists in the current directory
        if(exists(token,program)){
            // If it does, store the full path to the program
            programPath = token;
            strcat(programPath,"/");
            strcat(programPath,program);
            break;
        }

    } while ((token = strtok(NULL,":")) != NULL);

    // Free the copy of the PATH variable
    free(copy);
    
    // If the program was not found in any of the PATH directories, print an error message and return
    if(programPath == NULL){
        fprintf(stderr,"Command not found!\n");
        return;
    }

    // Fork the process
    pid_t childpid = fork();

    // If this is the child process
    if(childpid == 0){
        // Execute the program with the given arguments
        execv(programPath,args);
        // If execv returns, it means an error occurred
        fprintf(stderr,"Error executing the program\n");
    }

    int options = 0;

    // If the program should run in the background
    if(background){
        // Print the child process ID
        printf("[%d]\n",childpid);

        // Add the child process to the list of background processes
        add_bgprocess(childpid);
        // Set the foreground process ID to -1 (no foreground process)
        foregroundProcess = -1;
        // Set the options for waitpid to return immediately if the child process has already terminated
        options |= WNOHANG;
    }
    else{
        // Set the foreground process ID to the child process ID
        foregroundProcess = childpid;
    }

    // Wait for the child process to terminate
    waitpid(childpid,NULL,options);
}

void execute_programs(char*** args_separated, int* redirections,int redirectionCount, int background){
  int i, j;
  int fileDescriptor;
  int currentRedirection;
  pid_t pid;
  char** currentArgs;
  int inputRedirection = -1;
  int outputRedirection = -1;

  for(i = 0; i < redirectionCount + 1; i++){
    currentArgs = args_separated[i];
    currentRedirection = i < redirectionCount ? redirections[i] : -1;
    if(currentRedirection == LEFT_REDIRECTION || currentRedirection == LEFT_REDIRECTION_APPEND){
      // The first element of the currentArgs array is the file name for input redirection
      inputRedirection = open(currentArgs[0], currentRedirection == LEFT_REDIRECTION ? O_RDONLY : O_RDWR | O_CREAT | O_APPEND, 0644);
      if(inputRedirection < 0){
        fprintf(stderr, "Error opening file for input redirection: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
      }
      // Remove the file name from the currentArgs array and shift the remaining elements back
      char* fileName = currentArgs[0];
      for(j = 0; currentArgs[j+1] != NULL; j++)
        currentArgs[j] = currentArgs[j+1];
      currentArgs[j] = NULL;
      free(fileName);
    }
    else if(currentRedirection == RIGHT_REDIRECTION || currentRedirection == RIGHT_REDIRECTION_APPEND){
      // The first element of the currentArgs array is the file name for output redirection
        outputRedirection = open(currentArgs[0], currentRedirection == RIGHT_REDIRECTION ? O_WRONLY | O_TRUNC | O_CREAT : O_WRONLY | O_APPEND | O_CREAT, 0644);
    }
    else{
    if((pid = fork()) < 0){
      fprintf(stderr, "Error creating child process: %s\n", strerror(errno));
      exit(EXIT_FAILURE);
    }
    else if(pid == 0){
      if(inputRedirection != -1){
        if(dup2(inputRedirection, STDIN_FILENO) < 0){
          fprintf(stderr, "Error redirecting input: %s\n", strerror(errno));
          exit(EXIT_FAILURE);
        }
        close(inputRedirection);
      }
      if(outputRedirection != -1){
        if(dup2(outputRedirection, STDOUT_FILENO) < 0){
          fprintf(stderr, "Error redirecting output: %s\n", strerror(errno));
          exit(EXIT_FAILURE);
        }
      }
      if(execvp(currentArgs[0], currentArgs) < 0){
        fprintf(stderr, "Error executing command: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
      }
    }
    else{
      if(inputRedirection != -1)
        close(inputRedirection);
      if(outputRedirection != -1)
        close(outputRedirection);
      if(!background){
        if(foregroundProcess != -1)
          kill(foregroundProcess, SIGKILL);
        foregroundProcess = pid;
        signal(SIGINT, catch_fgkill);
        waitpid(pid, NULL, 0);
        foregroundProcess = -1;
        signal(SIGINT, SIG_DFL);
      }
      else{
        add_bgprocess(pid);
      }
    }
  }
}
}