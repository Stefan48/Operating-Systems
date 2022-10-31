/**
 * SO, 2016
 * Lab #12
 *
 * Task 01 - mini.c
 * Implementing a minimal comand line file utility
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/sysmacros.h>
#include <sys/mount.h>
#include <dirent.h>
#include <unistd.h>

#include "utils.h"

#define MAX_LINE_SIZE		256

const char *delim = " \t\n";
char *prompt = "so-lab12";

#define TODO2
#define TODO3
#define TODO4
#define TODO5
#define TODO6
#define TODO7

void print_dir(char *path, int depth)
{
	DIR *d = opendir(path);
	DIE(d == NULL, "opendir");
	struct dirent *entry = NULL;
	char new_path[1024];
	while(1)
	{
		entry = readdir(d);
		if (entry == NULL)
			break;
		if (strncmp(entry->d_name, ".", 1) == 0 || strncmp(entry->d_name, "..", 2) == 0)
			continue;
		for (int i = 0; i < depth; ++i)
			printf("-");
		printf("%s\n", entry->d_name);
		if (entry->d_type & DT_DIR)
		{
			sprintf(new_path, "%s/%s", path, entry->d_name);
			print_dir(new_path, depth + 1);
		}
	}
	DIE(closedir(d) == -1, "closedir");
}

int main(void)
{
	char line[MAX_LINE_SIZE];
	char *cmd, *arg1, *arg2, *arg3;
	int ret; /* to be used for function calls return code */

	while (1) {
		printf("<%s>", prompt);
		fflush(stdout);

		memset(line, 0, MAX_LINE_SIZE);

		if (fgets(line, sizeof(line), stdin) == NULL)
			exit(EXIT_SUCCESS);

		cmd = strtok(line, delim);
		if (!cmd)
			continue;
#ifdef DEBUG
		printf("Executing command: %s\n", cmd);
#endif
		if (strncmp(cmd, "quit", 4) == 0)
			break;
#ifdef TODO2
		/* TODO2: implement list <device_node>
		 * Output: <c|b> <major>:<minor>
		 * e.g: list /dev/zero
		 * Output: /dev/zero: <c> 1:5
		 */
		if (strncmp(cmd, "list", 4) == 0) {
			arg1 = strtok(NULL, delim); /* device_node name */
			if (!arg1)
				continue;
				
			struct stat statbuf;
			DIE(stat(arg1, &statbuf) == -1, "stat");

			printf("%s: <%c> %d:%d\n",
			    //arg1, /* type */, /* major */, /* minor */);
				arg1, S_ISCHR(statbuf.st_mode) ? 'c' : (S_ISBLK(statbuf.st_mode) ? 'b' : '?'), major(statbuf.st_rdev), minor(statbuf.st_rdev));
		}
#endif

#ifdef TODO3
		/* TODO3: implement mount source target fs_type
		 * e.g: mount /dev/sda1 /mnt/disk2 ext3
		 */
		if (strncmp(cmd, "mount", 5) == 0) {
			arg1 = strtok(NULL, delim); /* source */
			arg2 = strtok(NULL, delim); /* target */
			arg3 = strtok(NULL, delim); /* fs_type (e.g: ext2) */
			DIE(mount(arg1, arg2, arg3, 0, NULL) < 0, "mount");
		}
		if (strncmp(cmd, "umount", 6) == 0) {
			/* TODO3: implement umount */
			arg1 = strtok(NULL, delim); /* target */
			DIE(umount(arg1) < 0, "umount");
		}
#endif

#ifdef TODO4
		/* TODO4: implement symlink oldpath newpath
		 * e.g: symlink a.txt b.txt
		 */
		if (strncmp(cmd, "symlink", 7) == 0) {
			arg1 = strtok(NULL, delim); /* oldpath */
			arg2 = strtok(NULL, delim); /* newpath */
			DIE(symlink(arg1, arg2) == -1, "symlink");
		}
		if (strncmp(cmd, "unlink", 6) == 0) {
			/* TODO4: implement unlink */
			arg1 = strtok(NULL, delim); /* pathname */
			DIE(unlink(arg1) == -1, "unlink");
		}
#endif

#ifdef TODO5
		/* TODO5: implement mkdir pathname mode
		 * e.g: mkdir food 0777
		 */
		if (strncmp(cmd, "mkdir", 5) == 0) {
			arg1 = strtok(NULL, delim); /* pathname */
			DIE(mkdir(arg1, (mode_t)0755) == -1, "mkdir");
		}
		if (strncmp(cmd, "rmdir", 5) == 0) {
			/* TODO5: implement rmdir pathname */
			arg1 = strtok(NULL, delim); /* pathname */
			DIE(rmdir(arg1) == -1, "rmdir");
		}
#endif

#ifdef TODO6
		/* TODO6: implement ls dirname
		 * e.g: ls ..
		 */
		if (strncmp(cmd, "ls", 2) == 0) {
			/* recursively print files starting with arg1 */
			arg1 = strtok(NULL, delim);
			print_dir(arg1, 0);
		}
#endif

#ifdef TODO7
		if (strncmp(cmd, "chdir", 5) == 0) {
			/* TODO7: implement chdir <dir>
			 * e.g: chdir bar
			 */
			arg1 = strtok(NULL, delim); /* pathname */
			DIE(chdir(arg1) == -1, "chdir");
		}

		if (strncmp(cmd, "pwd", 3) == 0) {
			/* TODO7: implement pwdir
			 * e.g: pwd
			 */
			/* print workding directory */
			char cwd[256];
			DIE(getcwd(cwd, 256) == NULL, "getcwd");
			printf("%s\n", cwd);
		}
#endif
	}

	return 0;
}
