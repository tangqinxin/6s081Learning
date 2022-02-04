#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define BUFFERSIZE 100

int
main(int argc, char *argv[])
{
  int p_parentWrite[2];
  int p_childWrite[2];

  // 1 build 2 pipes
  pipe(p_parentWrite);
  pipe(p_childWrite);

  // 2 child process
  if(fork()==0){
    int child_pid=getpid();
    // child process will read from the pipe p_parentWrite, and write in the pipe p_childWrite
    // so child process should close the write end in p_parentWrite and the read end in p_childWrite
    close(p_parentWrite[1]);
    close(p_childWrite[0]);
    char buf[BUFFERSIZE];
    int n = read(p_parentWrite[0],buf,BUFFERSIZE);
    buf[n]='\0';
    // printf("child receive %s\n",buf); // for debug
    printf("%d: received ping\n",child_pid);
    write(p_childWrite[1],buf,n);

    close(p_parentWrite[0]);
    close(p_childWrite[1]);
    exit(0);
  }

  // 3 parent process will read from the pipe p_childWrite, and write in the pipe p_parentWrite
  // so parent process should close the read end in p_parentWrite and the write end in p_childWrite
  int parent_pid=getpid();
  close(p_parentWrite[0]);
  close(p_childWrite[1]);
  write(p_parentWrite[1],"test",4);
  wait(0);
  char buf[BUFFERSIZE];
  int n = read(p_childWrite[0],buf,BUFFERSIZE);
  buf[n]='\0';
  // printf("parent receive %s\n",buf); // for debug
  printf("%d: received pong\n",parent_pid);

  close(p_parentWrite[1]);
  close(p_childWrite[0]);
  exit(0);
}
