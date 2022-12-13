#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>

int main(int argc, char *argv[]){

    system("ls -l");
    /*
    => fork() :  

        Creates a child process, returns 0 to parent process and the pid of the child process to child process
    */
   
    /*
    => wait(int* status_location) :

        Waits for the child processes to terminate, returns the process id that terminated or -1 and sets errno
    */

    /*
    => waitpid(pid_t pid, int* status_location, int options) :
     
        Waits for the specified process whose process id is pid when pid > 0 and waits for all the child processes when pid = -1,
        It returns 0 to specify that there are child processes that did not yet terminate
        WNOHANG flag : Causes the function to return even if the child is not terminated
    */

   

    /*
    => execl (const char *path, const char *arg0, ... , char *argn)
    => execle (const char *path, const char *arg0, ... , char *argn, char *const envp[])
    => execlp (const char *file, const char *arg0, ... , char *argn )
    => execv(const char *path, char *const argv[])
    => execve (const char *path, char *const argv[], char *const envp[])
    => execvp (const char *file, char *const argv[]) :
    
        Executes the program with the given arguments
    
    */

    /*
    => dup(int oldfd) :  
    
        Creates a copy of a file descriptor.
    */

    /*
    => dup2(int oldfd, int newfd) : 

        Duplicates the old file descriptor into the new file descriptor, after that the new file descriptor can be used in place of old file descriptor
    */

    /*
    => pipe(int fileDesc[2]) :
        Creates a unidirectional communication buffer
        fileDesc[0] is the read end of the pipe
        fileDesc[1] is the write end of the pipe
    */

    /*
    => read (int fd, void* buf, size_t cnt) :

        Reads cnt bytes from the file described by the fd into the buf
    */

    /*
    => write (int fd, void* buf, size_t cnt) : 

        Writes cnt bytes from buf into the file described by fd
    */

    /*
    => kill(pid_t pid, int signalNbr) : 

        Sends a signal to a child process, returns 0 if successful otherwise 0
    */

    /*
    => sigemptyset(sigset_t *set)
    => sigfillset(sigset_t *set)
    => sigaddset(sigset_t *set, int signo)
    => sigdelset(sigset_t *set, int signo)
    => sigismember(const sigset_t *set, int signo) :

        Performs fill/add/find/remove operations on a signal set
    */
}
