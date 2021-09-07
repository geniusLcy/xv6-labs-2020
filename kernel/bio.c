// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

#define BUCKET_NUM 13

#ifdef BUF_NUM
#undef BUF_NUM
#endif
#define BUF_NUM (BUCKET_NUM * 3)

#define GET_HASH(i) (i % BUCKET_NUM)

struct {
  struct spinlock lock;
  struct buf buf[BUF_NUM];
} bcache;

struct bucket {
  struct spinlock lock;
  struct buf head;
}hash_table[BUCKET_NUM];


void
binit(void)
{
  struct buf *b;

  initlock(&bcache.lock, "bcache");


  for(b = bcache.buf; b < bcache.buf + BUF_NUM; b++){
    initsleeplock(&b->lock, "buffer");
  }

  b = bcache.buf;
  for (int i = 0; i < BUCKET_NUM; i++) {
    initlock(&hash_table[i].lock, "bcache_bucket");
    for (int j = 0; j < BUF_NUM / BUCKET_NUM; j++) {
      b->blockno = i;
      b->next = hash_table[i].head.next;
      hash_table[i].head.next = b;
      b++;
    }
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;

  int idx = GET_HASH(blockno);
  struct bucket* bucket = hash_table + idx;
  acquire(&bucket->lock);

  // Is the block already cached?
  for(b = bucket->head.next; b != 0; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bucket->lock);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // Not cached.
  // find in current bucket.
  int minimum = 0x3f3f3f3f;
  struct buf* replace_buf = 0;

  for(b = bucket->head.next; b != 0; b = b->next){
    if(b->refcnt == 0 && b->lru_record < minimum) {
      replace_buf = b;
      minimum = b->lru_record;
    }
  }
  if(replace_buf) {
    goto found;
  }

  // find in other bucket.
  acquire(&bcache.lock);
  loop:
  for(b = bcache.buf; b < bcache.buf + BUF_NUM; b++) {
    if(b->refcnt == 0 && b->lru_record < minimum) {
      replace_buf = b;
      minimum = b->lru_record;
    }
  }
  if (replace_buf) {
    // remove from old bucket
    int yaidx = GET_HASH(replace_buf->blockno);
    acquire(&hash_table[yaidx].lock);
    if(replace_buf->refcnt != 0){
      // be used in another bucket's local find between finded and acquire
      release(&hash_table[yaidx].lock);
      goto loop;
    }
    struct buf *previous = &hash_table[yaidx].head;
    struct buf *ptr = hash_table[yaidx].head.next;
    while (ptr != replace_buf) {
      previous = previous->next;
      ptr = ptr->next;
    }
    previous->next = ptr->next;
    release(&hash_table[yaidx].lock);
    // add to current bucket
    replace_buf->next = hash_table[idx].head.next;
    hash_table[idx].head.next = replace_buf;
    release(&bcache.lock);
    goto found;
  }
  else {
    panic("bget: no buffers");
  }

  found:
  replace_buf->dev = dev;
  replace_buf->blockno = blockno;
  replace_buf->valid = 0;
  replace_buf->refcnt = 1;
  release(&bucket->lock);
  acquiresleep(&replace_buf->lock);
  return replace_buf;
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  int idx = GET_HASH(b->blockno);

  acquire(&hash_table[idx].lock);
  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    b->lru_record = ticks;
  }
  
  release(&hash_table[idx].lock);
}

void
bpin(struct buf *b) {
  int idx = GET_HASH(b->blockno);
  acquire(&hash_table[idx].lock);
  b->refcnt++;
  release(&hash_table[idx].lock);
}

void
bunpin(struct buf *b) {
  int idx = GET_HASH(b->blockno);
  acquire(&hash_table[idx].lock);
  b->refcnt--;
  release(&hash_table[idx].lock);
}


