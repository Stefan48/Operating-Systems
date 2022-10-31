#include "so_stdio.h"

#define BUFSIZE 4096
#define OP_READ 0
#define OP_WRITE 1

#define NO_APPEND 0
#define APPEND_UNSET 1
#define APPEND_SET 2

struct _so_file
{
    HANDLE handle; /* file handle */
    int append; /* for files opened in append mode */
    unsigned char buffer[BUFSIZE]; /* for buffering */
    unsigned int buffer_pos;
    unsigned int buffer_len;
    long file_pos; /* current position in file */
    int last_op; /* OP_READ or OP_WRITE */
    int eof; /* 1 if end of file reached, 0 otherwise */
    int error; /* 1 if encountered error on a file operation, 0 otherwise */
    HANDLE process; /* for popen */
};

SO_FILE *so_fopen(const char *pathname, const char *mode)
{
    DWORD dwDesiredAccess = 0;
    DWORD dwCreationDisposition;
    SO_FILE *stream;
    if (mode[0] == 'r')
    {
        dwDesiredAccess = GENERIC_READ;
        if (mode[1] == '+')
        {
            dwDesiredAccess |= GENERIC_WRITE;
        }
        dwCreationDisposition = OPEN_EXISTING;
    }
    else if (mode[0] == 'w')
    {
        dwDesiredAccess = GENERIC_WRITE;
        if (mode[1] == '+')
        {
            dwDesiredAccess |= GENERIC_READ;
        }
        dwCreationDisposition = CREATE_ALWAYS;
    }
    else if (mode[0] == 'a')
    {
        dwDesiredAccess = GENERIC_WRITE;
        if (mode[1] == '+')
        {
            dwDesiredAccess |= GENERIC_READ;
        }
        dwCreationDisposition = OPEN_ALWAYS;
    }
    else
    {
        return NULL;
    }
    
    stream = malloc(sizeof(SO_FILE));
    if (stream == NULL)
    {
        return NULL;
    }
    stream->handle = CreateFile(pathname,
                                dwDesiredAccess, /* access mode */
                                FILE_SHARE_READ | FILE_SHARE_WRITE, /* sharing option */
                                NULL, /* security attributes */
                                dwCreationDisposition, /* creation disposition */
                                FILE_ATTRIBUTE_NORMAL, /* file attributes */
                                NULL
    );
    if (stream->handle == INVALID_HANDLE_VALUE)
    {
        free(stream);
        return NULL;
    }
    memset(stream->buffer, 0, BUFSIZE);
    stream->buffer_pos = 0;
    stream->buffer_len = 0;
    if (mode[0] == 'a')
    {
        stream->append = APPEND_UNSET;
    }
    else
    {
        stream->append = NO_APPEND;
    }
    stream->file_pos = 0;
    stream->last_op = -1;
    stream->eof = 0;
    stream->error = 0;
    stream->process = INVALID_HANDLE_VALUE;
    return stream;
}

int so_fclose(SO_FILE *stream)
{
    int ret1 = so_fflush(stream);
    BOOL ret2 = CloseHandle(stream->handle);
    free(stream);
    if (ret1 == SO_EOF || ret2 == FALSE)
    {
        return SO_EOF;
    }
    return 0;
}

HANDLE so_fileno(SO_FILE *stream)
{
    return stream->handle;
}

