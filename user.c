#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <poll.h>
#include <pthread.h>
#include <setjmp.h>
#include <sys/ioctl.h>
#include <time.h>
#include <linux/input.h>

#define CONFIG 'k'
#define SIGNAL SIGUSR1

void handler();  // signal handler function
void *mouse_read(); //mouse thread function
int sleep_millisecond( float tmsc); //sleep function
int error_function(void); //error function

struct pollfd poll_echo = {0}; //poll fd structure
struct input_event inev; //input event structure
struct sigaction act; //signalstructure
struct user_input *input_set; 

int val = 0; //to check sigset return value
int s = 1; //to check is signal is being called
int re;
int mouse_priority = 0; // mouse priority for mouse 
int fd_mouse; // file decriptor for mouse 
int policy = SCHED_FIFO; //policy 
float d_c; //variable for duty cycle

sigjmp_buf jmpbuf; //jump buffer
pthread_t mouse;  // mouse thread id
pthread_t main_id; // main thread id

/**User input structure**/
struct user_input{
int ip1;
int ip2;
int ip3;
long duty_cycle;
};

int main()
{
   int fd,ret; 
   main_id = pthread_self();

   act.sa_flags = 0;           //setting user signal structure
   act.sa_sigaction = handler;
   sigemptyset(&act.sa_mask);
   sigaction(SIGNAL,&act,NULL);

   fd_mouse = open("/dev/input/event2",O_RDONLY); //open mouse file
   poll_echo.fd = fd_mouse;
   poll_echo.events = POLLIN; 
   poll_echo.revents = 0;

   pthread_attr_t mouse_attr;                           //Creating mouse thread with highest priority 
   struct sched_param mouse_param;
   pthread_attr_init(&mouse_attr);
   mouse_param.sched_priority=mouse_priority;
   pthread_attr_setschedpolicy(&mouse_attr,policy);
   pthread_attr_setschedparam(&mouse_attr,&mouse_param);
   pthread_create(&mouse,&mouse_attr,mouse_read,NULL); //mouse thread creation 
    
   /*** millisecon sleep function***/
   int sleep_millisecond( float tmsc) 
  {
   float millisecond = tmsc;
   struct timespec req = {0};
   req.tv_sec = 0;
   req.tv_nsec = millisecond*1000000L;

   nanosleep(&req,( struct timespec*)NULL);
   return 0;    
  }

   input_set = (struct user_input*) malloc(sizeof(struct user_input));
   //Taking user inputs
   printf("Please enter input Pin1\n");
   scanf("%d",&input_set->ip1);
   printf("Pin1 = %d\n",input_set->ip1);
   printf("Please enter input Pin2\n");
   scanf("%d",&input_set->ip2);
   printf("Pin2 = %d\n",input_set->ip2);
   printf("Please enter input Pin3\n");
   scanf("%d",&input_set->ip3);
   printf("Pin3 = %d\n",input_set->ip3);
   printf("Please enter Duty cycle\n");
   scanf("%f",&d_c);
   input_set->duty_cycle = ((d_c/100)*20000000);
   printf("Duty Cycle = %lu\n",input_set->duty_cycle);

   ret = error_function();

   if(ret == -1)
    {
      errno = EINVAL;
      printf("Please run again\n");
      exit(0);
    }
   
   fd = open("/dev/RGBLed", O_RDWR);

   if (fd < 0 )
   { printf("Can not open device file.\n");		
     return 0;
   }
   else 
   {
    ioctl(fd,CONFIG,input_set);

    while(1)
   {
     val = sigsetjmp(jmpbuf,1); //jumping to this point when a click is detected
     if (val == 0){
     write(fd,"0",1);//OFF 000
     sleep_millisecond(500);
     write(fd,"1",1);       //RED 001
     sleep_millisecond(500); 
     write(fd,"2",1);       //GREEN 010
     sleep_millisecond(500);
     write(fd,"4",1);       //BLUE 100    
     sleep_millisecond(500); 
     write(fd,"3",1);       //RG 011
     sleep_millisecond(500);
     write(fd,"6",1);       //GB 110
     sleep_millisecond(500); 
     write(fd,"5",1);       //RB 101
     sleep_millisecond(500);
     write(fd,"7",1);       //RGB 111
     sleep_millisecond(500); 
    }
   }
  }
}

/***mouse thread function***/
void *mouse_read() 
{
   poll(&poll_echo,1,-1); //polling mouse

  if(poll_echo.revents & POLLIN) // mouse click detection
  {
   while(1)
   {
     read(poll_echo.fd,&inev,sizeof(struct input_event));

     if((inev.code==BTN_RIGHT || inev.code==BTN_LEFT) && inev.value==1)
        {
          printf("Mouse clicked");
          re = pthread_kill(main_id,SIGNAL); // send signal to main thread    
        }
    }
   }
  poll_echo.revents = 0;
  return 0;
}
/***signal hadler function***/
void handler() 
{
  printf("Signal %d\n",s);
  sigaction(SIGNAL,&act,NULL);
  s++;
  siglongjmp(jmpbuf, 1);
}

/***Error handling function***/
int error_function(void)
{
   if(!((input_set->ip1 == 0) || (input_set->ip1 == 1) || (input_set->ip1 == 2) || (input_set->ip1 == 3) || (input_set->ip1 == 10) || (input_set->ip1 == 15)))
    {
      printf("error:invalid input pin1 \n");
      return -1;
     }
   else if(!((input_set->ip2 == 0) || (input_set->ip2 == 1) || (input_set->ip2 == 2) || (input_set->ip2 == 3) || (input_set->ip2 == 10) || (input_set->ip2 == 15)))
    {
      printf("error:invalid input pin2 \n");
      return -1;
     }
   else if(!((input_set->ip3 == 0) || (input_set->ip3 == 1) || (input_set->ip3 == 2) || (input_set->ip3 == 3) || (input_set->ip3 == 10) ||    (input_set->ip3 == 15)))
    {
      printf("error:invalid input pin3 \n");
      return -1;
     }
   else if(((d_c > 100) || (d_c < 0)))
    {
      printf("error:invalid duty cycle \n");
      return-1;
     }
   else 
     {
      return 0;
     }
   return 0;
}

  
