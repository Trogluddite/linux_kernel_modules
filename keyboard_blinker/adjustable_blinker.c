#include <linux/module.h>
#include <linux/init.h>
#include <linux/tty.h>		        /* For fg_console, MAX_NR_CONSOLES */
#include <linux/kd.h>		          /* For KDSETLED */
#include <linux/vt_kern.h>
#include <linux/console_struct.h>	/* For vc_cons */
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/string.h>

/*
* proc_fs stuff
*/
#define PROC_FILE_NAME "blinker"
#define PROCFS_MAX_SIZE		1024
static char procfs_buffer[PROCFS_MAX_SIZE];
static unsigned long procfs_buffer_size = 0;
char outbuff[PROCFS_MAX_SIZE] = "Lights to blink: 7, HZ divisor: 5\n";
char inbuff[PROCFS_MAX_SIZE] = "L0";

static short blink_leds = 0x07;
static short blink_divisor = 5;
/*
* blinkenstuff
*/
struct timer_list blinken_timer;
struct tty_driver *blinken_driver;
char kbledstatus = 0;
#define RESTORE_LEDS  0xFF

ssize_t blink_read(struct file *filp, char *buf, size_t count, loff_t *offp ) 
{
	char* readspot = outbuff + *offp;
	size_t readlen = strlen(readspot) > count ? count : strlen(readspot);
	printk("proc_read just ran!  count = %lu, sizeof(loff_t) = %lu, *offp = %llu\n", count, sizeof(loff_t), *offp);	
	if(*offp >= strlen(outbuff))
			return 0;
	strncpy(buf, readspot, count);
	*offp += readlen;
	return readlen;
}

ssize_t blink_write(struct file *file, const char *buffer,
                 size_t count, loff_t *ppos)
{
  procfs_buffer_size = count;
  if (procfs_buffer_size > PROCFS_MAX_SIZE) {
    procfs_buffer_size = PROCFS_MAX_SIZE;
  }

  /* Validaate input */
  if (copy_from_user(procfs_buffer, buffer, procfs_buffer_size) ){
    printk("copy_from_user failed with input buffer of size %lu", procfs_buffer_size);
    return -EFAULT;
  }
  strncpy(inbuff, buffer, count);
  if(count > 3){
    printk("Invalid control sequence, expected input of no more than 2 characters but got %s", inbuff);
    return -EINVAL;
  }
  else if( inbuff[0] != 'L' && inbuff[0] != 'D'){
    printk("Invalid control sequence, expected first character of input to be L or D but got %s", inbuff);
    return -EINVAL;
  }
  else if( (inbuff[0] == 'L') && (((char)inbuff[1] - '0') > 7 || ((char)inbuff[1] - '0') < 0) ) {
    printk(
      "Invalid control sequence, LED Pattern must be a number in the range 0-7 but input was %s",
      inbuff
    );
    return -EINVAL;
  }
  else if( inbuff[0] == 'D' && (((char)inbuff[0] - '0') > 9 || ((char)inbuff[1] - '0') < 0) ) {
    printk(
      "Invalid control sequence, HZ Divisor must be in the range 0-9 but input was %s",
      inbuff
    );
  }
  /* end input validation */

  /* reset blinkenvals */
  if(inbuff[0] == 'L'){
    blink_leds = inbuff[1] - 48;
  }
  else{
    blink_divisor = inbuff[1] - 48;
  }

  char num_buff[2] = "0"; //for sprintf
  char new_status[PROCFS_MAX_SIZE] = "Lights to blink: ";
  sprintf(num_buff, "%d", blink_leds);
  strcat(new_status, num_buff);
  strcat(new_status, " HZ divisor: ");
  sprintf(num_buff, "%d", blink_divisor);
  strcat(new_status, num_buff);
  strcat(new_status, " \n");
  strncpy(outbuff, new_status, strlen(new_status));
  /* end reset blinkenvals */
  
  return procfs_buffer_size;
}

struct proc_ops proc_fops = {
	proc_read: blink_read,
	proc_write: blink_write
};

/*
 * Function timer_func blinks the keyboard LEDs periodically by invoking
 * command KDSETLED of ioctl() on the keyboard driver. For more info on 
 * terminal ioctl operations, see file:
 *     /usr/src/linux/drivers/char/vt_ioctl.c, function vt_ioctl().
 *
 * The argument to KDSETLED is alternatively set to 7 (thus causing the led 
 * mode to be set to LED_SHOW_IOCTL, and all the leds are lit) and to 0xFF
 * (any value above 7 switches back the led mode to LED_SHOW_FLAGS, thus
 * the LEDs reflect the actual keyboard status).  For more info, see, 
 * please see file:
 *     /usr/src/linux/drivers/char/keyboard.c, function setledstate().
 * 
 */

static void timer_func(struct timer_list *timers)
{
	int *pstatus = (int *)&kbledstatus;

	if (*pstatus == blink_leds)
		*pstatus = RESTORE_LEDS;
	else
		*pstatus = blink_leds;

	(blinken_driver->ops->ioctl) (vc_cons[fg_console].d->port.tty, KDSETLED,
			    *pstatus);

	blinken_timer.expires = jiffies + (HZ / blink_divisor);
	add_timer(&blinken_timer);
}

int kbleds_init(void){
	int i;

	printk(KERN_INFO "HZ = %d, jiffies = %lu\n", HZ, jiffies);
	printk(KERN_INFO "kbleds: loading\n");
	printk(KERN_INFO "kbleds: fgconsole is %x\n", fg_console);
	for (i = 0; i < MAX_NR_CONSOLES; i++) {
		if (!vc_cons[i].d)
			break;
		printk(KERN_INFO "poet_atkm: console[%i/%i] #%i, tty %lx\n", i,
		       MAX_NR_CONSOLES, vc_cons[i].d->vc_num,
		       (unsigned long)vc_cons[i].d->port.tty);
	}
	printk(KERN_INFO "kbleds: finished scanning consoles\n"); 

	blinken_driver = vc_cons[fg_console].d->port.tty->driver;
  // tty_driver->magic no present in kernel 6.7.6
	printk(KERN_INFO "kbleds: tty driver type %x\n", blinken_driver->type);

	/*
	 * Set up the LED blink timer the first time
	 */
	
	timer_setup(&blinken_timer, timer_func, 0);
	blinken_timer.expires = jiffies + (HZ / blink_divisor);
	add_timer(&blinken_timer);

	blinken_driver->ops->ioctl (vc_cons[fg_console].d->port.tty, KDSETLED, blink_leds);

	return 0;
}

int blink_init(void) {
	printk("initializing proc_fs interface and blinkenlights\n");
	proc_create(PROC_FILE_NAME,0,NULL,&proc_fops);
  kbleds_init();
	return 0;
}

void blink_cleanup(void) {
	remove_proc_entry(PROC_FILE_NAME,NULL);
  printk(KERN_INFO "kbleds: unloading...\n");
	del_timer(&blinken_timer);
	(blinken_driver->ops->ioctl) (vc_cons[fg_console].d->port.tty, KDSETLED, RESTORE_LEDS);
  return;
}

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("CS-430 Project, keyboard blinkenlights ocfs read/write.");
MODULE_AUTHOR("Joe Burchettt, following exaamples from Seth Long & Daniele Paolo Scarpazza");
module_init(blink_init);
module_exit(blink_cleanup);
