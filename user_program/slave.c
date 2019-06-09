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
#define BUF_SIZE PAGE_SIZE
#define EXIT_FAILURE 1
#define HANDLE_FATAL(r, x) if ((intptr_t)r < 0) do { fprintf(stderr, "[-] SYSTEM ERROR : %s\n", #x); \
		fprintf(stderr, "\tLocation : %s(), %s:%u\n", __FUNCTION__, __FILE__, __LINE__); \
		perror("      OS message "); fprintf(stderr, "\n"); return EXIT_FAILURE; } while (0)

int main (int argc, char* argv[])
{
	char buf[BUF_SIZE];
	int i, ret, dev_fd, file_fd;// the fd for the device and the fd for the input file
	size_t nread, file_size = 0, data_size = -1, offset = 0, length;
	char file_name[50];
	char method[20];
	char ip[20];
	struct timeval start, end;
	double trans_time; //calulate the time between the device is opened and it is closed
	char *kernel_address, *file_address;

	strcpy(file_name, argv[1]);
	strcpy(method, argv[2]);
	strcpy(ip, argv[3]);

	dev_fd = open("/dev/slave_device", O_RDWR); //should be O_RDWR for PROT_WRITE when mmap()
	HANDLE_FATAL(dev_fd, "failed to open /dev/slave_device");
	
	gettimeofday(&start, NULL);
	
	file_fd = open(file_name, O_RDWR | O_CREAT | O_TRUNC);
	HANDLE_FATAL(file_fd, "failed to open input file");

	ret = ioctl(dev_fd, 0x12345677, ip); //0x12345677 : connect to master in the device
	HANDLE_FATAL(ret, "ioclt create slave socket error");

    write (1, "ioctl success\n", 14);

	switch(method[0]) {
		case 'f'://fcntl : read()/write()
			do
			{
				nread = read(dev_fd, buf, sizeof(buf)); // read from the the device
				write(file_fd, buf, nread); //write to the input file
				file_size += nread;
			} while(nread > 0);
			break;
		case 'm'://mmap : mmap()/memcpy()
			while((ret = ioctl(dev_fd, 0x12345678)) > 0)
			{
				while(ret == 0 && file_size == 0)
					ret = ioctl(dev_fd, 0x12345678);

				length = ret;
				char *file_addr, *dev_addr;
				ftruncate(file_fd, offset+length);
				file_addr = mmap(NULL, length, PROT_WRITE, MAP_SHARED, file_fd, offset);
				HANDLE_FATAL(file_addr, "Can't mmap to file!");
				dev_addr = mmap(NULL, length, PROT_READ, MAP_SHARED, dev_fd, 0);
				HANDLE_FATAL(dev_addr, "Can't mmap to slave device!");
				memcpy(file_addr, dev_addr, length);

				munmap(file_addr, length);
				munmap(dev_addr, length);
				offset += length;
				file_size += length;
			}
			break;
	}

	ret = ioctl(dev_fd, 0x12345679); // end receiving data, close the connection
	HANDLE_FATAL(ret, "ioclt client exits error");
	
	gettimeofday(&end, NULL);
	trans_time = (end.tv_sec - start.tv_sec)*1000 + (end.tv_usec - start.tv_usec)*0.0001;
	printf("Transmission time: %lf ms, File size: %lu bytes\n", trans_time, file_size);

	close(file_fd);
	close(dev_fd);
	
	return 0;
}
