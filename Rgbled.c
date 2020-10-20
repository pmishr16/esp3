#include<linux/module.h>
#include<linux/kernel.h>
#include<linux/fs.h>
#include<linux/cdev.h>
#include<linux/types.h>
#include<linux/slab.h>
#include<asm/uaccess.h>
#include<linux/device.h>
#include<linux/string.h>
#include<linux/jiffies.h>
#include<linux/init.h>
#include<linux/ktime.h>
#include<linux/hrtimer.h>
#include<linux/moduleparam.h>
#include <linux/gpio.h>

#define DEVICE_NAME "RGBLed" /* name of created device*/
#define CONFIG 'k'/*IOCTL COMMAND*/

int RED_LED,BLUE_LED,GREEN_LED,io_1,io_2,io_3; /*Variables to configure pins*/
unsigned long duty_cycle; /*Variable for duty cycle*/
int i,k,n,x,count; /*Variable to run loops and count timer calls*/
int in_val; /*input integer for RGB*/
char dtm[4];/*array to copy in_val*/
int in_array[8]; /*input array for write calls*/
ktime_t ktime, curr_time, period; /* variable for timer in ktime_t format*/

static dev_t rgb_dev_number; /*alloted device number*/
struct class *rgb_dev_class; /* Tie with the device model */
static struct device *rgb_dev_device;
struct user_input *istruct; /* user input structure pointer */
static struct hrtimer timer; /* High resolution timer structure*/

int rgb_driver_open(struct inode *inode, struct file *file);/***Open RGB Driver***/
int rgb_driver_release(struct inode *inode, struct file *file);/**Release RGB Driver**/
ssize_t rgb_driver_write(struct file *file, const char *buf, size_t count, loff_t *ppos);/***RGB Driver Write function***/
static long rgb_driver_ioctl(struct file *filp, unsigned int cmd, unsigned long arg);/**RGB Driver IOCTL function**/
int configure_led(void);/***Function to set gpio pins according to user input***/
static enum hrtimer_restart timer_callback(struct hrtimer *timer);/***HR timer callback function***/ 

/** Character device structure**/
struct rgb_dev
{
    struct cdev cdev; /*Assigning a cdev structure*/
    char name[20]; /*name of device*/
    char in_string[256]; /*buffer for input string*/
} *rgb_devp;

/**File operations structure**/
static struct file_operations rgb_fops=
{
    .owner=THIS_MODULE,/*Owner*/
    .open=rgb_driver_open,
    .release=rgb_driver_release,
    .write=rgb_driver_write,
    .unlocked_ioctl=rgb_driver_ioctl,
};

/**File operations structure**/
struct user_input
{
    int ip1;
    int ip2;
    int ip3;
    long duty_cycle;
};

/***HR timer callback function***/ 
static enum hrtimer_restart timer_callback(struct hrtimer *timer)
{
      count++;

      if ( count == 51)
      {
        return HRTIMER_NORESTART;
      }

      if(count%2 != 0)
      {
        curr_time = ktime_get();
        period = ktime_set(0,20000000-duty_cycle);
        gpio_set_value(RED_LED,0);
        gpio_set_value(GREEN_LED,0);
        gpio_set_value(BLUE_LED,0);
        hrtimer_forward(timer, curr_time , period); //extend timer 
        return HRTIMER_RESTART;
      }

      if(count%2 == 0)
      {
        curr_time = ktime_get();
        period = ktime_set(0,duty_cycle);
        gpio_set_value(RED_LED,in_array[7]);
        gpio_set_value(GREEN_LED,in_array[6]);
        gpio_set_value(BLUE_LED,in_array[5]);
        hrtimer_forward(timer, curr_time , period); //extend timer 
        return HRTIMER_RESTART;
      }      
       return 0;
}

/***Open RGB Driver***/
int rgb_driver_open(struct inode *inode, struct file *file)
{
    struct rgb_dev *rgb_devp;

    rgb_devp = container_of(inode->i_cdev, struct rgb_dev, cdev);

    file->private_data = rgb_devp;
    printk("\n%s is open \n", rgb_devp->name);
    return 0;
}

