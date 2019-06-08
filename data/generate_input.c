#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[])
{
	if (argc != 2) {
		puts("Usage: ./generate_input [NUMBER_OF_BYTES_TO_GENERATE]");
		exit(EXIT_FAILURE);
	}
	
	long num_bytes = atol(argv[1]);
	char filename[32] = "Input_";
	strcat(filename, argv[1]);
	strcat(filename, "_bytes");
	
	
	FILE *fp = fopen(filename, "w");
	
	for (long i = 0; i < num_bytes; i++) {
		fputc('a', fp);
	}
	
	fclose(fp);
	
	printf("Finished: Generate to \"%s\"\n", filename);

	return 0;
}
