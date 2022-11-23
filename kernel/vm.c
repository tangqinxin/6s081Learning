#include "param.h"
#include "types.h"
#include "memlayout.h"
#include "elf.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"

#include "spinlock.h"
#include "proc.h"

/*
 * the kernel's page table.
 */
pagetable_t kernel_pagetable;

extern char etext[];  // kernel.ld sets this to end of kernel code.

extern char trampoline[]; // trampoline.S

/*
 * create a direct-map page table for the kernel.
 */
void
kvminit()
{
  kernel_pagetable = (pagetable_t) kalloc();
  memset(kernel_pagetable, 0, PGSIZE);

  // uart registers
  kvmmap(UART0, UART0, PGSIZE, PTE_R | PTE_W);

  // virtio mmio disk interface
  kvmmap(VIRTIO0, VIRTIO0, PGSIZE, PTE_R | PTE_W);

  // CLINT
  kvmmap(CLINT, CLINT, 0x10000, PTE_R | PTE_W);

  // PLIC
  kvmmap(PLIC, PLIC, 0x400000, PTE_R | PTE_W);

  // map kernel text executable and read-only.
  kvmmap(KERNBASE, KERNBASE, (uint64)etext-KERNBASE, PTE_R | PTE_X);

  // map kernel data and the physical RAM we'll make use of.
  kvmmap((uint64)etext, (uint64)etext, PHYSTOP-(uint64)etext, PTE_R | PTE_W);

  // map the trampoline for trap entry/exit to
  // the highest virtual address in the kernel.
  kvmmap(TRAMPOLINE, (uint64)trampoline, PGSIZE, PTE_R | PTE_X);
}

// Switch h/w page table register to the kernel's page table,
// and enable paging.
void
kvminithart()
{
  w_satp(MAKE_SATP(kernel_pagetable));
  sfence_vma();
}

// Return the address of the PTE in page table pagetable
// that corresponds to virtual address va.  If alloc!=0,
// create any required page-table pages.
//
// The risc-v Sv39 scheme has three levels of page-table
// pages. A page-table page contains 512 64-bit PTEs.
// A 64-bit virtual address is split into five fields:
//   39..63 -- must be zero.
//   30..38 -- 9 bits of level-2 index.
//   21..29 -- 9 bits of level-1 index.
//   12..20 -- 9 bits of level-0 index.
//    0..11 -- 12 bits of byte offset within the page.
pte_t *
walk(pagetable_t pagetable, uint64 va, int alloc)
{
  if(va >= MAXVA)
    panic("walk");

  for(int level = 2; level > 0; level--) {
    pte_t *pte = &pagetable[PX(level, va)];
    if(*pte & PTE_V) {
      pagetable = (pagetable_t)PTE2PA(*pte);
    } else {
      if(!alloc || (pagetable = (pde_t*)kalloc()) == 0){
        return 0;
      }
      memset(pagetable, 0, PGSIZE);
      *pte = PA2PTE(pagetable) | PTE_V;
    }
  }
  return &pagetable[PX(0, va)];
}

// Look up a virtual address, return the physical address,
// or 0 if not mapped.
// Can only be used to look up user pages.
uint64
walkaddr(pagetable_t pagetable, uint64 va)
{
  pte_t *pte;
  uint64 pa;

  if(va >= MAXVA)
    return 0;

  pte = walk(pagetable, va, 0);
  if(pte == 0)
    return 0;
  if((*pte & PTE_V) == 0)
    return 0;
  if((*pte & PTE_U) == 0)
    return 0;
  pa = PTE2PA(*pte);
  return pa;
}

// add a mapping to the kernel page table.
// only used when booting.aa
// does not flush TLB or enable paging.
void
kvmmap(uint64 va, uint64 pa, uint64 sz, int perm)
{
  if(mappages(kernel_pagetable, va, sz, pa, perm) != 0)
    panic("kvmmap");
}

