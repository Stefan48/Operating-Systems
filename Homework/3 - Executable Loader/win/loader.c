/*
 * Loader Implementation
 *
 * 2018, Operating Systems
 */

#include <stdio.h>
#include <Windows.h>

#define DLL_EXPORTS
#include "loader.h"
#include "exec_parser.h"

static so_exec_t *exec;

typedef struct so_seg_info
{
    /* number of pages used to store the segment */
    int pages_no;
    /* boolean array storing which pages have already been mapped */
    uint8_t *is_mapped;
} seg_info_t;

/* page size */
static DWORD page_size;
/* how many pages to reserve at once in order to keep the alignment */
static DWORD page_reserve_step;
/* executable file handle */
static HANDLE handle;
/* handle for the page fault exception handler */
static LPVOID page_fault_handler;

/* page fault exception handler */
static LONG CALLBACK page_fault(PEXCEPTION_POINTERS ExceptionInfo)
{
    DWORD ExceptionCode;
    DWORD pos;
    DWORD protect, old_protect;
    BOOL bRet;
    uintptr_t addr;
    int segments_no, page, start_page, i;
	unsigned int j;
    DWORD bytes, bytes_read, bytes_remaining;
    so_seg_t *seg;
    seg_info_t *data;
    LPBYTE p;
    uint8_t fault_in_segment;
    uintptr_t start, end;
    
    ExceptionCode = ExceptionInfo->ExceptionRecord->ExceptionCode;
    if (ExceptionCode != EXCEPTION_ACCESS_VIOLATION && ExceptionCode != EXCEPTION_DATATYPE_MISALIGNMENT)
    {
        /* pass exception to next exception handler */
        return EXCEPTION_CONTINUE_SEARCH;
    }
    /* get the memory location which caused the page fault */
    addr = ExceptionInfo->ExceptionRecord->ExceptionInformation[1];
    segments_no = exec->segments_no;
    fault_in_segment = 0;
    for (i = 0; i < segments_no; ++i)
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
                 * pass exception to next exception handler */
                return EXCEPTION_CONTINUE_SEARCH;
            }
            else
            {
                /* page not mapped
                 *  - map a set of pages
                 *  - set protection to read and write initially */
                if (page % page_reserve_step == 0)
                {
                    start_page = page;
                }
                else
                {
                    start_page = page - page % page_reserve_step;
                }
                p = VirtualAlloc((LPBYTE)seg->vaddr + start_page * page_size, page_reserve_step * page_size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
                if (p == NULL)
                {
                    return EXCEPTION_CONTINUE_SEARCH;
                }
                /* copy memory from executable's segment's corresponding pages to mapped pages
                 * offset in file: seg->offset + start_page * page_size */
                pos = SetFilePointer(handle, /* file handle */
                                     seg->offset + start_page * page_size, /* offset in file */
                                     NULL, /* no 64 bytes offset */
                                     FILE_BEGIN
                );
                if (pos == INVALID_SET_FILE_POINTER)
                {
                    return EXCEPTION_CONTINUE_SEARCH;
                }
                bytes_read = 0;
                bytes_remaining = 16 * page_size;
                while (bytes_remaining > 0)
                {
                    bRet = ReadFile(handle, /* file handle */
                                    (LPBYTE)seg->vaddr + start_page * page_size + bytes_read, /* where to put data */
                                    bytes_remaining, /* number of bytes to read */
                                    &bytes, /* number of bytes that were read */
                                    NULL /* no overlapped structure */
                    );
                    if (bRet == FALSE)
                    {
                        return EXCEPTION_CONTINUE_SEARCH;
                    }
                    if (bytes == 0)
                    {
                        break;
                    }
                    bytes_read += bytes;
                    bytes_remaining -= bytes;
                }    
                if (bytes_read < page_reserve_step * page_size)
                {
                    /* zero mapped memory that hasn't been read from the executable file */
                    memset((unsigned char*)seg->vaddr + start_page * page_size + bytes_read, 0, bytes_remaining);
                }
                /* update mapped memory protection according to segment's specification */
                protect = 0;
                if (seg->perm & PERM_R)
                {
                    if (seg->perm & PERM_W)
                    {
                        if (seg->perm & PERM_X)
                        {
                            protect = PAGE_EXECUTE_READWRITE;
                        }
                        else
                        {
                            protect = PAGE_READWRITE;
                        }
                    }
                    else if (seg->perm & PERM_X)
                    {
                        protect = PAGE_EXECUTE_READ;
                    }
                    else
                    {
                        protect = PAGE_READONLY;
                    }
                }
                else if (seg->perm & PERM_W)
                {
                    if (seg->perm & PERM_X)
                    {
                        protect = PAGE_EXECUTE_READWRITE;
                    }
                    else
                    {
                        protect = PAGE_READWRITE;
                    }
                }
                else if (seg->perm & PERM_X)
                {
                    protect = PAGE_EXECUTE;
                }
                else
                {
                    protect = PAGE_NOACCESS;
                }
                bRet = VirtualProtect((LPBYTE)seg->vaddr + start_page * page_size, page_reserve_step * page_size, protect, &old_protect);
                if (bRet == FALSE)
                {
                    return EXCEPTION_CONTINUE_SEARCH;
                }
                /* mark mapped pages */
                for (j = start_page; j < start_page + page_reserve_step; ++j)
                {
                    data->is_mapped[page] = 1;
                }
                /* if page_set_end > file_size and mem_size > file_size */
                if (seg->vaddr + (start_page + page_reserve_step) * page_size > seg->vaddr + seg->file_size && seg->mem_size > seg->file_size)
                {
                    /* zero memory between file_size_end and mem_size_end */
                    /* start = max(seg->vaddr + seg->file_size, seg->vaddr + start_page * page_size) */
                    if (seg->vaddr + seg->file_size >= seg->vaddr + start_page * page_size)
                    {
                        start = seg->vaddr + seg->file_size;
                    }
                    else
                    {
                        start = seg->vaddr + start_page * page_size;
                    }
                    /* end = min(seg->vaddr + seg->mem_size, seg->vaddr + (start_page + page_reserve_step) * page_size) */
                    if (seg->vaddr + seg->mem_size <= seg->vaddr + (start_page + page_reserve_step) * page_size)
                    {
                        end = seg->vaddr + seg->mem_size;
                    }
                    else
                    {
                        end = seg->vaddr + (start_page + page_reserve_step) * page_size;
                    }
                    memset((unsigned char*)start, 0, end - start);
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
         * pass exception to next exception handler */
        return EXCEPTION_CONTINUE_SEARCH;
    }
    return EXCEPTION_CONTINUE_EXECUTION;
}

int so_init_loader(void)
{
    /* initialize on-demand loader */
    SYSTEM_INFO siSysInfo;
    GetSystemInfo(&siSysInfo);
    page_size = siSysInfo.dwPageSize;
    page_reserve_step = 16;
    page_fault_handler = AddVectoredExceptionHandler(1, page_fault);
    if (page_fault_handler == NULL)
    {
        return -1;
    }
    return 0;
}

int so_execute(char *path, char *argv[])
{
    int segments_no, pages_no, i, j;
    seg_info_t *data;

    /* parse executable file */
    exec = so_parse_exec(path);
    if (!exec)
        return -1;
    /* open executable file for reading when mapping memory */
    handle = CreateFile(path,
                        GENERIC_READ, /* access mode */
                        FILE_SHARE_READ, /* sharing option */
                        NULL, /* security attributes */
                        OPEN_EXISTING, /* open only if it exists */
                        FILE_ATTRIBUTE_NORMAL, /* file attributes */
                        NULL
    );
    if (handle == INVALID_HANDLE_VALUE)
    {
        return -1;
    }
    /* initialize segments data */
    segments_no = exec->segments_no;
    for (i = 0; i < segments_no; ++i)
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
        for (j = 0; j < pages_no; ++j)
        {
            data->is_mapped[j] = 0;
        }
    }
    /* start executable file */
    so_start_exec(exec, argv);
    return 0;
}