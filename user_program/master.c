#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>

#define PAGE_SIZE 4096
#define NPAGE 50
#define BUF_SIZE 512
size_t get_filesize(const char* filename);//get the size of the input file


int main (int argc, char* argv[])
{
	char buf[BUF_SIZE];
	int i, dev_fd, file_fd;// the fd for the device and the fd for the input file
	size_t ret, file_size, offset = 0, length = NPAGE * PAGE_SIZE;
	char file_name[50], method[20];
	char *kernel_address = NULL, *file_address = NULL;
	struct timeval start;
	struct timeval end;
	double trans_time; //calulate the time between the device is opened and it is closed


	strcpy(file_name, argv[1]);
	strcpy(method, argv[2]);


	if( (dev_fd = open("/dev/master_device", O_RDWR)) < 0)
	{
		perror("failed to open /dev/master_device\n");
		return 1;
	}
	gettimeofday(&start ,NULL);
	if( (file_fd = open (file_name, O_RDWR)) < 0 )
	{
		perror("failed to open input file\n");
		return 1;
	}

	if( (file_size = get_filesize(file_name)) < 0)
	{
		perror("failed to get filesize\n");
		return 1;
	}


	if(ioctl(dev_fd, 0x12345677) == -1) //0x12345677 : create socket and accept the connection from the slave
	{
		perror("ioclt server create socket error\n");
		return 1;
	}


	switch(method[0])
	{
		case 'f': //fcntl : read()/write()
			do
			{
				ret = read(file_fd, buf, sizeof(buf)); // read from the input file
				write(dev_fd, buf, ret);//write to the the device
			}while(ret > 0);
			break;
		case 'm': //mmap : memcpy() and mmap()
			while(offset < file_size)
			{
				char *file_addr, *dev_addr;
				if(offset + length > file_size)
					length = file_size - offset;
				file_addr = mmap(NULL, length, PROT_READ, MAP_PRIVATE, file_fd, offset);
				dev_addr = mmap(NULL, length, PROT_WRITE, MAP_PRIVATE, dev_fd, 0);

				memcpy(dev_addr,file_addr,length);
				if(ioctl(dev_fd, 0x12345678, length) == -1)
				{
					perror("ioctl server sending error\n");
					return 1;
				}

				munmap(file_addr, length);
				munmap(dev_addr, length);
				offset += length;
			}
			break;
	}

	if(ioctl(dev_fd, 0x12345679) == -1) // end sending data, close the connection
	{
		perror("ioclt server exits error\n");
		return 1;
	}
	gettimeofday(&end, NULL);
	trans_time = (end.tv_sec - start.tv_sec)*1000 + (end.tv_usec - start.tv_usec)*0.0001;
	printf("Transmission time: %lf ms, File size: %d bytes\n", trans_time, file_size / 8);

	close(file_fd);
	close(dev_fd);

	return 0;
}

size_t get_filesize(const char* filename)
{
    struct stat st;
    stat(filename, &st);
    return st.st_size;
}
