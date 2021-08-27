#include "types.h"
#include "param.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#include "sysinfo.h"

// system call: sysinfo. Collects information about the running system.
// The system call takes one argument: a pointer to a struct sysinfo
// kernel fill out the fields of this struct: 
// the freemem field should be set to the number of bytes of free memory,
// and the nproc field should be set to the number of processes whose state is not UNUSED. 
// Return 0 if success, -1 if failed.
int get_sysinfo(uint64 sysinfo_addr){
    struct sysinfo my_sysinfo;

    my_sysinfo.freemem = cnt_freemem();
    my_sysinfo.nproc = cnt_proc();

    if(copyout(myproc()->pagetable, sysinfo_addr, (char*)&my_sysinfo, sizeof(my_sysinfo)) < 0){
        return -1;
    }
    return 0;
}