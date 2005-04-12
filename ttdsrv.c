/*
 * ttdsrv (c) 2005 by Bernd 'Aard' Wachter <bwachter-ttd@lart.info>
 * You may change and redistribute this file under the terms and conditions
 * of the GNU GPL v2
 *
 * This program provides a wrapper around a dedicated openttd-server
 * (http://www.openttd.org). It has the following features:
 * - puts the server in a chroot
 * - drops priviledges
 * - redirect servers stderr and stdout to files; move logfiles on restart
 * - terminate the wrapper if the child exits, or terminate the child if the 
 *   wrapper gets SIGTERM
 * - move the latest autosave to /startup.sav and load it (works with nightly/0.4.0)
 * - read from a control fifo, and write everything showing up there to the server
 * 
 */

#include <string.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <dirent.h>
#include <stdlib.h>

#ifdef __dietlibc__
#include <write12.h>
#else
static inline int __write1(const char*s) { return write(1,s,strlen(s)); }
static inline int __write2(const char*s) { return write(2,s,strlen(s)); }
#endif

int mf(char* name){
  // test if a fifo exists and create it if it doesn't
  struct stat fifostat;
  mode_t mode=0666;
  if(stat(name, &fifostat) != -1) {
    // the fifo is already there. maybe do some crap
  } else { 
    if (mknod(name, S_IFIFO | mode, 0))
      return -1;
  }
  return 0;
}

int tf(char *name){
  // test if a file exists by opening and closing it
  int fd;
  if ((fd=open(name, O_RDONLY))==-1) return errno;
  close(fd);
  return 0;
}

int get_autosave(){
  //return 0 for normal startup, 1 for autosave
  DIR* dir_ptr;
  struct dirent *temp;
  time_t tmptime=0;
  struct stat filestat;

  if (chdir("/save/autosave")==-1){
    perror("Unable to chdir to autosaves");
    return 0;
  }

  if ((dir_ptr=opendir("."))==NULL){
    perror("Unable to open dir");
    chdir("/");
    return 0; //shit happens. neither cookie nor autosave...
  }

  for (temp=readdir(dir_ptr); temp!=NULL; temp=readdir(dir_ptr)){
    if (strncmp(temp->d_name+strlen(temp->d_name)-4, ".sav", 4)) continue;
    if (!strcmp(temp->d_name, ".")) continue;
    if (!strcmp(temp->d_name, "..")) continue;
    // if the current file is a .sav and newer than the time in tmptime create
    // a link to /startup.sav and unlink the original file. The unlink is just 
    // to make sure nothing gets confused when startup.sav changes because openttd
    // writes another autosave
    stat(temp->d_name, &filestat);
    if (filestat.st_mtime > tmptime) {
      unlink("/startup.sav");
      if (link(temp->d_name, "/startup.sav")==0) {
        tmptime=filestat.st_mtime;
	unlink(temp->d_name);
      }
    }
  }
  chdir("/");
  if (tmptime > 0) return 1;
  else return 0;
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
  // we'll just send TERM to the child, which will make us catch
  // SIGCHLD. Not nice, but effective ;)
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

    // the usual startup foo
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

    // we'd unlink it later anyway, just to be sure
    if (!tf("startup.sav")) unlink("startup.sav");

    // move the old logs to new, cryptical names.
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

    // houston, prepare fork. open pipe.
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
      // now for the fun part... open will use the first free file descriptor
      // -> if we close 1 we got stdin redirected to the now filedescriptor, ..
      close(1);
      fd1=open(log, O_RDWR|O_CREAT|O_TRUNC, 0644);
      close(2);
      fd2=open(errlog, O_RDWR|O_CREAT|O_TRUNC, 0644);
      if (fd1==-1 || fd2==-1){
	perror("Unable to redirect to logfile");
	return -1;
      }

      // I'll leave this one as an excercise for you. Almost the same as 
      // explained above
      close(fd[1]);
      close(0);
      i=dup(fd[0]);
      close(fd[0]);

      // check if we can use some autosave. This will work with OpenTTD 0.4.0.
      // OpenTTD 0.3.6 silently ignores -g, therefore we can just add it now.
      if (get_autosave())
	execlp("/openttd", "openttd", "-D", "-g", "startup.sav", 0);
      else
        execlp("/openttd", "openttd", "-D", 0);
      //execlp is not supposed to return...baaad, baaad server
      perror("execlp() failed");
    } else {
      // we don't need read on the pipe. The other stuff is the redirection 
      // thing I explained above.
      close(fd[0]); 
      close(1);
      i=dup(fd[1]);
      close(fd[1]);

      // open our control fifo...
      fdc=open(".ctrl", O_RDWR);
      if (fdc==-1){
	perror("Unable to open control fifo");
	return -1;
      }

      for (;;){
	// set up signal handlers
        signal(SIGCHLD, sighandle_child);
	signal(SIGTERM, sighandle_term);
	// and just read. We don't care that's blocking since we only have
	// to read from one file descriptor
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
