/*
 * Loader Implementation
 *
 * 2018, Operating Systems
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>

#include "exec_parser.h"

static so_exec_t *exec;

typedef struct so_seg_info
{
	/* number of pages used to store the segment */
	int pages_no;
	/* boolean array storing which pages have already been mapped */
	uint8_t *is_mapped;
} seg_info_t;

/* default signal handler */
static struct sigaction old_sa;
/* page size */
static int page_size;
/* executable file descriptor */
static int fd;

static void segv_handler(int signum, siginfo_t *info, void *context)
{
	if (signum != SIGSEGV)
	{
		/* run default signal handler */
		old_sa.sa_sigaction(signum, info, context);
		return;
	}
	/* address which caused the page fault */
	uintptr_t addr = (uintptr_t)info->si_addr;
	int segments_no = exec->segments_no;
	int page;
	so_seg_t *seg;
	seg_info_t *data;
	char *p;
	uint8_t fault_in_segment = 0;
	for (int i = 0; i < segments_no; ++i)
	{
		seg = &exec->segments[i];
		if (addr >= seg->vaddr && addr < (seg->vaddr + seg->mem_size))
		{
			/* page fault inside segment */
			fault_in_segment = 1;
			page = (addr - seg->vaddr) / page_size;
			data = (seg_info_t*)seg->data;
			if (data->is_mapped[page])
			{
				/* page already mapped -> forbidden memory access
				 * run default signal handler */
				old_sa.sa_sigaction(signum, info, context);
			}
			else
			{
				/* page not mapped
				 *  - map page
				 *  - set protection to read and write initially */
				p = mmap((void*)seg->vaddr + page * page_size, page_size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
				if (p == MAP_FAILED)
				{	
					return;
				}
				/* copy memory from executable's segment's corresponding page to mapped page
				 * offset in file: seg->offset + page * page_size */
				if (lseek(fd, seg->offset + page * page_size, SEEK_SET) == -1)
				{
					return;
				}
				int bytes, bytes_read = 0, bytes_remaining = page_size;
				while (bytes_remaining > 0)
				{
					bytes = read(fd, (void*)seg->vaddr + page * page_size + bytes_read, bytes_remaining);
					if (bytes == -1)
					{
						return;
					}
					if (bytes == 0)
					{
						break;
					}
					bytes_read += bytes;
					bytes_remaining -= bytes;
				}
				if (bytes_read < page_size)
				{
					/* zero mapped memory that hasn't been read from the executable file */
					memset((void*)seg->vaddr + page * page_size + bytes_read, 0, bytes_remaining);
				}
				/* update mapped memory protection according to segment's specification */
				int prot = 0;
				if (seg->perm & PERM_R)
				{
					prot |= PROT_READ;
				}
				if (seg->perm & PERM_W)
				{
					prot |= PROT_WRITE;
				}
				if (seg->perm & PERM_X)
				{
					prot |= PROT_EXEC;
				}
				if (mprotect((void*)seg->vaddr + page * page_size, page_size, prot) == -1)
				{
					return;
				}
				data->is_mapped[page] = 1;
				/* if page_end >= file_size */
				if (seg->vaddr + (page + 1) * page_size > seg->vaddr + seg->file_size)
				{
					/* zero memory between file_size_end and mem_size_end */
					uintptr_t start, end;
					/* start = max(seg->vaddr + seg->file_size, seg->vaddr + page * page_size) */
					if (seg->vaddr + seg->file_size >= seg->vaddr + page * page_size)
					{
						start = seg->vaddr + seg->file_size;
					}
					else
					{
						start = seg->vaddr + page * page_size;
					}
					/* end = min(seg->vaddr + seg->mem_size, seg->vaddr + (page + 1) * page_size) */
					if (seg->vaddr + seg->mem_size <= seg->vaddr + (page + 1) * page_size)
					{
						end = seg->vaddr + seg->mem_size;
					}
					else
					{
						end = seg->vaddr + (page + 1) * page_size;
					}
					memset((void*)start, 0, end - start);
				}
			}
			break;
		}
		else if (addr >= seg->vaddr && addr < (seg->vaddr + ((seg_info_t*)seg->data)->pages_no * page_size))
		{
			/* inside segment but outside mem_size -> unspecified behavior */
			fault_in_segment = 1;
			break;
		}
	}
	if (fault_in_segment == 0)
	{
		/* page fault outside any segment
		 * run default signal handler */
		old_sa.sa_sigaction(signum, info, context);
	}
}

int so_init_loader(void)
{
	/* initialize on-demand loader */
	page_size = getpagesize();
	struct sigaction sa;
	memset(&sa, 0, sizeof(struct sigaction));
	sa.sa_flags = SA_SIGINFO;
	sa.sa_sigaction = segv_handler;
	sigemptyset(&sa.sa_mask);
	sigaddset(&sa.sa_mask, SIGSEGV);
	if (sigaction(SIGSEGV, &sa, &old_sa) == -1)
	{
		return -1;
	}
	return 0;
}

int so_execute(char *path, char *argv[])
{
	/* parse executable file */
	exec = so_parse_exec(path);
	if (!exec)
		return -1;
	/* open executable file for reading when mapping memory */
	fd = open(path, O_RDONLY);
	if (fd == -1)
	{
		return -1;
	}
	/* initialize segments data */
	int segments_no = exec->segments_no;
	int pages_no;
	seg_info_t *data;
	for (int i = 0; i < segments_no; ++i)
	{
		if (exec->segments[i].mem_size % page_size)
		{
			pages_no = exec->segments[i].mem_size / page_size + 1;
		}
		else
		{
			pages_no = exec->segments[i].mem_size / page_size;
		}
		exec->segments[i].data = malloc(sizeof(seg_info_t));
		data = (seg_info_t*)exec->segments[i].data;
		data->pages_no = pages_no;
		data->is_mapped = malloc(pages_no * sizeof(uint8_t));
		for (int j = 0; j < pages_no; ++j)
		{
			data->is_mapped[j] = 0;
		}
	}
	/* start executable file */
	so_start_exec(exec, argv);
	return 0;
}
