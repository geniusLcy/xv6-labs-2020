#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

// #define MY_DEBUG

int
main(int argc, char *argv[])
{
    // the pingpong cmd donot need any parameter
    if(argc > 1){
        fprintf(2, "pingpong: no operand needed.\n");
        exit(1);
    }
    if(argc < 1){
        fprintf(2, "pingpong: unexpected error.\n");
        exit(1);
    }
    // pipe accept type: int*
    int p1[2];
    int p2[2];
    // create two pipe
    pipe(p1); // pipe#1: parent -> child
    pipe(p2); // pipe#2: child -> parent

    int pid = fork(); // create child process
    if(pid < 0){ // error forking child
        fprintf(2, "fork: error forking child.\n");
        exit(1);
    }
    if(pid == 0){ // in child process
        char child_buf[1]; // for r/w pipe
        // bine pipe#1: parent -> child, read /////////////////////////////////////////////
        close(p1[1]); // close pipe#1 write port
        if(read(p1[0], child_buf, 1) != 1){ // read from pipe#1
            fprintf(2, "read: error reading pipe.\n");
            exit(1);
        }
        #ifdef MY_DEBUG
            printf("child receive byte: %c\n", child_buf[0]);
        #endif
        printf("%d: received ping\n", getpid()); // print ping msg
        close(p1[0]); // close pipe#1 read port
        // bine pipe#1: done ////////////////////////////////////////////////////////////

        // bine pipe#2 : child -> parent, write /////////////////////////////////////////
        close(p2[0]); // close pipe#2 read port
        if(write(p2[1], child_buf, 1) != 1){ // write to pipe#2
            fprintf(2, "write: error writing pipe.\n");
            exit(1);
        }
        close(p2[1]); // close pipe#2 write port
        // bine pipe#2 : done ///////////////////////////////////////////////////////////
    }else{ // in parent precess
        char parent_buf[1]; // for r/w pipe
        parent_buf[0] = 'a'; // my default pingpong byte

        // bine pipe#1: parent -> child, write /////////////////////////////////////////
        close(p1[0]); // close pipe#1 read port
        if(write(p1[1], parent_buf, 1) != 1){ // write to pipe#1
            fprintf(2, "write: error writing pipe.\n");
            exit(1);
        }
        close(p1[1]); // close pipe#1 write port
        // bine pipe#1: done ///////////////////////////////////////////////////////////

        // bine pipe#2: child -> parent, read //////////////////////////////////////////
        close(p2[1]); // close pipe#2 write port
        if(read(p2[0], parent_buf, 1) != 1){ // read from pipe#2
            fprintf(2, "read: error reading pipe.\n");
            exit(1);
        }
        #ifdef MY_DEBUG
            printf("parent receive byte: %c\n", parent_buf[0]);
        #endif
        printf("%d: received pong\n", getpid()); // print pong msg
        close(p2[0]); // close pipe#2 read port
        // bine pipe#2: done ///////////////////////////////////////////////////////////
    }
    exit(0);
}