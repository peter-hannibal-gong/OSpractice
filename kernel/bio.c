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
//added by gch
extern uint ticks;
#define NBUCKET      13
struct {
  struct spinlock lock[NBUCKET];  //每个桶都有一个锁
  struct buf buf[NBUF];
  struct buf buckets[NBUCKET];  //每个桶都有一个索引开始的地方，可以看作是原来的head
} bcache;
//定义哈希函数
int hash(uint blockno){
  return blockno % NBUCKET;
}

/*
struct {
  struct spinlock lock;
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  struct buf head;
} bcache;*/
/*
void
binit(void)
{
  struct buf *b;

  initlock(&bcache.lock, "bcache");

  // Create linked list of buffers
  bcache.head.prev = &bcache.head;
  bcache.head.next = &bcache.head;
  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    b->next = bcache.head.next;
    b->prev = &bcache.head;
    initsleeplock(&b->lock, "buffer");
    bcache.head.next->prev = b;
    bcache.head.next = b;
  }
}
*/
//added by gch
void
binit(void)
{
  struct buf *b;
  
  //初始化每个桶的锁
  for(int i = 0; i < NBUCKET; i++){
      initlock(&bcache.lock[i], "bcache");
  }

  // 将每个桶内的双向链表初始化
  for(int i = 0; i < NBUCKET;i++){
      bcache.buckets[i].prev = &bcache.buckets[i];
      bcache.buckets[i].next = &bcache.buckets[i];
  }
  
  //将每个双向链表分配好NUBF的内存
  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    b->next = bcache.buckets[0].next;
    b->prev = &bcache.buckets[0];
    initsleeplock(&b->lock, "buffer");
    bcache.buckets[0].next->prev = b;
    bcache.buckets[0].next = b;
  }
}


// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
/*static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;

  acquire(&bcache.lock);

  // Is the block already cached?
  for(b = bcache.head.next; b != &bcache.head; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.lock);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  for(b = bcache.head.prev; b != &bcache.head; b = b->prev){
    if(b->refcnt == 0) {
      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;
      b->refcnt = 1;
      release(&bcache.lock);
      acquiresleep(&b->lock);
      return b;
    }
  }
  panic("bget: no buffers");
}*/
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;
  
  int id = hash(blockno);
  acquire(&bcache.lock[id]);

  // 寻找对应设备号和块号的缓存块
  for(b = bcache.buckets[id].next; b != &bcache.buckets[id]; b = b->next){
    if(b->dev == dev && b->blockno == blockno){  //命中
      b->refcnt++;    //增加引用数
      release(&bcache.lock[id]);
      acquiresleep(&b->lock);
      return b;
    }
  }
  
  uint least = ticks; //获取当前时间戳
  struct buf *res = 0; //定义一个临时缓存块
  
  //找到最小的时间戳，也就是最早使用的缓存块，赋值给res
  for(b = bcache.buckets[id].next; b != &bcache.buckets[id]; b = b->next){
    if(b->refcnt == 0 && b->lastuse <= least){  //
        least = b->lastuse;
        res = b;
    }
  }
  
  if(res){ //如果这个块可用，那就初始化，将所有信息重置
     res->dev = dev;
     res->blockno = blockno;
     res->valid = 0;
     res->refcnt = 1;
     release(&bcache.lock[id]);
     acquiresleep(&res->lock);
     return res;
  } else { //如果不可用，需要从其他的桶中来“偷”
    for(int i = 0; i < NBUCKET;i++){
  	if(i == id) continue;  //跳过当前id的桶
  	
  	acquire(&bcache.lock[i]);  //给当前桶上锁
  	least = ticks;  //获取当前时间戳
  	//找到这个桶里最早使用的块
  	for(b = bcache.buckets[i].next; b != &bcache.buckets[i]; b = b->next){
          if(b->refcnt==0 && b->lastuse<=least) {
            least = b->lastuse;
            res = b;
          }
        }
          //如果这个块不可用，那就找下一个桶
          if(!res){
            release(&bcache.lock[i]);
            continue;
          }
          //可用就初始化这个快的信息
          res->dev = dev;
          res->blockno = blockno;
          res->valid = 0;
          res->refcnt = 1;
          
          //把这个可用块从当前桶（非id桶）中拿出来
          res->next->prev = res->prev;
          res->prev->next = res->next;
          release(&bcache.lock[i]);
          
          //把这个可用块放到需要的桶中id位置
          res->next = bcache.buckets[id].next;
          bcache.buckets[id].next->prev = res;
          bcache.buckets[id].next = res;
          res->prev = &bcache.buckets[id];
          
          release(&bcache.lock[id]);
          acquiresleep(&res->lock);
          return res; 
    }
  }
  //上述过程也没找到，说明当前调度算法下，没有可用块
  release(&bcache.lock[id]);
  panic("bget: no buffers");  

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

//added by gch
  int id = hash(b->blockno);

  acquire(&bcache.lock/*added by gch*/[id]);
  b->refcnt--;
  if (b->refcnt == 0) {
//added by gch
    b->lastuse = ticks; //引用计数为0，说明刚初始化，加上时间戳
/*
    // no one is waiting for it.
    b->next->prev = b->prev;
    b->prev->next = b->next;
    b->next = bcache.head.next;
    b->prev = &bcache.head;
    bcache.head.next->prev = b;
    bcache.head.next = b; 
*/}
  
  release(&bcache.lock/*added by gch*/[id]);
}

void
bpin(struct buf *b) {
  //added by gch 
  //将锁细化
  int id = hash(b->blockno);

  acquire(&bcache.lock/*added by gch*/[id]);
  b->refcnt++;
  release(&bcache.lock/*added by gch*/[id]);
}

void
bunpin(struct buf *b) {
  //added by gch
  //将锁细化
  int id = hash(b->blockno);

  acquire(&bcache.lock/*added by gch*/[id]);
  b->refcnt--;
  release(&bcache.lock/*added by gch*/[id]);
}


