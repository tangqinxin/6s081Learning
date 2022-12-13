#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

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
  backtrace(); // tm add
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

uint64
sys_sigalarm(void) {
  printf("sys_sigalarm is called\n");
  //因为argaddr实现里有解引用操作，所以这里用&myproc()而不是myproc()；最终其实是想p->fn_handler=a1寄存器值
  if(argint(0, &myproc()->ticks_target)<0 || argaddr(1, &myproc()->fn_handler) <0 ) {
    return -1;
  }
  return 0;
}

uint64
sys_sigreturn(void) {
  printf("sys_sigreturn is called\n");
  /*
  * 注意，这里会恢复现场。整个流程如下：
  * userspace->sigreturn->trap->usertrap()->系统调用陷入内核->分析陷入原因->syscall()->sys_sigreturn()->替换trapframe，然后返回到usertrap()->usertrapret()返回
  * 以上过程只有trapframe的a0返回值会修改，其他不变，因此不会受到影响
  */
  struct proc* p =  myproc();
  *p->trapframe = *p->alarm_trapframe;
  p->is_alarming = 0;
  return 0;
}