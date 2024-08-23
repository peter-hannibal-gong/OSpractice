#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char* argv[])
{ 
    if(argc==2){
    	write(1, "sleep", 6);
    	int second = atoi(argv[1]);
    	sleep(second*10);
	}
	else if(argc==1){
		write(1, "error:forget to input parameter", strlen("error:forget to input parameter"));
	}
	else if(argc==0){
		write(1, "error:argc == 0", strlen("error:argc == 0"));
	}
	else
	    write(1, "error:else", strlen("error:else"));
    write(1, "\n", 1);
    exit(0);
}
