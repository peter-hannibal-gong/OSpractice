#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

// 用于分割出带有/路径的文件名
// input:a/b/filename
// output:filename
char*
my_fmtname(char *path)
{
  char *p;
  // Find first character after last slash.
  for(p=path+strlen(path); p >= path && *p != '/'; p--)
    ;
  p++;

  return p;
}

void find(char* path, char* target_filename){
    // 模仿ls函数
    char buf[512], *p;  // 用于存储文件或目录路径的缓冲区。
    int fd;             // 文件描述符，用于后续的文件或目录操作。
    struct dirent de;   // 一个 dirent 结构体，用于读取目录中的条目。
    struct stat st;     // 一个 stat 结构体，用于存储文件或目录的状态信息。
    // 尝试打开目录
    if((fd = open(path, 0)) < 0){
        fprintf(2, "find: cannot open %s\n", path);
        return;
    }
    // 尝试获取状态信息
    if(fstat(fd, &st) < 0){
        fprintf(2, "find: cannot stat %s\n", path);
        close(fd);
        return;
    }

    switch(st.type){
    // 如果path指向的是一个文件，判断该文件与目标文件名是否一样
    case T_FILE:
       
       if(strcmp(my_fmtname(path),target_filename) == 0){
        // 表示文件名一样
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
        // 循环读取目录中的每个条目，并检查其索引节点号
        while(read(fd, &de, sizeof(de)) == sizeof(de)){
            if(de.inum == 0)
                continue;
            // 过滤 . ..
            if(!strcmp(de.name,".") || !strcmp(de.name,".."))
                continue;
            memmove(p, de.name, DIRSIZ);
            p[DIRSIZ] = 0;
            if(stat(buf, &st) < 0){
                printf("find: cannot stat %s\n", buf);
                continue;
            }
            //递归！！！
            find(buf, target_filename);
        }
        break;
    }
    close(fd);
}

void
main(int argc, char *argv[])
{
  
    if(argc != 3){
        printf("usage: %s <dir> <filename>",argv[0]);
        exit(0);
    }
    find(argv[1], argv[2]);
    exit(0);
}

