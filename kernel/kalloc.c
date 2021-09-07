// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

#define LOCK_NAME_SZ 6

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
  char name[LOCK_NAME_SZ]; // name of lock. For example: kmem0
} kmem[NCPU]; // every CPU has a kmem freelist

void
kinit()
{
  // initialize the lock
  int i;
  for(i = 0; i < NCPU; i++){
    snprintf(kmem[i].name, sizeof(kmem[i].name), "kmem%d", i);
    initlock(&kmem[i].lock, kmem[i].name);
  }
  // Let freerange give all free memory to the CPU running freerange
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  // push to current CPU's freelist

  push_off(); // turn off interrupt
  
  int current_cpu = cpuid();

  acquire(&kmem[current_cpu].lock);
  r->next = kmem[current_cpu].freelist;
  kmem[current_cpu].freelist = r;
  release(&kmem[current_cpu].lock);

  pop_off(); // turn on interrupt
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  push_off(); // turn off interrupt
  
  int current_cpu = cpuid();
  acquire(&kmem[current_cpu].lock);
  r = kmem[current_cpu].freelist;
  if(r){ // current CPU has free space
    kmem[current_cpu].freelist = r->next;
  }else{ // current CPU donot has free space,
    // borrow from other CPU
    int i;
    for(i = 0; i < NCPU; i++){
      if(i == current_cpu){
        // current CPU has no free space
        continue;
      }
      acquire(&kmem[i].lock);
      r = kmem[i].freelist;
      if(r){
        // find free space
        kmem[i].freelist = r->next;
        release(&kmem[i].lock);
        break;
      }
      // still not find free space, loop continue
      release(&kmem[i].lock);
    }
  }
  release(&kmem[current_cpu].lock);

  pop_off(); // turn on interrupt

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}
