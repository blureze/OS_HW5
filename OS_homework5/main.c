#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>		//included for __init and __exit macros
#include <linux/fs.h>	//chrdev
#include <linux/cdev.h>	//cdev_add()/cdev_del()
#include <asm/uaccess.h>	//copy_*_user()
#include <linux/workqueue.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/types.h>

#include "ioc_hw5.h"

#define DMA_BUFSIZE 64
#define DMASTUIDADDR 0x0	//Student ID
#define DMARWOKADDR 0x4		//RW function complete
#define DMAIOCOKADDR 0x8	//ioctl function complete
#define DMAIRQOKADDR 0xc	//ISR function complete

#define DMACOUNTADDR 0x10	//interrupt count function complete (bonus)
#define DMAANSADDR 0x14		//Computation answer
#define DMAREADABLEADDR 0x18	//READABLE variable for synchronize
#define DMAEBLOCKADDR 0x1c	//Blocking or Non-Blocking IO
#define DMAOPCODEADDR 0x20	//data.a opcode(char)
#define DMAOPERANDBADDR 0x21	//data.b operand1(int)
#define DMAOPERANDCADDR 0x25	//data.c operand2(short)

MODULE_LICENSE("Dual BSD/GPL");

void *dma_buf;
static int dev_major;
static int dev_minor;
static struct cdev *dev_cdevp;
static struct work_struct *work;

static void myoutb(unsigned char data, unsigned short int port);
static void myoutw(unsigned short data, unsigned short int port);
static void myoutl(unsigned int data, unsigned short int port);
static unsigned char myinb(unsigned short int port);
static unsigned short myinw(unsigned short int port);
static unsigned int myinl(unsigned short int port);
static int drv_open(struct inode *inode, struct file *filp);
static int drv_release(struct inode *inode, struct file *filp);
static long drv_read(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos);
static long drv_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos);
static long drv_ioctl(struct file *filp, unsigned int cmd, unsigned long arg);


struct file_operations drv_fops = 
{
	owner:	THIS_MODULE,
	read:	drv_read,
	write:	drv_write,
	unlocked_ioctl:	drv_ioctl,
	open:	drv_open,
	release:	drv_release
};

struct dataIn
{
	unsigned char a;
	unsigned int b;
	unsigned short c;
};

static void myoutb(unsigned char data, unsigned short int port)
{
	*(unsigned char*)(dma_buf+port) = data;
}
static void myoutw(unsigned short data, unsigned short int port)
{
	*(unsigned short*)(dma_buf+port) = data;
}
static void myoutl(unsigned int data, unsigned short int port)
{
	*(unsigned int*)(dma_buf+port) = data;
}
static unsigned char myinb(unsigned short int port)
{
	return *(volatile unsigned char*)(dma_buf+port);
}

static unsigned short myinw(unsigned short int port)
{
	return *(volatile unsigned short*)(dma_buf+port);
}

static unsigned int myinl(unsigned short int port)
{
	return *(volatile unsigned int*)(dma_buf+port);
}

static int drv_open(struct inode *inode, struct file *filp)
{
	printk("OS_HW5:%s():device open\n", __FUNCTION__);
	return 0;
}

static int drv_release(struct inode *inode, struct file *filp)
{
	printk("OS_HW5:%s():device close\n",__FUNCTION__);
	return 0;
}

static long drv_read(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
	int ans = myinl(DMAANSADDR);
	
	put_user(ans,(int *)buf);
	myoutl(0,DMAREADABLEADDR);
	myoutl(0,DMAANSADDR);
	
	printk("OS_HW5:%s():ans = %d\n", __FUNCTION__, ans);
	return 0;
}

static int prime(int base, short nth)
{
    int fnd = 0;
    int i, num, isPrime;

	num = base;
	while(fnd != nth)
	{
		isPrime = 1;
		num++;
		for(i = 2; i <= num/2; i++)
		{
			if(num%i == 0)
			{
				isPrime = 0;
				break;
			}
		}

		if(isPrime)
		{
			fnd++;
		}
	}
	return num;
}

static void arithmetic_routine(struct work_struct *data)
{
    int ans;
	unsigned char a;
	unsigned int b;
	unsigned short c;

	a = myinb(DMAOPCODEADDR);
	b = myinl(DMAOPERANDBADDR);
 	c = myinw(DMAOPERANDCADDR);

    switch(a) {
        case '+':
            ans=b+c;
            break;
        case '-':
            ans=b-c;
            break;
        case '*':
            ans=b*c;
            break;
        case '/':
            ans=b/c;
            break;
        case 'p':
            ans = prime(b, c);
            break;
        default:
            ans=0;
    }

	myoutl(ans, DMAANSADDR);
	myoutl(1, DMAREADABLEADDR);
	printk("OS_HW5:%s(): %d %c %d = %d\n", __FUNCTION__, b, a, c, ans);
}

