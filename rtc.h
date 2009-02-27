#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <linux/rtc.h>
#if 0
#include <spinlock.h>
#include <linux/mc146818rtc.h>
#endif

#include <sys/ioctl.h>
#include <fcntl.h>

void 
set_rtc(int fd, int hz)
{

  int re;

  if(hz == 1){
    re = ioctl(fd, RTC_UIE_ON, 0);
    if(re < 0){
      perror("ioctl RTC_UIE_ON");
      exit(1);
    }
  }else{
    re = ioctl(fd, RTC_IRQP_SET, hz);
    if(re < 0){
      fprintf(stderr,"hz = %d \n",hz);
      perror("ioctl RTC_IRQP_SET");
      exit(1);
    }
    re = ioctl(fd, RTC_PIE_ON, 0);
    if(re < 0){
      perror("ioctl RTC_PIE_ON");
      exit(1);
    }
  }
}

int 
open_rtc(int hz)
{

  int fd;

  fd = open("/dev/rtc",O_RDONLY);
  if(fd <0){
    perror("RTC open");
    exit(1);
  }

  set_rtc(fd,hz);

  return fd;

}

void 
close_rtc(int fd)
{
  close(fd);
}

void 
stop_rtc(int fd, int hz)
{
  int re;

  if(hz == 1)
    re = ioctl(fd, RTC_UIE_OFF, 0);
  else
    re = ioctl(fd, RTC_PIE_OFF, 0);
}

void 
wait_rtc(int fd)
{
  long ttt;
  read(fd,&ttt,sizeof(long));
}
