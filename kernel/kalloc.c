// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

#define MEMREF_IDX(address) ((address - KERNBASE) / PGSIZE) // calculate index of the array
#define MEMREF_SZ ((PHYSTOP - KERNBASE) / PGSIZE)

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

struct {
  struct spinlock lock;
  uint tables[MEMREF_SZ];
} mem_ref_cnt;


void
acquire_mem_ref_cnt(){
  acquire(&mem_ref_cnt.lock);
}


void
release_mem_ref_cnt(){
  release(&mem_ref_cnt.lock);
}

void
mem_ref_cnt_setter(uint64 pa, int n){
  mem_ref_cnt.tables[MEMREF_IDX((uint64)pa)] = n;
}


uint
mem_ref_cnt_getter(uint64 pa){
  return mem_ref_cnt.tables[MEMREF_IDX(pa)];
}

void
mem_ref_cnt_incr(uint64 pa, int n){
  mem_ref_cnt.tables[MEMREF_IDX(pa)] += n;
}

void
mem_ref_cnt_decr(uint64 pa, int n){
  mem_ref_cnt.tables[MEMREF_IDX(pa)] -= n;
}

void
kinit()
{
  initlock(&kmem.lock, "kmem");
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
  // page with refcnt > 1 should not be freed
  acquire_mem_ref_cnt();
  if(mem_ref_cnt.tables[MEMREF_IDX((uint64)pa)] > 1){
    mem_ref_cnt.tables[MEMREF_IDX((uint64)pa)] -= 1;
    release_mem_ref_cnt();
    return;
  }


  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);
  mem_ref_cnt.tables[MEMREF_IDX((uint64)pa)] = 0;
  release_mem_ref_cnt();


  r = (struct run*)pa;

  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r)
    kmem.freelist = r->next;
  release(&kmem.lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  if(r){
    acquire_mem_ref_cnt();
    mem_ref_cnt_incr((uint64)r, 1); // set refcnt to 1
    release_mem_ref_cnt();
  }
  return (void*)r;
}

void *
kalloc_bypass(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r)
    kmem.freelist = r->next;
  release(&kmem.lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk

  if(r){
    mem_ref_cnt_setter((uint64)r, 1); // set refcnt to 1
  }
  return (void*)r;
}