int so_fgetc(SO_FILE *stream)
{
    BOOL bRet;
    DWORD dwBytesRead;
    unsigned char c;
    stream->last_op = OP_READ;
    if (stream->append == APPEND_UNSET)
    {
        stream->append = APPEND_SET;
        stream->file_pos = SetFilePointer(stream->handle,
                                          0,
                                          NULL, /*no 64bytes offset */
                                          FILE_BEGIN
        );
        if (stream->file_pos == INVALID_SET_FILE_POINTER)
        {
            stream->error = 1;
            return SO_EOF;
        }
    }
    if (stream->buffer_pos >= stream->buffer_len)
    {
        /* if buffer is empty */
        bRet = ReadFile(stream->handle, /* file handle */
                        stream->buffer, /* where to put data */
                        BUFSIZE, /* number of bytes to read */
                        &dwBytesRead, /* number of bytes that were read */
                        NULL /* no overlapped structure */
        );
        if (bRet == FALSE)
        {
            stream->error = 1;
            if (stream->process != INVALID_HANDLE_VALUE)
            {
                /* pipe closed */
                stream->eof = 1;
            }
            return SO_EOF;
        }
        else if (dwBytesRead == 0)
        {
            stream->eof = 1;
            return SO_EOF;
        }
        stream->buffer_pos = 1;
        stream->buffer_len = dwBytesRead;
        stream->file_pos++;
        return stream->buffer[0];
    }
    c = stream->buffer[stream->buffer_pos];
    stream->buffer_pos++;
    stream->file_pos++;
    return c;
}
int so_fputc(int c, SO_FILE *stream)
{
    DWORD dwBytes, dwBytesWritten = 0;
    BOOL bRet;
    stream->last_op = OP_WRITE;
    if (stream->append != NO_APPEND)
    {
        if (stream->append == APPEND_UNSET)
        {
            stream->append = APPEND_SET;
        }
        stream->file_pos = SetFilePointer(stream->handle,
                                          0,
                                          NULL, /* no 64bytes offset */
                                          FILE_END
        );
        if (stream->file_pos == INVALID_SET_FILE_POINTER)
        {
            stream->error = 1;
            return SO_EOF;
        }
    }
    if (stream->buffer_len == BUFSIZE)
    {
        /* if buffer is full, write all buffer's content */
        while (dwBytesWritten < stream->buffer_len)
        {
            bRet = WriteFile(stream->handle, /* file handle */
                             stream->buffer + dwBytesWritten, /* start of data to write */
                             stream->buffer_len - dwBytesWritten, /* number of bytes to write */
                             &dwBytes, /* number of bytes that were written */
                             NULL /* no overlapped structure */
            );
            if (bRet == FALSE)
            {
                stream->error = 1;
                return SO_EOF;
            }
            dwBytesWritten += dwBytes;
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
    unsigned char *tmp;
    size_t offset, i, j;
    int c;
    tmp = malloc(size * sizeof(unsigned char));
    if (tmp == NULL)
    {
        return 0;
    }
    stream->last_op = OP_READ;
    for (i = 0; i < nmemb; ++i)
    {
        offset = 0;
        for (j = 0; j < size; ++j)
        {
            c = so_fgetc(stream);
            if (c == SO_EOF)
            {
                break;
            }
            tmp[offset++] = (unsigned char)c;
        }
        memcpy((char*)ptr + i * size, tmp, offset);
        if (offset < size)
        {
            free(tmp);
            return i;
        }
    }
    free(tmp);
    return nmemb;
}

size_t so_fwrite(const void *ptr, size_t size, size_t nmemb, SO_FILE *stream)
{
    
    int c;
    size_t i, j;
    stream->last_op = OP_WRITE;
    for (i = 0; i < nmemb; ++i)
    {
        for (j = 0; j < size; ++j)
        {
            c = so_fputc(*((unsigned char*)ptr + i * size + j), stream);
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
    DWORD dwBytes, dwBytesWritten = 0;
    BOOL bRet;
    if (stream->last_op != OP_WRITE)
    {
        return 0;
    }
    /* write all buffer's content */
    while (dwBytesWritten < stream->buffer_len)
    {
        bRet = WriteFile(stream->handle, /* file handle */
                         stream->buffer + dwBytesWritten, /* start of data to write */
                         stream->buffer_len - dwBytesWritten, /* number of bytes to write */
                         &dwBytes, /* number of bytes that were written */
                         NULL /* no overlapped structure */
        );
        if (bRet == FALSE)
        {
            stream->error = 1;
            return SO_EOF;
        }
        dwBytesWritten += dwBytes;
    }
    stream->buffer_len = 0;
    return 0;
}

int so_fseek(SO_FILE *stream, long offset, int whence)
{
    DWORD pos;
    DWORD dwMoveMethod;
    int ret;
    if (stream->append == APPEND_UNSET)
    {
        stream->append = APPEND_SET;
    }
    switch (whence)
    {
        case SEEK_SET:
            dwMoveMethod = FILE_BEGIN;
            break;
        case SEEK_CUR:
            dwMoveMethod = FILE_CURRENT;
            break;
        case SEEK_END:
            dwMoveMethod = FILE_END;
            break;
        default:
            return -1;
    }
    if (stream->last_op == OP_READ)
    {
        stream->buffer_pos = 0;
        stream->buffer_len = 0;
    }
    else if (stream->last_op == OP_WRITE)
    {
        ret = so_fflush(stream);
        if (ret == SO_EOF)
        {
            return -1;
        }
    }
    if (whence == SEEK_SET || whence == SEEK_END)
    {
        pos = SetFilePointer(stream->handle,
                             offset,
                              NULL, /* no 64bytes offset */
                             dwMoveMethod
        );
        stream->file_pos = pos;
    }
    else /* if (whence == SEEK_CUR) */
    {
        /* first, move cursor to the actual current position */
        pos = SetFilePointer(stream->handle,
                             stream->file_pos,
                              NULL, /* no 64bytes offset */
                             FILE_BEGIN
        );
        if (pos == INVALID_SET_FILE_POINTER)
        {
            stream->error = 1;
            return -1;
        }
        pos = SetFilePointer(stream->handle,
                             offset,
                              NULL, /* no 64bytes offset */
                             dwMoveMethod
        );
    }
    if (pos == INVALID_SET_FILE_POINTER)
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
    SO_FILE *stream;
    SECURITY_ATTRIBUTES sa;
    HANDLE hRead, hWrite; /* pipe handles */
    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    BOOL bRet;
    char cmd[100] = "cmd /C \"";
    char mode = type[0];
    
    if (mode != 'r' && mode != 'w')
    {
        return NULL;
    }
    /* init security attributes, startup info and process info */
    ZeroMemory(&sa, sizeof(sa));
    sa.bInheritHandle = TRUE;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));
    /* create pipe */
    bRet = CreatePipe(&hRead, &hWrite, &sa, 0);
    if (bRet == FALSE)
    {
        return NULL;
    }
    /* set handles from si to current STDIN, STDOUT, STDERR handles */
    si.dwFlags |= STARTF_USESTDHANDLES;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    si.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    if (mode == 'r')
    {
        bRet = SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0);
        si.hStdOutput = hWrite;
    }
    else /* if mode == 'w' */
    {
        bRet = SetHandleInformation(hWrite, HANDLE_FLAG_INHERIT, 0);
        si.hStdInput = hRead;
    }
    if (bRet == FALSE)
    {
        CloseHandle(hRead);
        CloseHandle(hWrite);
        return NULL;
    }
    /* launch command with stdin/stdout redirected */
    strcat(cmd, command);
    strcat(cmd, "\"");
    bRet = CreateProcess(NULL,
                         cmd,
                         NULL,
                         NULL,
                         TRUE,
                         0,
                         NULL,
                         NULL,
                         &si,
                         &pi);
    if (bRet == FALSE)
    {
        CloseHandle(hRead);
        CloseHandle(hWrite);
        return NULL;
    }
    if (mode == 'r')
    {
        bRet = CloseHandle(hWrite);
    }
    else
    {
        bRet = CloseHandle(hRead);
    }
    if (bRet == FALSE)
    {
        WaitForSingleObject(pi.hProcess, INFINITE);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        return NULL;
    }
    stream = malloc(sizeof(SO_FILE));
    if (stream == NULL)
    {
        WaitForSingleObject(pi.hProcess, INFINITE);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        return NULL;
    }
    if (mode == 'r')
    {
        stream->handle = hRead;
    }
    else
    {
        stream->handle = hWrite;
    }
    memset(stream->buffer, 0, BUFSIZE);
    stream->buffer_pos = 0;
    stream->buffer_len = 0;
    stream->append = NO_APPEND;
    stream->file_pos = 0;
    stream->last_op = -1;
    stream->eof = 0;
    stream->error = 0;
    stream->process = pi.hProcess;
    return stream;
}

int so_pclose(SO_FILE *stream)
{
    int ret1;
    BOOL ret2, ret4, ret5;
    DWORD ret3;
    DWORD dwExitCode;
    
    if (stream->process == INVALID_HANDLE_VALUE)
    {
        stream->error = 1;
        return -1;
    }
    ret1 = so_fflush(stream);
    ret2 = CloseHandle(stream->handle);
    ret3 = WaitForSingleObject(stream->process, INFINITE);
    ret4 = GetExitCodeProcess(stream->process, &dwExitCode);
    ret5 = CloseHandle(stream->process);
    free(stream);
    if (ret1 == SO_EOF || ret2 == FALSE || ret3 == WAIT_FAILED || ret4 == FALSE || ret5 == FALSE)
    {
        return -1;
    }
    return dwExitCode;
}