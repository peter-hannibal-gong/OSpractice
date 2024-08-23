#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#define READ 0
#define WRITE 1

void primes(int* left_pipe){
    // 关闭左管道的write端口
    close(left_pipe[WRITE]);
    // 读取第一个值
    int first = 0;
    if(read(left_pipe[READ],&first,sizeof(int)) != sizeof(int)){
        exit(0);
    }
    // 按照要求打印
    printf("prime %d\n",first);
    // 创建管道
    int right_pipe[2];
    pipe(right_pipe);
    // 从左管道读入并写到右管道中
    int num;
    while(read(left_pipe[READ], &num,sizeof(int) == sizeof(int))){
        if(num%first != 0){
            // 将素数写入右管道中
            write(right_pipe[WRITE],&num, sizeof(int));
        }
    }
    // 创建子进程
    if(fork() == 0){
        primes(right_pipe);
    }
    else{
        close(left_pipe[READ]);
        close(right_pipe[READ]);
        close(right_pipe[WRITE]);
        // 等待子进程退出?
        wait(0);
    }
    exit(0);
}

int
main(int argc, char *argv[])
{
    // 定义一个管道
    int p[2];
    pipe(p);
    //从第一个进程开始,将2-35输入到子进程中
    for(int i = 2; i <= 35; i++){
        write(p[WRITE],&i,sizeof(int));
    }
    if(fork() == 0){
        // 子进程
        primes(p);
    }
    else{
        close(p[WRITE]);
        close(p[READ]);
        // 等待子进程退出?
        wait(0);
    }
    
    exit(0);
}
