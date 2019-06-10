#include <stdio.h>
#include <stdlib.h>
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
#define BUF_SIZE PAGE_SIZE
#define EXIT_FAILURE 1
#define HANDLE_FATAL(r, x) if ((intptr_t)r < 0) do { fprintf(stderr, "[-] SYSTEM ERROR : %s\n", #x); \
		fprintf(stderr, "\tLocation : %s(), %s:%u\n", __FUNCTION__, __FILE__, __LINE__); \
		perror("      OS message "); fprintf(stderr, "\n"); return EXIT_FAILURE; } while (0)

size_t get_filesize(const char* filename);//get the size of the input file

int main (int argc, char* argv[])
{
	char buf[BUF_SIZE];
	int i, ret, dev_fd, file_fd;// the fd for the device and the fd for the input file
	size_t nread, file_size, offset = 0, length = NPAGE * PAGE_SIZE;
	char file_name[50], method[20];
	struct timeval start, end;
	double trans_time; //calulate the time between the device is opened and it is closed

	strcpy(file_name, argv[1]);
	strcpy(method, argv[2]);

	dev_fd = open("/dev/master_device", O_RDWR);
	HANDLE_FATAL(dev_fd, "failed to open /dev/master_device");
	
	gettimeofday(&start, NULL); /* Get current time precise to nsec */
	
	file_fd = open(file_name, O_RDWR);
	HANDLE_FATAL(file_fd, "failed to open input file");

	file_size = get_filesize(file_name);
	HANDLE_FATAL(file_size, "failed to get filesize");


	ret = ioctl(dev_fd, 0x12345677); //0x12345677 : create socket and accept the connection from the slave
	HANDLE_FATAL(ret, "ioclt server create socket error"); 
	// Prepare for mmapping
	char *file_addr = NULL;
	char *dev_addr = NULL;

	switch (method[0]) {
		case 'f': //fcntl : read()/write()
			do
			{
				nread = read(file_fd, buf, sizeof(buf)); // read from the input file
				write(dev_fd, buf, nread);//write to the the device
			} while(nread > 0);
			break;
		case 'm': //mmap : memcpy() and mmap()
			dev_addr = mmap(NULL, NPAGE * BUF_SIZE, PROT_WRITE, MAP_SHARED, dev_fd, 0);
			HANDLE_FATAL(dev_addr, "Can't mmap to master device!");
			*dev_addr=0;
			while(offset < file_size) {
				if(offset + length > file_size)
					length = file_size - offset;
				file_addr = mmap(NULL, length, PROT_READ, MAP_SHARED, file_fd, offset);
				HANDLE_FATAL(file_addr, "Can't mmap to file!");

				memcpy(dev_addr,file_addr,length);
				ret = ioctl(dev_fd, 0x12345678, length);
				HANDLE_FATAL(ret, "ioctl server sending error");

				munmap(file_addr, length);
				offset += length;
			}
			break;
	}

	ret = ioctl(dev_fd, 0x12345679); // end sending data, close the connection
	HANDLE_FATAL(ret, "ioclt server exits error");
	
	gettimeofday(&end, NULL);
	
	trans_time = (end.tv_sec - start.tv_sec)*1000 + (end.tv_usec - start.tv_usec)*0.0001;
	printf("Transmission time: %lf ms, File size: %lu bytes\n", trans_time, file_size);
	if (dev_addr){ // There's a memory region mapping to the device
		ioctl(dev_fd, 0x1234567a, dev_addr);
		munmap(dev_addr, NPAGE * PAGE_SIZE);
	}

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