// translate a kernel virtual address to
// a physical address. only needed for
// addresses on the stack.
// assumes va is page aligned.
uint64
kvmpa(uint64 va)
{
  uint64 off = va % PGSIZE;
  pte_t *pte;
  uint64 pa;
  
  // pte = walk(kernel_pagetable, va, 0); // tm ?
  pte = walk(myproc()->k_pagetable, va, 0); // 修改这里?
  if(pte == 0)
    panic("kvmpa");
  if((*pte & PTE_V) == 0)
    panic("kvmpa");
  pa = PTE2PA(*pte);
  return pa+off;
}

// Create PTEs for virtual addresses starting at va that refer to
// physical addresses starting at pa. va and size might not
// be page-aligned. Returns 0 on success, -1 if walk() couldn't
// allocate a needed page-table page.
int
mappages(pagetable_t pagetable, uint64 va, uint64 size, uint64 pa, int perm)
{
  uint64 a, last;
  pte_t *pte;

  a = PGROUNDDOWN(va);
  last = PGROUNDDOWN(va + size - 1);
  for(;;){
    if((pte = walk(pagetable, a, 1)) == 0){
      return -1;
    }
    if(*pte & PTE_V)
      panic("remap");
    *pte = PA2PTE(pa) | perm | PTE_V;
    if(a == last)
      break;
    a += PGSIZE;
    pa += PGSIZE;
  }
  return 0;
}

// Remove npages of mappings starting from va. va must be
// page-aligned. The mappings must exist.
// Optionally free the physical memory.
void
uvmunmap(pagetable_t pagetable, uint64 va, uint64 npages, int do_free)
{
  uint64 a;
  pte_t *pte;

  if((va % PGSIZE) != 0)
    panic("uvmunmap: not aligned");

  for(a = va; a < va + npages*PGSIZE; a += PGSIZE){
    if((pte = walk(pagetable, a, 0)) == 0){
      printf("uvmunmap walk panic: va: %p,npages: %p\n",a, npages);
      panic("uvmunmap: walk");
    }
    if((*pte & PTE_V) == 0){
      printf("uvmunmap panic: va: %p,npages: %p\n",va, npages);
      panic("uvmunmap: not mapped");
    }
    if(PTE_FLAGS(*pte) == PTE_V)
      panic("uvmunmap: not a leaf");
    if(do_free){
      uint64 pa = PTE2PA(*pte);
      kfree((void*)pa);
    }
    *pte = 0;
  }
}

// create an empty user page table.
// returns 0 if out of memory.
pagetable_t
uvmcreate()
{
  pagetable_t pagetable;
  pagetable = (pagetable_t) kalloc();
  if(pagetable == 0)
    return 0;
  memset(pagetable, 0, PGSIZE);
  return pagetable;
}

// Load the user initcode into address 0 of pagetable,
// for the very first process.
// sz must be less than a page.
void
uvminit(pagetable_t pagetable, uchar *src, uint sz)
{
  char *mem;

  if(sz >= PGSIZE)
    panic("inituvm: more than a page");
  mem = kalloc();
  memset(mem, 0, PGSIZE);
  mappages(pagetable, 0, PGSIZE, (uint64)mem, PTE_W|PTE_R|PTE_X|PTE_U);
  memmove(mem, src, sz);
}

// Allocate PTEs and physical memory to grow process from oldsz to
// newsz, which need not be page aligned.  Returns new size or 0 on error.
uint64
uvmalloc(pagetable_t pagetable, uint64 oldsz, uint64 newsz)
{
  char *mem;
  uint64 a;

  if(newsz < oldsz)
    return oldsz;

  oldsz = PGROUNDUP(oldsz);
  for(a = oldsz; a < newsz; a += PGSIZE){
    mem = kalloc();
    if(mem == 0){
      uvmdealloc(pagetable, a, oldsz);
      return 0;
    }
    memset(mem, 0, PGSIZE);
    if(mappages(pagetable, a, PGSIZE, (uint64)mem, PTE_W|PTE_X|PTE_R|PTE_U) != 0){
      kfree(mem);
      uvmdealloc(pagetable, a, oldsz);
      return 0;
    }
  }
  return newsz;
}

