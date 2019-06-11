#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/init.h>
#include <linux/kthread.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/net.h>
#include <net/sock.h>
#include <asm/processor.h>
#include <linux/netdevice.h>
#include <linux/ip.h>
#include <linux/in.h>
#include <linux/miscdevice.h>
#include <asm/uaccess.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/debugfs.h>
#include <linux/mm.h>
#include <asm/page.h>
#ifndef VM_RESERVED
#define VM_RESERVED   (VM_DONTEXPAND | VM_DONTDUMP)
#endif

#define slave_IOCTL_CREATESOCK 0x12345677
#define slave_IOCTL_MMAP 0x12345678
#define slave_IOCTL_EXIT 0x12345679
#define slave_PRINT_PAGE_DESCRIPTOR 0x1234567a

#define BUF_SIZE 4096
#define NPAGES 50

struct dentry *file1;//debug file

typedef struct socket *ksocket_t;

//newly added functions and structure for mmap
static int my_mmap(struct file *filp, struct vm_area_struct *vma);
void my_mmap_open(struct vm_area_struct *vma);
void my_mmap_close(struct vm_area_struct *vma);
static struct vm_operations_struct my_mmap_vm_ops = {
	.open = my_mmap_open,
	.close = my_mmap_close
};

//functions about kscoket are exported,and thus we use extern here
extern ksocket_t ksocket(int domain, int type, int protocol);
extern int kconnect(ksocket_t socket, struct sockaddr *address, int address_len);
extern ssize_t krecv(ksocket_t socket, void *buffer, size_t length, int flags);
extern int kclose(ksocket_t socket);
extern unsigned int inet_addr(char* ip);
extern char *inet_ntoa(struct in_addr *in); //DO NOT forget to kfree the return pointer

static int __init slave_init(void);
static void __exit slave_exit(void);

int slave_close(struct inode *inode, struct file *filp);
int slave_open(struct inode *inode, struct file *filp);
static long slave_ioctl(struct file *file, unsigned int ioctl_num, unsigned long ioctl_param);
ssize_t receive_msg(struct file *filp, char *buf, size_t count, loff_t *offp );

static mm_segment_t old_fs;
static ksocket_t sockfd_cli;//socket to the master server
static struct sockaddr_in addr_srv; //address of the master server

#ifdef ASYCHRONOUSIO
//struct for workqueue
#define work_handler_fcntl  (void*)receive_msg
static struct workqueue_struct *wq_fcntl;
DECLARE_WORK(work_fcntl, work_handler_fcntl);
#define work_handler_mmap (void*)slave_ioctl
static struct workqueue_struct *wq_mmap;
DECLARE_WORK(work_mmap, work_handler_mmap);
#endif

//file operations
static struct file_operations slave_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = slave_ioctl,
	.open = slave_open,
	.read = receive_msg,
	.release = slave_close,
	.mmap = my_mmap
};

//device info
static struct miscdevice slave_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "slave_device",
	.fops = &slave_fops
};

static int __init slave_init(void)
{
	int ret;
	file1 = debugfs_create_file("slave_debug", 0644, NULL, NULL, &slave_fops);

	//register the device
	if( (ret = misc_register(&slave_dev)) < 0){
		printk(KERN_ERR "misc_register failed!\n");
		return ret;
	}
	
	printk(KERN_INFO "slave has been registered!\n");

	return 0;
}

static void __exit slave_exit(void)
{
	misc_deregister(&slave_dev);
	#ifdef ASYCHRONOUSIO
	if(wq_fcntl != NULL) destroy_workqueue(wq_fcntl);
	if(wq_mmap != NULL) destroy_workqueue(wq_mmap);
	#endif
	printk(KERN_INFO "slave exited!\n");
	debugfs_remove(file1);
}

int slave_close(struct inode *inode, struct file *filp)
{
	kfree(filp->private_data);
	return 0;
}

int slave_open(struct inode *inode, struct file *filp)
{
	filp->private_data = kmalloc(PAGE_SIZE * NPAGES, GFP_KERNEL);
	return 0;
}

static int my_mmap(struct file *filp, struct vm_area_struct *vma)
{
	vma->vm_private_data = filp->private_data;
	vma->vm_ops = &my_mmap_vm_ops;
	vma->vm_flags |= VM_RESERVED;

	if(remap_pfn_range(vma,vma->vm_start,virt_to_phys(vma->vm_private_data)>>PAGE_SHIFT,vma->vm_end - vma->vm_start, vma->vm_page_prot))
		return -EAGAIN;

	my_mmap_open(vma);
	
	#ifdef ASYCHRONOUSIO
	if((wq_mmap = create_workqueue("master_wq_mmap")) == NULL)
	{
		printk(KERN_ERR "create_workqueue_mmap returned NULL\n");
		return -1;
	}
	
	queue_work(wq_mmap, &work_mmap);
	#endif
	
	return 0;
}

