/*
 * ttdconsole (c) 2005 by Bernd 'Aard' Wachter
 * You may change and redistribute this file under the terms and conditions
 * of the GNU GPL v2
 *
 * This program opens a console to a dedicated openttd-server running with
 * ttdsrv. It uses the control fifo polled by ttdsrv for output, the redirected
 * server-log as input
 */

#include <string.h>
#include <fcntl.h>
#include <stdio.h>
#include <write12.h>
#include <sys/poll.h>
#include <sys/stat.h>
#include <time.h>

int main(int argc, char **argv){
  int fd_fifo, fd_log, i;
  struct stat filestat;
  off_t filesize;
  char buf[1024];
  struct pollfd pfd[2];
  struct timespec ts_req;

  if (argc==2) {
    if (chdir(argv[1])==-1){
      perror("Unable to chdir");
      return -1;
    }

    fd_fifo=open(".ctrl", O_RDWR);
    if (fd_fifo==-1) {
      perror("Unable to open fifo");
      return -1;
    }

    fd_log=open("log", O_RDONLY);
    if (fd_log==-1) {
      perror("Unable to open log");
      return -1;
    }

    pfd[0].fd=0;
    pfd[0].events=POLLRDNORM | POLLIN;
    pfd[1].fd=fd_log;
    pfd[1].events=POLLRDNORM | POLLIN;
    
    fstat(pfd[1].fd, &filestat);
    filesize=filestat.st_size;
    lseek(pfd[1].fd, filesize-1024, SEEK_SET);
    ts_req.tv_nsec=1000;
    __write1("> ");
    while (strcmp(buf, "exit\n")){
      poll (pfd, 2, -1);
      if (pfd[0].revents & (POLLRDNORM | POLLIN)){
	if ((i=read(0,buf,1024))==-1) return -1;
	buf[i]='\0';
	write(fd_fifo, buf, strlen(buf));
	if (buf[1]=='\0') __write1("> ");
      } 
      if (pfd[1].revents & (POLLRDNORM | POLLIN)){
	fstat(pfd[1].fd, &filestat);
	if (filesize<filestat.st_size){
	  filesize=filestat.st_size;
	  if ((i=read(pfd[1].fd,buf,1024))==-1) return -1;
	  buf[i]='\0';
	  write(1, buf, strlen(buf));
          __write1("> ");
	} else {
	  nanosleep(&ts_req, NULL);
	}
      }

    }
  } 
  return 0;
}
