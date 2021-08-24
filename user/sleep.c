#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
    // my sleep only accept one parameter: sleep time(default second) / '--help'
    if(argc != 2){
        // number of parameter do not match
        fprintf(2, "sleep: missing operand\nTry \'sleep --help\' for more information.\n");
        exit(1);
    }
    
    if(strcmp(argv[1], "--help") == 0){
        // print the help
        printf("Usage: sleep NUMBER\nor:  sleep OPTION\nPause for NUMBER time unit.\n--help     display this help and exit\n");
    }
    else{
        // exec sleep
        sleep(atoi(argv[1]));
    }
    exit(0);
}