/*
 * This program is meant as a wrapper around a dedicated openttd-server
 * (http://www.openttd.org)
 * 
 */

#include <string.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include <signal.h>
#include <time.h>

#ifdef __dietlibc__
#include <write12.h>
#else
static inline int __write1(const char*s) { return write(1,s,strlen(s)); }
static inline int __write2(const char*s) { return write(2,s,strlen(s)); }
#endif

int mf(char* name){
  struct stat fifostat;
  mode_t mode=0666;
  if(stat(name, &fifostat) != -1) {
    // do some crap
  } else { // does not yet exist
    if (mknod(name, S_IFIFO | mode, 0))
      return -1;
  }
  return 0;
}

int tf(char *name){
  int fd;
  if ((fd=open(name, O_RDONLY))==-1) return errno;
  close(fd);
  return 0;
}

pid_t pid;

int sighandle_child(){
  int status;
  if (wait(&status)==pid){
    __write2("Our child exited\n");
    exit(-1);
  }
  return 0;
}

int sighandle_term(){
  __write2("Cought SIGTERM, try to terminate our child\n");
  kill(pid, SIGTERM);
  return 0;
}

int main(int argc, char **argv){
  int fd1, fd2, fdc, i, fd[2];
  char buf[1024];
  time_t t=time(NULL);
  int uid, gid;
  char *wkd, *log, *errlog;

  if (argc==6) {
    uid=atoi(argv[1]);
    gid=atoi(argv[2]);
    wkd=argv[3];
    log=argv[4];
    errlog=argv[5];

    if (chdir(wkd)==-1) {
      perror("Unable to chdir");
      return -1;
    }
    if (chroot(wkd)==-1){
      perror("Unable to chroot");
      return -1;
    }
    if (setgid(gid)==-1){
      perror("Unable to drop priviledges");
      return -1;
    }
    if (setuid(uid)==-1){
      perror("Unable to drop priviledges");
      return -1;
    }
    if (mf(".ctrl")) {
      perror("Unable to create control-fifo");
      return -1;
    }

    if (!tf(log)) {
      snprintf(buf, 512, "%s.%i", log, t);
      if (link(log, buf)==-1){
	perror("Unable to move logfile");
	return -1;
      }
      unlink(log);
    }

    if (!tf(errlog)) {
      snprintf(buf, 512, "%s.%i", errlog, t);
      if (link(errlog, buf)==-1){
	perror("Unable to move errlogfile");
	return -1;
      }
      unlink(log);
    }

    if (pipe(fd)==-1) {
      perror("Unable to create pipe");
      return -1;
    }

    pid=fork();
    if (pid==-1){
      perror("fork() failed");
      return -1;
    }

    if (pid==0) {
      close(1);
      fd1=open(log, O_RDWR|O_CREAT|O_TRUNC, 0644);
      close(2);
      fd2=open(errlog, O_RDWR|O_CREAT|O_TRUNC, 0644);
      if (fd1==-1 || fd2==-1){
	perror("Unable to redirect to logfile");
	return -1;
      }

      close(fd[1]);
      close(0);
      i=dup(fd[0]);
      close(fd[0]);
      execlp("/openttd", "openttd", "-D", 0);
      //execlp is not supposed to return...
      perror("execlp() failed");
    } else {
      close(fd[0]); // we don't need read
      close(1);
      i=dup(fd[1]);

      close(fd[1]);
      fdc=open(".ctrl", O_RDWR);
      if (fdc==-1){
	perror("Unable to open control fifo");
	return -1;
      }
      for (;;){
        signal(SIGCHLD, sighandle_child);
	signal(SIGTERM, sighandle_term);
	i=read(fdc, buf, 1024);
	buf[i]='\0';
	__write1(buf);
      }
    }
  } else {
    __write1("Usage: ttdsrv uid gid openttd-directory log errlog\n");
    return -1;
  }
  return 0;
}
