#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define MAXNUM 36
#define READSIDE 0
#define WRITESIDE 1
#define ONE '1'
#define ZERO '0'


//---------------------------------------version 1-----------------------------//
// void prime(int fd[]){
//   int val=-1;
//   int firstVal;
//   int p[2];
  
//   int n=read(fd[READSIDE],&firstVal,sizeof(int));
//   if(n==0){
//     // when the write side of the pipe is closed, the read system call will return 0 immediately!
//     return;
//   }

//   printf("prime %d\n",firstVal); // first val must be primes;
  
//   pipe(p); // build pipe
//   // child process
//   if(fork()==0){
//     close(p[WRITESIDE]);
//     prime(p);
//     close(p[READSIDE]);
//     exit(0);
//   }

//   // parent process
//   close(p[READSIDE]);
//   while(read(fd[READSIDE],&val,sizeof(int))>0){
//     if(val%firstVal!=0){
//       write(p[WRITESIDE],&val,sizeof(int));
//     }
//   }
//   close(p[WRITESIDE]);
//   wait(0);
// }

// int
// main(int argc, char *argv[])
// {
  
//   int fd[2];

//   pipe(fd);

//   // child process
//   if(fork()==0){
//     close(fd[WRITESIDE]);
//     prime(fd);
//     close(fd[READSIDE]);
//     exit(0);
//   }

//   close(fd[READSIDE]);
//   // parent process
//   for(int i=2;i<=MAXNUM;i++){
//     write(fd[WRITESIDE],&i,sizeof(int));
//   }
//   close(fd[WRITESIDE]);
//   wait(0); // wait for child process exit

//   exit(0);
// }


//---------------------------------------version 2-----------------------------//
void prime(int fd[],int start){
  char buffer[MAXNUM];
  int p[2];

  read(fd[READSIDE],buffer, MAXNUM * sizeof(char));

  int curPrime=start;
  for(int i=start;i<MAXNUM;i++){
    if(buffer[i]!=ZERO){
      printf("prime %d\n",i); // first val must be primes;
      curPrime=i;
      break;
    }
  }
  int cnt=0;
  for(int i=curPrime+1;i<MAXNUM;i++){
    if(buffer[i]!=ZERO){
      cnt++;
    }
  }
  if(cnt==0){
    // no number left is prime, no need to fork
    return;
  }
  
  pipe(p); // build pipe
  
  // child process
  if(fork()==0){
    close(p[WRITESIDE]);
    prime(p,curPrime+1);
    close(p[READSIDE]);
    exit(0);
  }
  
  // parent process
  close(p[READSIDE]);
  for(int i=curPrime;i<MAXNUM;i++){
      if(i%curPrime==0){
      buffer[i]=ZERO; // to be judge in the next place
    }
  }

  write(p[WRITESIDE],buffer,MAXNUM * sizeof(char));
  close(p[WRITESIDE]);
  wait(0);
}

int
main(int argc, char *argv[])
{
  
  int fd[2];
  char buffer[MAXNUM];
  for(int i=2;i<MAXNUM;i++){
    buffer[i]=ONE;
  }

  pipe(fd);

  // child process
  if(fork()==0){
    close(fd[WRITESIDE]);
    prime(fd,2);
    close(fd[READSIDE]);
    exit(0);
  }

  close(fd[READSIDE]);
  // parent process
  write(fd[WRITESIDE],buffer,MAXNUM * sizeof(char));
  
  close(fd[WRITESIDE]);
  wait(0); // wait for child process exit

  exit(0);
}
