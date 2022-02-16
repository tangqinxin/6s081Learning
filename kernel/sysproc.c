#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"
#include "sysinfo.h"

uint64 get_free_mem_size(); // get free memory size
uint64 get_proc_num(); // get proc num

uint64
sys_exit(void)
{
  int n;
  if(argint(0, &n) < 0)
    return -1;
  exit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  if(argaddr(0, &p) < 0)
    return -1;
  return wait(p);
}

uint64
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;

  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

// 在kernel/sysproc.c中添加一个sys_trace()函数，它通过将参数保存到proc结构体（请参见kernel/proc.h）里的一个新变量中来实现新的系统调用.
uint64
sys_trace(void)
{
    int n;
  if(argint(0, &n) < 0)
    return -1;
  // printf("call sys_trace,sys_num is %d\n",n); // for debug
  myproc()->mask=n;
  return 0;
}

// 在kernel/sysproc.c中添加一个sys_trace()函数，它通过将参数保存到proc结构体（请参见kernel/proc.h）里的一个新变量中来实现新的系统调用.
uint64
sys_sysinfo(void)
{
  struct proc *p = myproc();
  uint64 user_virtual_addr;
  
  // 1 get the input user virtual address
  if(argaddr(0, &user_virtual_addr) < 0){
    printf("sysinfo: Get the VM address failed\n");
    return -1;  
  }
  
  // 2 calculate relevant data in sysinfo
  uint64 free_mem_cnt=get_free_mem_size();
  uint64 proc_num=get_proc_num();
  
  struct sysinfo st;  
  st.freemem=free_mem_cnt;
  st.nproc=proc_num;
 
  // 3 copy the data in sysinfo out to the user virtual space
  if(copyout(p->pagetable, user_virtual_addr, (char *)&st, sizeof(st))  < 0){
    printf("sysinfo: Copyout failed\n"); // 这里不知道为什么会copyout返回-1，但是又成功通过了test，待后面学习后再确认
    return -1;
  }

  return 0;
}