void my_mmap_open(struct vm_area_struct *vma)
{
	return;
}
void my_mmap_close(struct vm_area_struct *vma)
{
	return;
}

static long slave_ioctl(struct file *file, unsigned int ioctl_num, unsigned long ioctl_param)
{
	long ret = -EINVAL;

	int addr_len;
	size_t offset = 0, recv_n;
	char *tmp, ip[20], buf[BUF_SIZE];

    pgd_t *pgd;
	p4d_t *p4d;
	pud_t *pud;
	pmd_t *pmd;
    pte_t *ptep, pte;
	old_fs = get_fs();
	set_fs(KERNEL_DS);

    printk("slave device ioctl");

	switch(ioctl_num){
		case slave_IOCTL_CREATESOCK:// create socket and connect to master
			printk("slave device ioctl create socket");

			if(copy_from_user(ip, (char*)ioctl_param, sizeof(ip)))
				return -ENOMEM;

			sprintf(current->comm, "ksktcli");

			memset(&addr_srv, 0, sizeof(addr_srv));
			addr_srv.sin_family = AF_INET;
			addr_srv.sin_port = htons(2325);
			addr_srv.sin_addr.s_addr = inet_addr(ip);
			addr_len = sizeof(struct sockaddr_in);

			sockfd_cli = ksocket(AF_INET, SOCK_STREAM, 0);
			printk("sockfd_cli = 0x%p  socket is created\n", sockfd_cli);
			if (sockfd_cli == NULL)
			{
				printk("socket failed\n");
				return -1;
			}
			if (kconnect(sockfd_cli, (struct sockaddr*)&addr_srv, addr_len) < 0)
			{
				printk("connect failed\n");
				return -1;
			}
			tmp = inet_ntoa(&addr_srv.sin_addr);
			printk("connected to : %s %d\n", tmp, ntohs(addr_srv.sin_port));
			kfree(tmp);
			printk("kfree(tmp)");
			
			#ifdef ASYCHRONOUSIO
			if((wq_fcntl = create_workqueue("master_wq_fcntl")) == NULL)
			{
				printk(KERN_ERR "create_workqueue_fcntl returned NULL\n");
				return -1;
			}
			
			queue_work(wq_fcntl, &work_fcntl);
			#endif
			
			ret = 0;
			break;
		case slave_IOCTL_MMAP:
			recv_n = krecv(sockfd_cli, file->private_data, sizeof(buf), 0);
			ret = recv_n;
			break;
		case slave_IOCTL_EXIT:
			if(kclose(sockfd_cli) == -1)
			{
				printk("kclose cli error\n");
				return -1;
			}
			ret = 0;
			break;
		case slave_PRINT_PAGE_DESCRIPTOR:
#define check_table(TYPE, table) \
	if ( TYPE##_none(table) || TYPE##_bad(table)) { printk("slave: bad" # TYPE); break;}
			printk("slave: trying to print page descriptor...");
            pgd = pgd_offset(current->mm, ioctl_param);
			check_table(pgd, *pgd);
			p4d = p4d_offset(pgd, ioctl_param);
			check_table(p4d, *p4d);
			pud = pud_offset(p4d, ioctl_param);
			check_table(pud, *pud);
			pmd = pmd_offset(pud, ioctl_param);
			check_table(pmd, *pmd);
			ptep = pte_offset_kernel(pmd , ioctl_param);
			pte = *ptep;
			if (pte_none(pte)){
				printk("slave: bad pte");
				break;
			}
			printk("slave: %lX\n", pte);
			ret = 0;
			break;
#undef check_table
		default:
			printk("Unrecognized ioctl param!");
			break;
	}

	set_fs(old_fs);
	return ret;
}

ssize_t receive_msg(struct file *filp, char *buf, size_t count, loff_t *offp)
{
//call when user is reading from this device
	char msg[BUF_SIZE];
	size_t len;
	len = krecv(sockfd_cli, msg, sizeof(msg), 0);
	if(copy_to_user(buf, msg, len))
		return -ENOMEM;
	return len;
}

module_init(slave_init);
module_exit(slave_exit);
MODULE_LICENSE("GPL");
