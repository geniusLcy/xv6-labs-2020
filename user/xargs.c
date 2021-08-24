#include "kernel/types.h"
#include "user/user.h"
#include "kernel/param.h"

#define BYTE_SIZE 1
#define BUFFER_SIZE 256

int main(int argc, char *argv[]){
    if (argc < 2){
        fprintf(2, "xargs: more oprand needed.\n");
        exit(1);
    }
    char c;
    int check_c, i = 0;
    char buf[BUFFER_SIZE];
    memset(buf, 0, sizeof(buf));
    while ((check_c = read(0, &c, BYTE_SIZE)) > 0){ // read one byte per loop
        if (check_c == BYTE_SIZE && c != '\n'){
            buf[i++] = c; // store to buf
        }
        if (c == '\n'){ // one line done
            buf[i] = '\0'; // c-string end

            argv[argc] = buf; // xargs add stdin to arguments list
            argv[argc + 1] = '\0'; // important! c-string end!

            int pid = fork();
            if (pid < 0){
                fprintf(2, "fork: error forking child.\n");
                exit(1);
            }
            if (pid == 0){ // in child process
                // execute command
                exec(argv[1], (char**)(argv + 1)); // the first argv is the cmd's name
                exit(0);
            }else{ // in parent process
                wait((int*)0);
            }

            i = 0; // reset reading head
            memset(buf, 0, sizeof(buf)); // clear buffer
        }
    }
    exit(0);
}