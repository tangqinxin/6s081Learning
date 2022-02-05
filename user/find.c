#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

char*
lastname(char *path)
{
  // 这个函数改编自ls的fmtname函数，本质上就是为了拿到最后一个/后面的文件名
  char *p;
  // Find first character after last slash.
  for(p=path+strlen(path); p >= path && *p != '/'; p--)
    ;
  p++;

  return p;
}

void find(char *path,char* filename)
{
  char buf[512], *p;
  int fd;
  struct dirent de;
  struct stat st;

  if((fd = open(path, 0)) < 0){
    fprintf(2, "find: cannot open %s\n", path);
    return;
  }

  if(fstat(fd, &st) < 0){
    fprintf(2, "find: cannot stat %s\n", path);
    close(fd);
    return;
  }

  switch(st.type){
  case T_FILE:
    if(strcmp(lastname(path),filename)==0){
      printf("%s\n",path);
    }
    break;

  case T_DIR:
    if(strlen(path) + 1 + DIRSIZ + 1 > sizeof buf){
      printf("find: path too long\n");
      break;
    }
    strcpy(buf, path);
    p = buf+strlen(buf);
    *p++ = '/';
    // 这里的重点是理解，文件夹文件符里面的是什么内容，实际上就是子目录的信息，每个信息都是相同结构体dirent
    while(read(fd, &de, sizeof(de)) == sizeof(de)){ // 这句话其实就是读取目录的子结点内容，读取到的文件名保存在de.name里面
      if(de.inum == 0)
        continue;
      if(strcmp(de.name,".")==0||strcmp(de.name,"..")==0){
        continue; // 我们不对"."和".."递归，因此这里对于文件夹文件符读取到的子结点
      }
      memmove(p, de.name, DIRSIZ);
      p[DIRSIZ] = 0;
      if(stat(buf, &st) < 0){
        printf("find: cannot stat %s\n", buf);
        continue;
      }
      find(buf,filename);
    }
    break;
  }
  close(fd);
}

int
main(int argc, char *argv[])
{
  if(argc != 3){
    printf("usage: find [path] [filename]\n");
    exit(0);
  }

  find(argv[1],argv[2]);
  exit(0);
}
