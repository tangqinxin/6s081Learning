#include "kernel/types.h"
#include "user/user.h"
#include "kernel/param.h"

#define BUFFERSIZE 128
#define MAXARGSNUM 32

int
main(int argc, char *argv[])
{

  if(argc < 2){
    printf("usage xargs [cmd] [params...]\n",argc);
    exit(0);
  }
  
  char* params[MAXARGSNUM];
  for(int i=1;i<argc;i++){
    params[i-1]=argv[i];
  }

  char buffer[BUFFERSIZE];
  char* p=buffer;
  int n=0;
  while((n=read(0,p,1))>0){
    if(*p=='\n'){
      *p=0;
      params[argc-1]=buffer; // argv[1]是cmd,放params[0]，argv[2]放params[1];因此其他参数放完应该是最后的argv[argc-1]对应为params[argc-2];因此这里放在params[argc-1]
      if(fork()==0){
        exec(argv[1],params); // 这里的params必须把第0个参数即命令cmd也带上，也就是跟main(argc,argv)里面的argv是一样的
        printf("exec %s failed\n",argv[1]);
        exit(1);
      }
      wait(0);
      p=buffer;
    }else{
      p++;
    }
  }
  
  exit(0);
}