// Deallocate user pages to bring the process size from oldsz to
// newsz.  oldsz and newsz need not be page-aligned, nor does newsz
// need to be less than oldsz.  oldsz can be larger than the actual
// process size.  Returns the new process size.
uint64
uvmdealloc(pagetable_t pagetable, uint64 oldsz, uint64 newsz)
{
  if(newsz >= oldsz)
    return oldsz;

  if(PGROUNDUP(newsz) < PGROUNDUP(oldsz)){
    int npages = (PGROUNDUP(oldsz) - PGROUNDUP(newsz)) / PGSIZE;
    uvmunmap(pagetable, PGROUNDUP(newsz), npages, 1);
  }

  return newsz;
}

// Recursively free page-table pages.
// All leaf mappings must already have been removed.
void
freewalk(pagetable_t pagetable)
{
  // there are 2^9 = 512 PTEs in a page table.
  for(int i = 0; i < 512; i++){
    pte_t pte = pagetable[i];
    if((pte & PTE_V) && (pte & (PTE_R|PTE_W|PTE_X)) == 0){
      // this PTE points to a lower-level page table.
      uint64 child = PTE2PA(pte);
      freewalk((pagetable_t)child);
      pagetable[i] = 0;
    } else if(pte & PTE_V){
      panic("freewalk: leaf");
    }
  }
  kfree((void*)pagetable);
}

// Free user memory pages,
// then free page-table pages.
void
uvmfree(pagetable_t pagetable, uint64 sz)
{
  if(sz > 0)
    uvmunmap(pagetable, 0, PGROUNDUP(sz)/PGSIZE, 1);
  freewalk(pagetable);
}

// Given a parent process's page table, copy
// its memory into a child's page table.
// Copies both the page table and the
// physical memory.
// returns 0 on success, -1 on failure.
// frees any allocated pages on failure.
int
uvmcopy(pagetable_t old, pagetable_t new, uint64 sz)
{
  pte_t *pte;
  uint64 pa, i;
  uint flags;
  char *mem;

  for(i = 0; i < sz; i += PGSIZE){
    if((pte = walk(old, i, 0)) == 0)
      panic("uvmcopy: pte should exist");
    if((*pte & PTE_V) == 0)
      panic("uvmcopy: page not present");
    pa = PTE2PA(*pte);
    flags = PTE_FLAGS(*pte);
    if((mem = kalloc()) == 0)
      goto err;
    memmove(mem, (char*)pa, PGSIZE);
    if(mappages(new, i, PGSIZE, (uint64)mem, flags) != 0){
      kfree(mem);
      goto err;
    }
  }
  return 0;

 err:
  uvmunmap(new, 0, i / PGSIZE, 1);
  return -1;
}

// mark a PTE invalid for user access.
// used by exec for the user stack guard page.
void
uvmclear(pagetable_t pagetable, uint64 va)
{
  pte_t *pte;
  
  pte = walk(pagetable, va, 0);
  if(pte == 0)
    panic("uvmclear");
  *pte &= ~PTE_U;
}

// Copy from kernel to user.
// Copy len bytes from src to virtual address dstva in a given page table.
// Return 0 on success, -1 on error.
int
copyout(pagetable_t pagetable, uint64 dstva, char *src, uint64 len)
{
  uint64 n, va0, pa0;

  while(len > 0){
    va0 = PGROUNDDOWN(dstva);
    pa0 = walkaddr(pagetable, va0);
    if(pa0 == 0)
      return -1;
    n = PGSIZE - (dstva - va0);
    if(n > len)
      n = len;
    memmove((void *)(pa0 + (dstva - va0)), src, n);

    len -= n;
    src += n;
    dstva = va0 + PGSIZE;
  }
  return 0;
}

