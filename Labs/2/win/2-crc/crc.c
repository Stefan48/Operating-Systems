/**
 * SO, 2017
 * Lab #2, Operatii I/O simple
 *
 * Task #2, Windows
 *
 * Implementing simple crc method
 */

/* do not use UNICODE */
#undef _UNICODE
#undef UNICODE

#include <windows.h>
#include <stdio.h>

#include <utils.h>
#include <crc32.h>

#define BUFSIZE 512
#define CHUNKSIZE 32

static void WriteCrc(int crc, HANDLE hWrite)
{
	BOOL bRet;
	DWORD dwBytesWritten, dwBytesToWrite, dwTotalWritten;
	/* TODO 1 - Write the CRC to the file. Use a loop! */
	dwBytesToWrite = sizeof(crc);
	dwTotalWritten = 0;
	do {
		bRet = WriteFile(hWrite,
			   &crc + dwTotalWritten,
			   dwBytesToWrite,
			   &dwBytesWritten,
			   NULL);
		DIE(bRet == FALSE, "WriteCrc");
		dwTotalWritten += dwBytesWritten;
		dwBytesToWrite -= dwBytesWritten;
	} while (dwBytesToWrite > 0);
}

static void GenerateCrc(CHAR *sourceFile, CHAR *destFile)
{
	HANDLE hRead, hWrite;
	CHAR buf[BUFSIZE];
	BOOL bRet;
	DWORD bytesRead;
	int crc = 0;

	/* TODO 1 - Open source file for reading */
	hRead = CreateFile(sourceFile,
			GENERIC_READ,
			0,
			NULL,
			OPEN_EXISTING,
			FILE_ATTRIBUTE_NORMAL,
			NULL);
	DIE(hRead == INVALID_HANDLE_VALUE, "CreateFile sourceFile");

	/* TODO 1 - Create destination file for writing */
	hWrite = CreateFile(destFile,
			 GENERIC_WRITE,
			 0,
			 NULL,
			 CREATE_ALWAYS,
			 FILE_ATTRIBUTE_NORMAL,
			 NULL);
	DIE(hWrite == INVALID_HANDLE_VALUE, "CreateFile destFile");

	/* Read from file  */
	while (1) {
		ZeroMemory(buf, sizeof(buf));

		/* TODO 1 - Read from source file into buf BUFSIZE bytes */
		bRet = ReadFile(hRead,
						buf,
						BUFSIZE,
						&bytesRead,
						NULL);
		DIE(bRet == FALSE, "ReadFile");

		/* TODO 1 - Test for end of file */
		if (bytesRead == 0)
			break;

		/* Calculate crc for buf */
		crc = update_crc(crc, (unsigned char *) buf, bytesRead);
	}

	/* Write crc to destination file */
	WriteCrc(crc, hWrite);

	/* TODO 1 - Close files */
	bRet = CloseHandle(hRead);
	DIE(bRet == FALSE, "CloseHandle");
	bRet = CloseHandle(hWrite);
	DIE(bRet == FALSE, "CloseHandle");
}


static DWORD GetSize(HANDLE file)
{
	DWORD dwSize;

	/* TODO 2 - Calculate and return file size using SetFilePointer */
	dwSize = SetFilePointer(
             file,
             0,           /* offset 0 */
             NULL,        /* no 64bytes offset */
             FILE_END);
	DIE(dwSize == INVALID_SET_FILE_POINTER, "SetFilePointer");
	DIE(SetFilePointer(
        file,
        0,           /* offset 0 */
        NULL,        /* no 64bytes offset */
        FILE_BEGIN) == INVALID_SET_FILE_POINTER, "SetFilePointer");

	return dwSize;
}

static DWORD ReadChunk(CHAR *chunk, HANDLE hFile)
{
	BOOL bRet;
	DWORD dwBytesRead, dwBytesToRead = CHUNKSIZE, dwTotalRead = 0;

	/*
	 * TODO 3
	 * Read at most CHUNKSIZE bytes from file into the buffer. Use a loop!
	 * Return the number of read bytes.
	 */
	while (dwBytesToRead > 0) {
		bRet = ReadFile(hFile,
						chunk + dwTotalRead,
						dwBytesToRead,
						&dwBytesRead,
						NULL);
		DIE(bRet == FALSE, "ReadChunk");

		if (dwBytesRead == 0)
			break;
		dwTotalRead += dwBytesRead;
		dwBytesToRead -= dwBytesRead;
		
	}

	return dwTotalRead;
}

static BOOL CompareFiles(CHAR *file1, CHAR *file2)
{
	DWORD bytesRead1, bytesRead2, size1, size2, i;
	HANDLE hFile1, hFile2;
	CHAR chunk1[CHUNKSIZE], chunk2[CHUNKSIZE];
	//BOOL result = FALSE, bRet;
	BOOL result = TRUE, bRet, stop;

	/* TODO 4 - Open file handles */
	hFile1 = CreateFile(file1,
			 GENERIC_READ,
			 FILE_SHARE_READ,
			 NULL,
			 OPEN_EXISTING,
			 FILE_ATTRIBUTE_NORMAL,
			 NULL);
	DIE(hFile1 == INVALID_HANDLE_VALUE, "CompareFiles file1");
	hFile2 = CreateFile(file2,
			 GENERIC_READ,
			 FILE_SHARE_READ,
			 NULL,
			 OPEN_EXISTING,
			 FILE_ATTRIBUTE_NORMAL,
			 NULL);
	DIE(hFile2 == INVALID_HANDLE_VALUE, "CompareFiles file2");

	/* TODO 4 - Compare file size */
	size1 = GetSize(hFile1);
	size2 = GetSize(hFile2);
	if (size1 != size2)
	{
		result = FALSE;
		goto exit;
	}

	/* TODO 4 - Compare the files, chunk by chunk */
	stop = FALSE;
	//while (1) {
	while (!stop) {
		ZeroMemory(chunk1, sizeof(chunk1));
	 	ZeroMemory(chunk2, sizeof(chunk2));

		bytesRead1 = ReadChunk(chunk1, hFile1);
		bytesRead2 = ReadChunk(chunk2, hFile2);

		/* TODO 4 - Test for the end of the files */
		if (bytesRead1 == 0 || bytesRead2 == 0)
		{
			stop = TRUE;
		}
		
		/* TODO 4 - Compare the previously read chunks */
		for (i = 0; i < CHUNKSIZE; ++i)
		{
			//printf("*** DEBUG ***\n %d %d\n", (int)chunk1[i], (int)chunk2[i]);
			if (chunk1[i] != chunk2[i])
			{
				result = FALSE;
				goto exit;
			}
		}
	}

exit:
	/* TODO 4 - Close files */
	bRet = CloseHandle(hFile1);
	DIE(bRet == FALSE, "CloseHandle");
	bRet = CloseHandle(hFile2);
	DIE(bRet == FALSE, "CloseHandle");

	return result;
}

int main(int argc, char *argv[])
{
	BOOL equal;

	if (argc != 4) {
		fprintf(stderr, "Usage:\n"
				"\tcrc.exe -g <input_file> <output_file> - generate crc\n"
				"\tcrc.exe -c <file1> <file2>            - compare files\n");
		exit(EXIT_FAILURE);
	}

	if (strcmp(argv[1], "-g") == 0)
		GenerateCrc(argv[2], argv[3]);

	if (strcmp(argv[1], "-c") == 0) {
		equal = CompareFiles(argv[2], argv[3]);

		if (equal)
			printf("Files are equal\n");
		else
			printf("Files differ\n");
	}

	return 0;
}
