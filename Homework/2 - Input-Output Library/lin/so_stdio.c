#include "so_stdio.h"
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>

#define BUFSIZE 4096
#define OP_READ 0
#define OP_WRITE 1

struct _so_file
{
	int fd; /* file descriptor */
	unsigned char buffer[BUFSIZE]; /* for buffering */
	int buffer_pos;
	int buffer_len;
	long file_pos; /* current position in file */
	int last_op; /* OP_READ or OP_WRITE */
	int eof; /* 1 if end of file reached, 0 otherwise */
	int error; /* 1 if encountered error on a file operation, 0 otherwise */
	pid_t pid; /* for popen */
};

SO_FILE *so_fopen(const char *pathname, const char *mode)
{
	int flags = 0;
	mode_t m = 0;
	if (mode[0] == 'r')
	{
		if (mode[1] == '+')
		{
			flags = O_RDWR;
		}
		else
		{
			flags = O_RDONLY;
		}
	}
	else if (mode[0] == 'w')
	{
		if (mode[1] == '+')
		{
			flags = O_RDWR | O_TRUNC | O_CREAT;
		}
		else
		{
			flags = O_WRONLY | O_TRUNC | O_CREAT;
		}
		m = 0666;
	}
	else if (mode[0] == 'a')
	{
		if (mode[1] == '+')
		{
			flags = O_RDWR | O_APPEND | O_CREAT;
			m = 0666;
		}
		else
		{
			flags = O_WRONLY | O_APPEND | O_CREAT;
			m = 0666;
		}
	}
	else
	{
		return NULL;
	}
	
	SO_FILE *stream = malloc(sizeof(SO_FILE));
	if (stream == NULL)
	{
		return NULL;
	}
	if (m)
	{
		stream->fd = open(pathname, flags, m);
		if (stream->fd < 0)
		{
			free(stream);
			return NULL;
		}
	}
	else
	{
		stream->fd = open(pathname, flags);
		if (stream->fd < 0)
		{
			free(stream);
			return NULL;
		}
	}
	memset(stream->buffer, 0, BUFSIZE);
	stream->buffer_pos = 0;
	stream->buffer_len = 0;
	stream->file_pos = 0;
	stream->last_op = -1;
	stream->eof = 0;
	stream->error = 0;
	stream->pid = -1;
	return stream;
}

int so_fclose(SO_FILE *stream)
{
	int ret1 = so_fflush(stream);
	int ret2 = close(stream->fd);
	free(stream);
	if (ret1 == SO_EOF || ret2 == -1)
	{
		return SO_EOF;
	}
	return 0;
}

int so_fileno(SO_FILE *stream)
{
	return stream->fd;
}

int so_fgetc(SO_FILE *stream)
{
	stream->last_op = OP_READ;
	if (stream->buffer_pos >= stream->buffer_len)
	{
		/* if buffer is empty */
		int bytes = read(stream->fd, stream->buffer, BUFSIZE);
		if (bytes <= 0)
		{
			if (bytes < 0)
			{
				stream->error = 1;
			}
			else
			{
				stream->eof = 1;
			}
			return SO_EOF;
		}
		stream->buffer_pos = 1;
		stream->buffer_len = bytes;
		stream->file_pos++;
		return stream->buffer[0];
	}
	unsigned char c = stream->buffer[stream->buffer_pos];
	stream->buffer_pos++;
	stream->file_pos++;
	return c;
}
int so_fputc(int c, SO_FILE *stream)
{
	stream->last_op = OP_WRITE;
	if (stream->buffer_len == BUFSIZE)
	{
		/* if buffer is full, write all buffer's content */
		int bytes, bytes_written = 0;
		while (bytes_written < stream->buffer_len)
		{
			bytes = write(stream->fd, stream->buffer + bytes_written, stream->buffer_len - bytes_written);
			if (bytes == -1)
			{
				stream->error = 1;
				return SO_EOF;
			}
			bytes_written += bytes;
		}
		stream->buffer[0] = (unsigned char)c;
		stream->buffer_len = 1;
	}
	else
	{
		stream->buffer[stream->buffer_len++] = (unsigned char)c;
	}
	stream->file_pos++;
	return c;
}

size_t so_fread(void *ptr, size_t size, size_t nmemb, SO_FILE *stream)
{
	unsigned char tmp[size];
	size_t offset;
	int c;
	stream->last_op = OP_READ;
	for (size_t i = 0; i < nmemb; ++i)
	{
		offset = 0;
		for (size_t j = 0; j < size; ++j)
		{
			c = so_fgetc(stream);
			if (c == SO_EOF)
			{
				break;
			}
			tmp[offset++] = (unsigned char)c;
		}
		memcpy(ptr + i * size, tmp, offset);
		if (offset < size)
		{
			return i;
		}
	}
	return nmemb;
}

