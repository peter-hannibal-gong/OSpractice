// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem/*added by gch*/[NCPU];

void
kinit()
{
//added by gch
  for(int i = 0; i < NCPU; i++){
    char name[9] = {0};
    snprintf(name, 8, "kmem_%d", i); //每个锁的名字不一样
    initlock(&kmem[i].lock,name);
  }

  //initlock(&kmem.lock, "kmem");
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
/*void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);
}
*/
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");
  
  
  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;
  
  //关中断，获取当前CPU的id
  push_off();
  int i = cpuid();
  pop_off();
  
  acquire(&kmem[i].lock);
  r->next = kmem[i].freelist;
  kmem[i].freelist = r;
  release(&kmem[i].lock);
}


// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
/*void *
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
  return (void*)r;
}
*/
void *
kalloc(void)
{
  struct run *r;
  
  //关闭所有中断，找到当前的cpuid
  push_off();
  int i = cpuid();
  pop_off();
  
  //获取当前cpu的freelist
  acquire(&kmem[i].lock);
  r = kmem[i].freelist;
  
  if(r){
    kmem[i].freelist = r->next; //如果可用，就直接用
  }else{ //不可用，找其他的CPU是否有可用的freelist
    for(int j = 0; j < NCPU; j++){
        if(j == i) continue;
        
    	acquire(&kmem[j].lock);  //上锁
    	if(!kmem[j].freelist){   //没有可用的，解锁，继续找
    	  release(&kmem[j].lock);
    	  continue;
    	}
    	
    	r = kmem[j].freelist;  //找到了可用的freelist
    	kmem[j].freelist = r->next;  //将可用的freelist赋给之前需要的CPU
    	release(&kmem[j].lock);  //解锁
    	break;  //直接退出，无需再找
    }
  
  }
  release(&kmem[i].lock);
  
  
  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}

