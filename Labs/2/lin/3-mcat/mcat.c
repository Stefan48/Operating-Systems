/**
 * SO, 2017
 * Lab #2, Operatii I/O simple
 *
 * Task #3, Linux
 *
 * cat/cp applications
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>

#include "utils.h"
#include "xfile.h"

#define BUFSIZE		32

int main(int argc, char **argv)
{
	int fd_src, fd_dst, rc, bytesRead, fd1;
	char buffer[BUFSIZE];

	if (argc < 2 || argc > 3) {
		printf("Usage:\n\t%s [source_file] [destination_file]\n",
			argv[0]);
		return 0;
	}

	/* TODO 1 - open source file for reading */
	fd_src = open(argv[1], O_RDONLY);
	DIE(fd_src < 0, "open source file");

	if (argc == 3) {
		/* TODO 2 - redirect stdout to destination file */
		fd1 = open(argv[2], O_CREAT | O_RDWR, 0644);
		DIE(fd1 < 0, "open destination file");
		fd_dst = dup2(fd1, STDOUT_FILENO);
		DIE(fd_dst < 0, "dup2");
	}

	/* TODO 1 - read from file and print to stdout
	 * use _only_ read and write functions
     * for writing to output use write(STDOUT_FILENO, buffer, bytesRead);
	 */
	 
	 while (1)
	 {
	 	// while (bytesRead < BUFSIZE) ...
	 	bytesRead = read(fd_src, buffer, BUFSIZE);
	 	if (bytesRead == 0)
	 	{
	 		break;
	 	}
	 	write(STDOUT_FILENO, buffer, bytesRead);
	 }

    /* TODO 3 - Change the I/O strategy and implement xread/xwrite. These
     * functions attempt to read _exactly_ the size provided as parameter.
     */

	/* TODO 1 - close file */
	rc = close(fd_src);
	DIE(rc < 0, "close source file");

	return 0;
}
