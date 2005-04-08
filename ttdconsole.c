#include <string.h>
#include <fcntl.h>
#include <stdio.h>
#include <write12.h>

int main(int argc, char **argv){
  int fd,i;
  char buf[1024];
  if (argc==2) {
    if (chdir(argv[1])==-1){
      perror("Unable to chdir");
      return -1;
    }

    fd=open(".ctrl", O_RDWR);
    if (fd==-1) {
      perror("Unable to open fifo");
      return -1;
    }
    while (strcmp(buf, "exit")){
      __write1("> ");
      if ((i=read(0,buf,512))==-1) return -1;
      buf[i]='\0';
      write(fd, buf, strlen(buf));
    }
  } 
  return 0;
}