/**Release RGB Driver**/
int rgb_driver_release(struct inode *inode, struct file *file)
{
    struct rgb_dev *rgb_devp = file->private_data;
    
    //freeing all GPIO PINS
    gpio_free(RED_LED);
    gpio_free(GREEN_LED);
    gpio_free(BLUE_LED);
   
    if(RED_LED==11 || GREEN_LED==11 || BLUE_LED==11)
    {
        gpio_free(32);  
    }
    if(RED_LED==12 || GREEN_LED==12 || BLUE_LED==12)
    {
        gpio_free(28);
        gpio_free(45);
    }
    if(RED_LED==13 || GREEN_LED==13 || BLUE_LED==13)
    {       
        gpio_free(34);
        gpio_free(77);
    }
    if(RED_LED==14 || GREEN_LED==14 || BLUE_LED==14)
    {
        gpio_free(16);
        gpio_free(76);  
        gpio_free(64);
    }
    if(RED_LED==10 || GREEN_LED==10 || BLUE_LED==10)
    { 
        gpio_free(26);
        gpio_free(74);      
    }
    if(RED_LED==15 || GREEN_LED==15 || BLUE_LED==15)
    {
        gpio_free(42);
    }

    printk("\n%s closed.\n", rgb_devp->name);

    return 0;
}

/***RGB Driver Write function***/
ssize_t rgb_driver_write(struct file *file, const char *buf,
                         size_t count, loff_t *ppos)
{
    struct rgb_dev *rgb_devp = file->private_data;

    get_user(rgb_devp->in_string[0], buf);          
    printk("Writing -- %s \n", rgb_devp->in_string);
      
    strcpy( dtm, rgb_devp->in_string );
    sscanf( dtm, "%d",&in_val);
    printk("%d\n", in_val);
    n = 0;
    for(i=7;i>=0;i--)
    {
      k = in_val >> i;
    
      if(k & 1)
      {   
       printk("1");
       in_array[n] = 1;
       n++;
      }
      else
      {
       printk("0");
       in_array[n] = 0;
       n++;
      }
    }
      //check LSB values
      printk("\n");
      printk("%d", in_array[5]);
      printk("%d", in_array[6]);
      printk("%d", in_array[7]);
      printk("\n");
      
      //set RGB values with the LSB being Red color
      gpio_set_value(RED_LED,in_array[7]);
      gpio_set_value(GREEN_LED,in_array[6]);
      gpio_set_value(BLUE_LED,in_array[5]);

      count = 0; //reset timer count 
      printk("Starting timer");  
      hrtimer_start(&timer, ktime, HRTIMER_MODE_REL); //start timer
    
      return 0; 
}

/**RGB Driver IOCTL function**/
static long rgb_driver_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    switch (cmd)
    {
    case CONFIG:
    {  
        int ret_val;
        istruct = (struct user_input*) kzalloc(sizeof(struct user_input),GFP_KERNEL);
        printk("configuring led\n");
    
        ret_val = copy_from_user(istruct,(struct user_input*) arg,sizeof(struct user_input));
        if(ret_val != 0){
        printk("error");}

        //check user values
        io_1 = istruct->ip1;
        printk("Pin1 for RED = %d\n",io_1);
        io_2 = istruct->ip2;
        printk("Pin2 for GREEN = %d\n",io_2);
        io_3 = istruct->ip3;
        printk("Pin3 for BLUE = %d\n",io_3);
        duty_cycle = istruct->duty_cycle;
        printk("Duty cycle = %lu\n",duty_cycle);
 
        configure_led();

        ktime = ktime_set(0,duty_cycle); // set timer accordignto duty cycle as given by user 
        hrtimer_init(&timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);// init timer
        timer.function = &timer_callback;
        
        break;
    }
    default:
    {
        printk("Error IOCTL CONFIG");
        break;
    }
    }
    return 0;
}