static long drv_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
	struct dataIn *data;
	int blocking_mode;
    INIT_WORK(work, arithmetic_routine);

	printk("OS_HW5:%s():queue work\n",__FUNCTION__);
	
	data = (struct dataIn *) buf;
	myoutb(data->a, DMAOPCODEADDR);
	myoutl(data->b, DMAOPERANDBADDR);
	myoutw(data->c, DMAOPERANDCADDR);
	
	blocking_mode  = myinl(DMAEBLOCKADDR);
	if(blocking_mode == 1)	//block mode
	{
		printk("OS_HW5:%s():block\n",__FUNCTION__);
		schedule_work(work);	
		flush_scheduled_work();
	}
	else	//non-block mode
	{
		schedule_work(work);
	}

	return 0;
}

static long drv_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int value, id;

	switch(cmd)
	{
		case HW5_IOCSETSTUID:
			get_user(id,(int *)arg);
			myoutl(id,DMASTUIDADDR);
			id = myinl(DMASTUIDADDR);
			printk("OS_HW5:%s():My STUID is = %d\n",__FUNCTION__, id);
			break;
		case HW5_IOCSETRWOK:
			get_user(value, (int *)arg);
			myoutl(value,DMARWOKADDR);
			if(myinl(DMARWOKADDR))
				printk("OS_HW5:%s():RW OK\n",__FUNCTION__);
			break;		
		case HW5_IOCSETIOCOK:
			get_user(value, (int *)arg);
			myoutl(value,DMAIOCOKADDR);
			if(myinl(DMAIOCOKADDR))
				printk("OS_HW5:%s():IOC OK\n",__FUNCTION__);
			break;
		case HW5_IOCSETIRQOK:
			get_user(value, (int *)arg);
			myoutl(value,DMAIRQOKADDR);
			if(myinl(DMAIRQOKADDR))
				printk("OS_HW5:%s():IRQ OK\n",__FUNCTION__);
			break;	
		case HW5_IOCSETBLOCK:
			get_user(value, (int *)arg);
			myoutl(value,DMAEBLOCKADDR);
			if(value == 1)
			{
				printk("OS_HW5:%s():Blocking IO\n",__FUNCTION__);
			}
			else
			{
				printk("OS_HW5:%s():Non-Blocking IO\n",__FUNCTION__);
			}			
			break;		
		case HW5_IOCWAITREADABLE:
			while(myinl(DMAREADABLEADDR) != 1)
			{
				msleep(1000);
			}
			
			put_user(1, (int *)arg);
			printk("OS_HW5:%s():wait readable 1\n",__FUNCTION__);

			break;		
		default:
			return -ENOTTY;
	}
	
	return 0;
}

static int __init init_modules(void)
{
	dev_t dev;
	int ret = 0;
	work = kzalloc(sizeof(typeof(*work)), GFP_KERNEL);

	printk("OS_HW5:%s():..............Start..............\n",__FUNCTION__);
	ret = alloc_chrdev_region(&dev, 0, 1, "mydev");
	if(ret)
	{
		printk("Cannot alloc chrdev\n");
		return ret;
	}
	
	dev_major = MAJOR(dev);
	dev_minor = MINOR(dev);
	printk("OS_HW5:%s():register chrdev(%d,%d)\n",__FUNCTION__,dev_major,dev_minor);
	
	dev_cdevp = cdev_alloc();

	cdev_init(dev_cdevp, &drv_fops);
	dev_cdevp->owner = THIS_MODULE;
	ret = cdev_add(dev_cdevp, MKDEV(dev_major, dev_minor), 1);
	if(ret < 0)
	{
		printk("Add chrdev failed\n");
		return ret;
	}
	
	dma_buf = kzalloc(DMA_BUFSIZE, GFP_KERNEL);
	printk("OS_HW5:%s():allocate dma buffer\n",__FUNCTION__);
	return 0;	//Non-zero return means that the module couldn't be loaded.
}

static void __exit exit_modules(void)
{
	dev_t dev;
	
	kfree(work);
	kfree(dma_buf);
	printk("OS_HW5:%s():free dma buffer\n",__FUNCTION__);

	dev = MKDEV(dev_major, dev_minor);
	unregister_chrdev_region(dev, 1);
	printk("OS_HW5:%s():unregister chrdev\n",__FUNCTION__);
	cdev_del(dev_cdevp);
	printk("OS_HW5:%s():..............End..............\n",__FUNCTION__);
}

module_init(init_modules);
module_exit(exit_modules);
