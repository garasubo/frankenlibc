#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/disk.h>
#include <sys/socket.h>
#include <net/if.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#include "rexec.h"

int pfd = -1;

#ifdef NOCAPSICUM
int
os_init(char *program, int nx)
{

	if (nx == 1) {
		fprintf(stderr, "cannot disable mprotect execution\n");
		exit(1);
	}

	pfd = open(program, O_EXEC | O_CLOEXEC);
	if (pfd == -1) {
		perror("open");
		exit(1);
	}

	return 0;
}

int
os_pre()
{

	return 0;
}

void
os_dropcaps()
{
}

int
filter_fd(int fd, int flags, struct stat *st)
{

	return 0;
}
#else

#include <sys/capability.h>

int
os_init(char *program, int nx)
{

	if (nx == 1) {
		fprintf(stderr, "cannot disable mprotect execution\n");
		exit(1);
	}
	pfd = open(program, O_EXEC | O_CLOEXEC);
	if (pfd == -1) {
		perror("open");
		exit(1);
	}

	return 0;
}

int
os_pre()
{

	if (cap_enter() == -1) {
		perror("cap_enter");
		exit(1);
	}

	return 0;
}

void
os_dropcaps()
{
}

int
filter_fd(int fd, int flags, struct stat *st)
{
	cap_rights_t rights;
	int ret;
	unsigned long ioctlb[1] = {DIOCGMEDIASIZE};
	unsigned long ioctlc[1] = {SIOCGIFADDR};

	if (fd == pfd) {
		cap_rights_init(&rights, CAP_READ, CAP_FEXECVE);
		ret = cap_rights_limit(pfd, &rights);
		if (ret == -1) return ret;
		return 0;
	}

	/* XXX we could cut capabilities down a little further */
	switch (flags & O_ACCMODE) {
	case O_RDONLY:
		if (S_ISBLK(st->st_mode) || S_ISCHR(st->st_mode)) {
			cap_rights_init(&rights, CAP_READ,
				CAP_SEEK, CAP_FSYNC, CAP_FCNTL, CAP_FSTAT, CAP_IOCTL);
		} else {
			cap_rights_init(&rights, CAP_READ,
				CAP_SEEK, CAP_FSYNC, CAP_FCNTL, CAP_FSTAT);
		}
		break;
	case O_WRONLY:
		if (S_ISBLK(st->st_mode) || S_ISCHR(st->st_mode)) {
			cap_rights_init(&rights, CAP_WRITE,
				CAP_SEEK, CAP_FSYNC, CAP_FCNTL, CAP_FSTAT, CAP_IOCTL);
		} else {
			cap_rights_init(&rights, CAP_WRITE,
				CAP_SEEK, CAP_FSYNC, CAP_FCNTL, CAP_FSTAT);
		}
		break;
	case O_RDWR:
		if (S_ISBLK(st->st_mode) || S_ISCHR(st->st_mode)) {
			cap_rights_init(&rights, CAP_READ, CAP_WRITE,
				CAP_SEEK, CAP_FSYNC, CAP_FCNTL, CAP_FSTAT, CAP_IOCTL);
		} else if (S_ISCHR(st->st_mode)) {
			cap_rights_init(&rights, CAP_READ, CAP_WRITE,
				CAP_SEEK, CAP_FSYNC, CAP_FCNTL, CAP_FSTAT, CAP_IOCTL, CAP_EVENT);
		} else {
			cap_rights_init(&rights, CAP_READ, CAP_WRITE,
				CAP_SEEK, CAP_FSYNC, CAP_FCNTL, CAP_FSTAT);
		}
		break;
	default:
		abort();
	}
	ret = cap_rights_limit(fd, &rights);
	if (ret == -1) return ret;

	if (S_ISBLK(st->st_mode)) {
		ret = cap_ioctls_limit(fd, ioctlb, 1);
		if (ret == -1) return ret;
	} else if (S_ISCHR(st->st_mode)) {
		ret = cap_ioctls_limit(fd, ioctlc, 1);
		if (ret == -1) return ret;
	}

	ret = cap_fcntls_limit(fd, CAP_FCNTL_GETFL);
	if (ret == -1) return ret;

	return 0;
}
#endif /* CAPSICUM */

int
filter_load_exec(char *program, char **argv, char **envp)
{

	if (fexecve(pfd, argv, envp) == -1) {
		perror("fexecve");
		exit(1);
	}

	return 0;
}

int
os_emptydir()
{

	return emptydir();
}

int
os_extrafiles()
{

	return 0;
}

int
os_open(char *pre, char *post)
{

	fprintf(stderr, "platform does not support %s:%s\n", pre, post);
	exit(1);
}