// Copy from user to kernel.
// Copy len bytes to dst from virtual address srcva in a given page table.
// Return 0 on success, -1 on error.
int
copyin(pagetable_t pagetable, char *dst, uint64 srcva, uint64 len)
{
  uint64 n, va0, pa0;

  while(len > 0){
    va0 = PGROUNDDOWN(srcva);
    pa0 = walkaddr(pagetable, va0);
    if(pa0 == 0)
      return -1;
    n = PGSIZE - (srcva - va0);
    if(n > len)
      n = len;
    memmove(dst, (void *)(pa0 + (srcva - va0)), n);

    len -= n;
    dst += n;
    srcva = va0 + PGSIZE;
  }
  return 0;
}

// Copy a null-terminated string from user to kernel.
// Copy bytes to dst from virtual address srcva in a given page table,
// until a '\0', or max.
// Return 0 on success, -1 on error.
int
copyinstr(pagetable_t pagetable, char *dst, uint64 srcva, uint64 max)
{
  uint64 n, va0, pa0;
  int got_null = 0;

  while(got_null == 0 && max > 0){
    va0 = PGROUNDDOWN(srcva);
    pa0 = walkaddr(pagetable, va0);
    if(pa0 == 0)
      return -1;
    n = PGSIZE - (srcva - va0);
    if(n > max)
      n = max;

    char *p = (char *) (pa0 + (srcva - va0));
    while(n > 0){
      if(*p == '\0'){
        *dst = '\0';
        got_null = 1;
        break;
      } else {
        *dst = *p;
      }
      --n;
      --max;
      p++;
      dst++;
    }

    srcva = va0 + PGSIZE;
  }
  if(got_null){
    return 0;
  } else {
    return -1;
  }
}

// print the page table
void vmprint(pagetable_t pagetable) {
  printf("page table %p\n", pagetable);
  dfs(pagetable, 0);
}

// 
void dfs(pagetable_t pagetable,int level) {
 // there are 2^9 = 512 PTEs in a page table.
  for(int i = 0; i < 512; i++){
    pte_t pte = pagetable[i];

    if((pte & PTE_V) && (pte & (PTE_R|PTE_W|PTE_X)) == 0){
      // this PTE points to a lower-level page table.
      uint64 child = PTE2PA(pte);
      // 进入下一级之前，需要打印一次
      addBlankAtHead(level);
      printf("%d: pte %p pa %p\n",i , pte, child);
      dfs((pagetable_t)child, level+1);
    } else if(pte & PTE_V){
      // 最后一级，直接打印地址
      uint64 pa = PTE2PA(pte);
      addBlankAtHead(level);
      printf("%d: pte %p pa %p\n",i , pte, pa);
    }
  }
}

void addBlankAtHead(int level){
  printf("..");
  for(int i=0;i<level;i++){
    if(level!=0){
      printf(" "); // 第一级前面不加空格
    }
    printf("..");
  }
}



/*
 * create a direct-map page table for process
 * 1、每个进程有一个内核页表
 * 2、内核页表创建的时候，大部分地址映射是固定的
 * 3、进程的内核页表，可能只有trampoline和kernel_data部分不同；但是实际上由于映射的地址是相同的，因此生成的页表也是相同的
 * 4、做了相同的映射，那么内核页表应该就是相同的
 * 5、要如何将页表项进行同步? -> 考虑walk一遍进行拷贝?
 */