size_t so_fwrite(const void *ptr, size_t size, size_t nmemb, SO_FILE *stream)
{
	unsigned char tmp[size];
	int c;
	stream->last_op = OP_WRITE;
	for (size_t i = 0; i < nmemb; ++i)
	{
		memcpy(tmp, ptr + i * size, size);
		for (size_t j = 0; j < size; ++j)
		{
			c = so_fputc(tmp[j], stream);
			if (c == SO_EOF)
			{
				return 0;
			}
		}
	}
	return nmemb;
}

int so_fflush(SO_FILE *stream)
{
	if (stream->last_op != OP_WRITE)
	{
		return 0;
	}
	/* write all buffer's content */
	int bytes, bytes_written = 0;
	while (bytes_written < stream->buffer_len)
	{
		bytes = write(stream->fd, stream->buffer + bytes_written, stream->buffer_len - bytes_written);
		if (bytes == -1)
		{
			stream->error = 1;
			return SO_EOF;
		}
		bytes_written += bytes;
	}
	stream->buffer_len = 0;
	return 0;
}

int so_fseek(SO_FILE *stream, long offset, int whence)
{
	if (stream->last_op == OP_READ)
	{
		stream->buffer_pos = 0;
		stream->buffer_len = 0;
	}
	else if (stream->last_op == OP_WRITE)
	{
		int ret = so_fflush(stream);
		if (ret == SO_EOF)
		{
			return -1;
		}
	}
	off_t pos;
	if (whence == SEEK_SET || whence == SEEK_END)
	{
		pos = lseek(stream->fd, offset, whence);
		stream->file_pos = pos;
	}
	else /* if (whence == SEEK_CUR) */
	{
		/* first, move cursor to the actual current position */
		pos = lseek(stream->fd, stream->file_pos, SEEK_SET);
		if (pos == -1)
		{
			stream->error = 1;
			return -1;
		}
		pos = lseek(stream->fd, offset, whence);
		stream->file_pos = pos;
	}
	if (pos == -1)
	{
		stream->error = 1;
		return -1;
	}
	return 0;
}

long so_ftell(SO_FILE *stream)
{
	return stream->file_pos;
}

int so_feof(SO_FILE *stream)
{
	return stream->eof;
}
int so_ferror(SO_FILE *stream)
{
	return stream->error;
}

SO_FILE *so_popen(const char *command, const char *type)
{
	int pipefd[2];
	int ret = pipe(pipefd);
	if (ret == -1)
	{
		return NULL;
	}
	char mode = type[0];
	if (mode != 'r' && mode != 'w')
	{
		return NULL;
	}
	pid_t pid = fork();
	if (pid == -1)
	{
		close(pipefd[0]);
		close(pipefd[1]);
		return NULL;
	}
	else if (pid == 0)
	{
		/* child process */
		if (mode == 'r')
		{
			ret = close(pipefd[0]);
			if (ret == -1)
			{
				exit(1);
			}
			ret = dup2(pipefd[1], STDOUT_FILENO);
			if (ret == -1)
			{
				exit(1);
			}
		}
		else /* if (mode == 'w') */
		{
			ret = close(pipefd[1]);
			if (ret == -1)
			{
				exit(1);
			}
			ret = dup2(pipefd[0], STDIN_FILENO);
			if (ret == -1)
			{
				exit(1);
			}
		}
		ret = execlp("sh", "sh", "-c", command, NULL);
		if (ret == -1)
		{
			exit(1);
		}
	}
	else
	{
		/* parent process */
		SO_FILE *stream = malloc(sizeof(SO_FILE));
		if (stream == NULL)
		{
			waitpid(pid, NULL, 0);
			return NULL;
		}
		memset(stream->buffer, 0, BUFSIZE);
		stream->buffer_pos = 0;
		stream->buffer_len = 0;
		stream->file_pos = 0;
		stream->last_op = -1;
		stream->eof = 0;
		stream->error = 0;
		stream->pid = pid;
		if (mode == 'r')
		{
			ret = close(pipefd[1]);
			stream->fd = pipefd[0];
		}
		else /* if (mode == 'w') */
		{
			ret = close(pipefd[0]);
			stream->fd = pipefd[1];
		}
		if (ret == -1)
		{
			waitpid(pid, NULL, 0);
			free(stream);
			return NULL;
		}
		return stream;
	}
	/* shouldn't get here */
	return NULL;
}

int so_pclose(SO_FILE *stream)
{
	if (stream->pid == -1)
	{
		stream->error = 1;
		return -1;
	}
	int status;
	int ret1 = so_fflush(stream);
	int ret2 = close(stream->fd);
	int ret3 = waitpid(stream->pid, &status, 0);
	free(stream);
	if (ret1 == SO_EOF || ret2 == -1 || ret3 == -1)
	{
		return -1;
	}
	return WEXITSTATUS(status);
}