/***Function to set gpio pins according to user input***/
int configure_led()
{
    // shield pin  -> linux pin
    if(io_1==0)
    {
        RED_LED=11;
    }
    else if (io_1==1)
    {
        RED_LED=12;
    }
    else if (io_1==2)
    {
        RED_LED=13;
    }
    else if (io_1==3)
    {
        RED_LED=14;
    }
    else if (io_1==10)
    {
        RED_LED=10;
    }
    else if (io_1==12)
    {
        RED_LED=15;
    }
    else
    {
        printk("Not a valid Pin");
    }
    if(io_2==0)
    {
        GREEN_LED=11;
    }
    else if (io_2==1)
    {
        GREEN_LED=12;
    }
    else if (io_2==2)
    {
        GREEN_LED=13;
    }
    else if (io_2==3)
    {
        GREEN_LED=14;
    }
    else if (io_2==10)
    {
        GREEN_LED=10;
    }
    else if (io_2==12)
    {
        GREEN_LED=15;
    }
    else
    {
        printk("Not a valid Pin");
    }
    if(io_3==0)
    {
        BLUE_LED=11;
    }
    else if (io_3==1)
    {
        BLUE_LED=12;
    }
    else if (io_3==2)
    {
        BLUE_LED=13;
    }
    else if (io_3==3)
    {
        BLUE_LED=14;
    }
    else if (io_3==10)
    {
        BLUE_LED=10;
    }
    else if (io_3==12)
    {
        BLUE_LED=15;
    }
    else
    {
        printk("Not a valid Pin");
    }
    
    //check configured values
    printk("Duty cycle=%lu\n",duty_cycle);
    printk("Red led=%d\n",RED_LED);
    printk("Green led=%d\n",GREEN_LED);
    printk("Blue led=%d\n",BLUE_LED);

    //setting pins at which RGB are connected
    gpio_request(RED_LED,"sysfs");
    gpio_export(RED_LED,false);
    gpio_direction_output(RED_LED,0);
    gpio_request(GREEN_LED,"sysfs");
    gpio_export(GREEN_LED,false);
    gpio_direction_output(GREEN_LED,0);   
    gpio_request(BLUE_LED,"sysfs");   
    gpio_export(BLUE_LED,false);  
    gpio_direction_output(BLUE_LED,0);

    //muxing pins
    if(RED_LED==11 || GREEN_LED==11 || BLUE_LED==11)
    {
        gpio_request(32,"sysfs");
        gpio_export(32,false);
        gpio_direction_output(32,0); 
    }
    if(RED_LED==12 || GREEN_LED==12 || BLUE_LED==12)
    {
        gpio_request(28,"sysfs");
        gpio_export(28,false);
        gpio_direction_output(28,0);
        gpio_request(45,"sysfs");
        gpio_export(45,false);
        gpio_direction_output(45,0);
    }
    if(RED_LED==13 || GREEN_LED==13 || BLUE_LED==13)
    {
        gpio_request(34,"sysfs");
        gpio_export(34,false);
        gpio_direction_output(34,0);
        gpio_request(77,"sysfs");
        gpio_export(77,false);
        gpio_direction_output(77,0);
    }
    if(RED_LED==14 || GREEN_LED==14 || BLUE_LED==14)
    {
        gpio_request(16,"sysfs");
        gpio_export(16,false);
        gpio_direction_output(16,0);
        gpio_request(76,"sysfs");
        gpio_export(76,false);
        gpio_direction_output(76,0);
        gpio_request(64,"sysfs");
        gpio_export(64,false);
        gpio_direction_output(64,0);
    }
    if(RED_LED==10 || GREEN_LED==10 || BLUE_LED==10)
    {
        gpio_request(26,"sysfs");
        gpio_export(26,false);
        gpio_direction_output(26,0);
        gpio_request(74,"sysfs");
        gpio_export(74,false);
        gpio_direction_output(74,0);
    }
    if(RED_LED==15 || GREEN_LED==15 || BLUE_LED==15)
    {
        gpio_request(42,"sysfs");
        gpio_export(42,false);
        gpio_direction_output(42,0);
    }
    
    return 0;
}

/***Module init function***/
int __init rgb_driver_init(void) 
{
    int ret;
    if(alloc_chrdev_region(&rgb_dev_number,0,1,DEVICE_NAME)<0)
    {
        printk (KERN_DEBUG "Can't register device\n");
        return -1;
    }

    rgb_dev_class=class_create(THIS_MODULE,DEVICE_NAME); 
    rgb_devp=kmalloc(sizeof(struct rgb_dev),GFP_KERNEL);

    if(!rgb_devp)
    {
        printk("allocation failed\n");
        return -ENOMEM;
    }

    sprintf(rgb_devp->name,DEVICE_NAME);
    cdev_init(&rgb_devp->cdev,&rgb_fops);
    rgb_devp->cdev.owner=THIS_MODULE;
    ret=cdev_add(&rgb_devp->cdev,rgb_dev_number,1);

    if(ret)
    {
        printk("cdev error\n");
        return ret;
    }
    rgb_dev_device = device_create(rgb_dev_class, NULL, MKDEV(MAJOR(rgb_dev_number), 0), NULL, DEVICE_NAME);
    memset(rgb_devp->in_string, 0, 256);
    printk("RGB driver is available.\n");
    return 0;
}

/***Module exit function***/
void __exit rgb_driver_exit(void)
{
    hrtimer_cancel(&timer);
   
    unregister_chrdev_region((rgb_dev_number), 1);
      
    device_destroy (rgb_dev_class, MKDEV(MAJOR(rgb_dev_number), 0));
    cdev_del(&rgb_devp->cdev);
    kfree(rgb_devp);
    kfree(istruct);

    class_destroy(rgb_dev_class);

    printk("RGB driver is unavailable.\n");
}

module_init(rgb_driver_init);
module_exit(rgb_driver_exit);
MODULE_LICENSE("GPL v2");