pagetable_t kvm_create_kpt_process() {
  pagetable_t proc_kernel_pagetable = (pagetable_t) kalloc();
  if (proc_kernel_pagetable == 0) return 0;

  memset(proc_kernel_pagetable, 0, PGSIZE);

  // uart registers
  kvm_map_kpt_process(UART0, UART0, PGSIZE, PTE_R | PTE_W, proc_kernel_pagetable);

  // virtio mmio disk interface
  kvm_map_kpt_process(VIRTIO0, VIRTIO0, PGSIZE, PTE_R | PTE_W, proc_kernel_pagetable);

  // 用户进程的最大大小限制为小于内核的最低虚拟地址。内核启动后，在XV6中该地址是0xC000000，即PLIC寄存器的地址
  // CLINT
  // kvm_map_kpt_process(CLINT, CLINT, 0x10000, PTE_R | PTE_W, proc_kernel_pagetable);

  // PLIC
  kvm_map_kpt_process(PLIC, PLIC, 0x400000, PTE_R | PTE_W, proc_kernel_pagetable);

  // map kernel text executable and read-only.
  kvm_map_kpt_process(KERNBASE, KERNBASE, (uint64)etext-KERNBASE, PTE_R | PTE_X, proc_kernel_pagetable);

  // map kernel data and the physical RAM we'll make use of.
  kvm_map_kpt_process((uint64)etext, (uint64)etext, PHYSTOP-(uint64)etext, PTE_R | PTE_W, proc_kernel_pagetable);

  // map the trampoline for trap entry/exit to
  // the highest virtual address in the kernel.
  kvm_map_kpt_process(TRAMPOLINE, (uint64)trampoline, PGSIZE, PTE_R | PTE_X, proc_kernel_pagetable);
  return proc_kernel_pagetable;
}

// 对特殊的页表建立映射
void kvm_map_kpt_process(uint64 va, uint64 pa, uint64 sz, int perm, pagetable_t proc_kernel_pagetable) {
  if(mappages(proc_kernel_pagetable, va, sz, pa, perm) != 0){
    printf("panic when map %p\n", va);
    panic("kvm_map_kpt_process");
  }
}


void proc_free_kernel_pagetable(pagetable_t pagetable, uint64 kstack, uint64 sz)
{
  uvmunmap(pagetable, UART0, 1, 0);
 
  uvmunmap(pagetable, VIRTIO0, 1, 0);

  // 用户进程的最大大小限制为小于内核的最低虚拟地址。内核启动后，在XV6中该地址是0xC000000，即PLIC寄存器的地址
  // uvmunmap(pagetable, CLINT, 0x10000 / PGSIZE, 0);
 
  uvmunmap(pagetable, PLIC, 0x400000 / PGSIZE, 0);
 
  uvmunmap(pagetable, KERNBASE, ((uint64)etext-KERNBASE) / PGSIZE, 0);
 
  uvmunmap(pagetable, (uint64)etext, (PHYSTOP-(uint64)etext) / PGSIZE, 0);
 
  uvmunmap(pagetable, TRAMPOLINE, 1, 0);
  
  // 该进程私有的内核栈释放物理内存
  uvmunmap(pagetable, kstack, 1, 1);

  // 实验指导书：Xv6使用从零开始的虚拟地址作为用户地址空间，幸运的是内核的内存从更高的地址开始
  // 用户进程空间为sz，因此这里应该释放sz/PGSIZE的空间
  uvmunmap(pagetable, 0, PGROUNDUP(sz)/PGSIZE, 0);

  // 释放页表所占物理内存
  freewalk(pagetable); // 此处递归释放内核页表物理页
}

// 将user_pgtb中的从srcva用户虚拟地址空间的映射关系 同步 到同一进程的内核页表kernel_pgtb中
void copyUserPgtb2kPgtb(pagetable_t user_pgtb, pagetable_t kernel_pgtb, uint64 srcva, uint64 sz)
{
  pte_t* user_pte;
  pte_t* kernel_pte;
  srcva = PGROUNDUP(srcva);
  for(uint64 i = srcva; i < srcva + sz; i += PGSIZE){
    if((user_pte = walk(user_pgtb, i, 0)) == 0){
      panic("copyUserPgtb2kPgtb: pte in user_pgtb not exist");
    }
    // if((*user_pte & PTE_V) == 0){
    //   panic("copyUserPgtb2kPgtb: pte in user_pgtb not present");
    // }
    if((kernel_pte = walk(kernel_pgtb, i, 1)) == 0){
      panic("copyUserPgtb2kPgtb: pte in kernel_pgtb not found or create failed");
    }
    // 此处并非拷贝整个页，而是将页表映射关系添加到内核页表中
    *kernel_pte = (*user_pte) & (~PTE_U); // 去除PTE_U标志
  }
}