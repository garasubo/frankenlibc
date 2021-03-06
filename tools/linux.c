#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <linux/fs.h>
#include <linux/if_tun.h>
#include <linux/sockios.h>
#include <linux/if_packet.h>
#include <linux/capability.h>
#include <linux/rtnetlink.h>

#include "rexec.h"

/* only in libcap header, not linux header */
int capset(cap_user_header_t, cap_user_data_t);

/* only in linux/fcntl.h that cannot be included */
#ifndef AT_EMPTY_PATH
#define AT_EMPTY_PATH		0x1000
#endif

#ifndef SYS_getrandom
#if defined(__x86_64__)
#define SYS_getrandom 318
#elif defined(__i386__)
#define SYS_getrandom 355
#elif defined(__ARMEL__) || defined(__ARMEB__)
#define SYS_getrandom 384
#elif defined(__AARCH64EL__) || defined (__AARCH64EB__)
#define SYS_getrandom 278
#elif defined(__PPC64__)
#define SYS_getrandom 359
#elif defined(__PPC__)
#define SYS_getrandom 359
#elif defined(__MIPSEL__) || defined(__MIPSEB__)
#define SYS_getrandom 4353
#else
#error "Unknown architecture"
#endif
#endif

#ifndef SECCOMP
int
os_init(char *program, int nx)
{

	if (nx == 1) {
		fprintf(stderr, "No seccomp, cannot disable mprotect execution\n");
		exit(1);
	}
	return 0;
}

int
filter_fd(int fd, int flags, struct stat *st)
{

	return 0;
}

int
os_emptydir()
{

	return 0;
}

int
os_pre()
{

	return 0;
}

int
filter_load_exec(char *program, char **argv, char **envp)
{

	if (execve(program, argv, envp) == -1) {
		perror("execve");
		exit(1);
	}

	return 0;
}
#else

#include <seccomp.h>

#ifdef SYS_arch_prctl
#include <asm/prctl.h>
#include <sys/prctl.h>
#endif

scmp_filter_ctx ctx;

#ifdef EXECVEAT
int pfd = -1;
#endif

int
os_init(char *program, int nx)
{
	int ret;

	ctx = seccomp_init(SCMP_ACT_KILL);
	if (ctx == NULL)
		return -1;

#ifdef EXECVEAT
        pfd = open(program, O_RDONLY | O_CLOEXEC);

        if (pfd == -1) {
                perror("open executable");
                exit(1);
        }
#endif

	/* arch_prctl(ARCH_SET_FS, x) */
#ifdef SYS_arch_prctl
	ret = seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(arch_prctl), 1,
		SCMP_A0(SCMP_CMP_EQ, ARCH_SET_FS));
	if (ret < 0) return ret;
#endif

	/* exit(x) */
	ret = seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(exit), 0);
	if (ret < 0) return ret;

	/* exit_group(x) */
	ret = seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(exit_group), 0);
	if (ret < 0) return ret;

	/* clock_gettime(x, y) */
	ret = seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(clock_gettime), 0);
	if (ret < 0) return ret;

	/* clock_getres(x, y) */
	ret = seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(clock_getres), 0);
	if (ret < 0) return ret;

	/* clock_nanosleep(w, x, y, z) */
	ret = seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(clock_nanosleep), 0);
	if (ret < 0) return ret;

	/* getrandom(x, y, z) */
	ret = seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SYS_getrandom, 0);
	if (ret < 0) return ret;

	/* kill(0, SIGABRT) */
	ret = seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(kill), 2,
		SCMP_A0(SCMP_CMP_EQ, 0), SCMP_A1(SCMP_CMP_EQ, SIGABRT));
	if (ret < 0) return ret;

	/* mmap(a, b, c, d, -1, e) */
#ifdef SYS_mmap2
#define MMAP SCMP_SYS(mmap2)
#else
#define MMAP SCMP_SYS(mmap)
#endif
	if (nx == 0)
		ret = seccomp_rule_add(ctx, SCMP_ACT_ALLOW, MMAP, 1,
			SCMP_A4(SCMP_CMP_EQ, -1));
	else
		ret = seccomp_rule_add(ctx, SCMP_ACT_ALLOW, MMAP, 2,
			SCMP_A2(SCMP_CMP_MASKED_EQ, PROT_EXEC, 0), SCMP_A4(SCMP_CMP_EQ, -1));
	if (ret < 0) return ret;

	/* mprotect(a, b, c) */
	if (nx == 0)
		ret = seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(mprotect), 0);
	else /* do not allow executable pages */
		ret = seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(mprotect), 1,
			SCMP_A2(SCMP_CMP_MASKED_EQ, PROT_EXEC, 0));
	if (ret < 0) return ret;

	/* munmap(a, b) */
	ret = seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(munmap), 0);
	if (ret < 0) return ret;

	/* fstat(a, b) as used to check for existence */
#ifdef SYS_fstat64
	ret = seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(fstat64), 0);
#else
	ret = seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(fstat), 0);
#endif
	if (ret < 0) return ret;

	/* allow ppoll for network readiness */
	ret = seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(ppoll), 0);
	if (ret < 0) return ret;

	return 0;
}

int
os_pre()
{

	return 0;
}

int
filter_fd(int fd, int flags, struct stat *st)
{
	int ret;

	/* read(fd, ...), pread(fd, ...) */
	if ((flags & O_ACCMODE) == O_RDONLY || (flags & O_ACCMODE) == O_RDWR) {
		ret = seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(read), 1,
			SCMP_A0(SCMP_CMP_EQ, fd));
		if (ret < 0) return ret;
		ret = seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(pread64), 1,
			SCMP_A0(SCMP_CMP_EQ, fd));
		if (ret < 0) return ret;
		ret = seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(readv), 1,
			SCMP_A0(SCMP_CMP_EQ, fd));
		if (ret < 0) return ret;
		ret = seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(preadv), 1,
			SCMP_A0(SCMP_CMP_EQ, fd));
	}

	/* write(fd, ...), pwrite(fd, ...) */
	if ((flags & O_ACCMODE) == O_WRONLY || (flags & O_ACCMODE) == O_RDWR) {
		ret = seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(write), 1,
			SCMP_A0(SCMP_CMP_EQ, fd));
		if (ret < 0) return ret;
		ret = seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(pwrite64), 1,
			SCMP_A0(SCMP_CMP_EQ, fd));
		if (ret < 0) return ret;
		ret = seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(writev), 1,
			SCMP_A0(SCMP_CMP_EQ, fd));
		if (ret < 0) return ret;
		ret = seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(pwritev), 1,
			SCMP_A0(SCMP_CMP_EQ, fd));
	}

	/* fsync(fd) */
	ret = seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(fsync), 1,
		SCMP_A0(SCMP_CMP_EQ, fd));
	if (ret < 0) return ret;

	/* fcntl(fd, F_GETFL, ...) */
#ifdef SYS_fcntl64
	ret = seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(fcntl64), 2,
		SCMP_A0(SCMP_CMP_EQ, fd), SCMP_A1(SCMP_CMP_EQ, F_GETFL));
#else
	ret = seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(fcntl), 2,
		SCMP_A0(SCMP_CMP_EQ, fd), SCMP_A1(SCMP_CMP_EQ, F_GETFL));
#endif
	if (ret < 0) return ret;

	/* ioctl(fd, BLKGETSIZE64) for block devices */
	if (S_ISBLK(st->st_mode)) {
		ret = seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(ioctl), 2,
			SCMP_A0(SCMP_CMP_EQ, fd), SCMP_A1(SCMP_CMP_EQ, BLKGETSIZE64));
		if (ret < 0) return ret;
	}

	/* XXX be more specific, only for tap devices */
	if (S_ISCHR(st->st_mode)) {
		ret = seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(ioctl), 2,
			SCMP_A0(SCMP_CMP_EQ, fd), SCMP_A1(SCMP_CMP_EQ, TUNGETIFF));
		if (ret < 0) return ret;
		ret = seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(ioctl), 2,
			SCMP_A0(SCMP_CMP_EQ, fd), SCMP_A1(SCMP_CMP_EQ, TUNSETIFF));
		if (ret < 0) return ret;
		ret = seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(ioctl), 2,
			SCMP_A0(SCMP_CMP_EQ, fd), SCMP_A1(SCMP_CMP_EQ, SIOCGIFHWADDR));
		if (ret < 0) return ret;
	}
	/* XXX for dummy socket and packet sockets */
	if (S_ISSOCK(st->st_mode)) {
		ret = seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(ioctl), 2,
			SCMP_A0(SCMP_CMP_EQ, fd), SCMP_A1(SCMP_CMP_EQ, SIOCGIFHWADDR));
		if (ret < 0) return ret;
		ret = seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(ioctl), 2,
			SCMP_A0(SCMP_CMP_EQ, fd), SCMP_A1(SCMP_CMP_EQ, SIOCGIFNAME));
		if (ret < 0) return ret;
		ret = seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(getsockname), 1,
			SCMP_A0(SCMP_CMP_EQ, fd));
		if (ret < 0) return ret;
	}

	return 0;
}

/* without being able to exec a file descriptor, we cannot lock down as
   much, but this is only a very recent facility in Linux */
#ifdef EXECVEAT
/* not yet defined by libc */
int execveat(int, const char *, char *const [], char *const [], int);

int
execveat(int dirfd, const char *pathname,
	char *const argv[], char *const envp[], int flags)
{

	return syscall(SYS_execveat, dirfd, pathname, argv, envp, flags);
}

int
os_emptydir()
{

	return emptydir();
}

int
filter_load_exec(char *program, char **argv, char **envp)
{
	int ret;
	const char *emptystring = "";

	/* lock down execveat to exactly what we need to exec program */
	ret = seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(execveat), 5,
		SCMP_A0(SCMP_CMP_EQ, pfd),
		SCMP_A1(SCMP_CMP_EQ, (long)emptystring),
		SCMP_A2(SCMP_CMP_EQ, (long)argv),
		SCMP_A3(SCMP_CMP_EQ, (long)envp),
		SCMP_A4(SCMP_CMP_EQ, AT_EMPTY_PATH));
	if (ret < 0) return ret;

	ret = seccomp_load(ctx);
	if (ret < 0) {
		fprintf(stderr, "seccomp_load failed, probably no kernel support\n");
		return ret;
	}

	seccomp_release(ctx);

	if (execveat(pfd, emptystring, argv, envp, AT_EMPTY_PATH) == -1) {
		perror("execveat");
		exit(1);
	}

	return 0;
}
#else /* execveat */
int
os_emptydir()
{

	/* need to be able to see executable */
	return 0;
}

int
filter_load_exec(char *program, char **argv, char **envp)
{
	int ret;

	ret = seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(execve), 0);
	if (ret < 0) return ret;

	ret = seccomp_load(ctx);
	if (ret < 0) return ret;

	seccomp_release(ctx);

	if (execve(program, argv, envp) == -1) {
		perror("execve");
		exit(1);
	}

	return 0;
}
#endif /* execveat */

#endif /* SECCOMP */

int getrandom(void *, size_t, unsigned int);

int
getrandom(void *buf, size_t buflen, unsigned int flags)
{

	return syscall(SYS_getrandom, buf, buflen, flags);
}

void
os_dropcaps()
{
	struct __user_cap_header_struct hdr = {.version = _LINUX_CAPABILITY_VERSION_3, .pid = 0};
	struct __user_cap_data_struct data[2];

	memset(data, 0, sizeof(data));
	if (capset(&hdr, data) == -1) {
		perror("capset");
		exit(1);
	}
}

int
os_extrafiles()
{
	int sock[2];
	int ret;
	int fd __attribute__((unused));
	char buf[1];

	/* if getrandom() syscall works we do not need to pass random source in */
	if (getrandom(buf, 1, 0) == -1 && errno == ENOSYS) {
		fd = open("/dev/urandom", O_RDONLY);
		if (fd == -1) {
			perror("open /dev/urandom");
			exit(1);
		}
	}

	/* Linux needs a socket to do ioctls on to get mac addresses from macvtap devices */
	/* Fix upstreamed */
	ret = socketpair(AF_UNIX, SOCK_STREAM, 0, sock);
	close(sock[1]);

	return ret;
}

int
readRoutes(int sock, char *buf, size_t size, unsigned int seq, unsigned int pid)
{
	struct nlmsghdr *hdr;
	int n, len = 0;

	do {
		n = recv(sock, buf, size - len, 0);
		if (n == -1) {
			perror("recv");
			return -1;
		}

		hdr = (struct nlmsghdr *)buf;

		if ((NLMSG_OK(hdr, n) == 0) || (hdr->nlmsg_type == NLMSG_ERROR))
			return -1;

		if (hdr->nlmsg_type == NLMSG_DONE)
			break;

		buf += n;
		len += n;

		if ((hdr->nlmsg_flags & NLM_F_MULTI) == 0)
			break;

	} while ((hdr->nlmsg_seq != seq) || (hdr->nlmsg_pid != pid));

	return len;
}

int
parseRoutes(struct nlmsghdr *hdr, struct in_addr *dst, struct in_addr *gw)
{
	struct rtmsg *msg;
	struct rtattr *attr;
	int rtlen;

	msg = (struct rtmsg *)NLMSG_DATA(hdr);

	if ((msg->rtm_family != AF_INET) || (msg->rtm_table != RT_TABLE_MAIN))
		return -1;

	attr = (struct rtattr *)RTM_RTA(msg);
	rtlen = RTM_PAYLOAD(hdr);

	for (; RTA_OK(attr,rtlen); attr = RTA_NEXT(attr,rtlen)) {
		switch(attr->rta_type) {
		case RTA_OIF:
			break;
		case RTA_GATEWAY:
			memcpy(gw, RTA_DATA(attr), sizeof(struct in_addr));
			break;
		case RTA_PREFSRC:
			break;
		case RTA_DST:
			memcpy(dst, RTA_DATA(attr), sizeof(struct in_addr));
			break;
		}
	}

	return 0;
}

int get_gateway(struct in_addr *gw)
{
	struct nlmsghdr *nlmsg;
	char buf[8192];
	int sock, len, seq = 0;
	struct in_addr dest;
	int ret = -1;

	sock = socket(PF_NETLINK, SOCK_DGRAM, NETLINK_ROUTE);
	if (sock == -1) {
		perror("socket");
		return -1;
	}

	nlmsg = (struct nlmsghdr *)buf;

	nlmsg->nlmsg_len = NLMSG_LENGTH(sizeof(struct rtmsg));
	nlmsg->nlmsg_type = RTM_GETROUTE;
	nlmsg->nlmsg_flags = NLM_F_DUMP | NLM_F_REQUEST;
	nlmsg->nlmsg_seq = seq++;
	nlmsg->nlmsg_pid = getpid();

	if (send(sock, nlmsg, nlmsg->nlmsg_len, 0) == -1) {
		perror("send");
		return -1;
	}

	len = readRoutes(sock, buf, sizeof(buf), nlmsg->nlmsg_seq, nlmsg->nlmsg_pid);
	if (len == -1) {
		return -1;
	}

	for (; NLMSG_OK(nlmsg, len); nlmsg = NLMSG_NEXT(nlmsg, len)) {
		if (parseRoutes(nlmsg, &dest, gw) == -1)
			continue;

		if (dest.s_addr == 0) { /* if dest = 0.0.0.0 ie default gw */
			ret = 0;
			break;
		}
 	}

	close(sock);

	return ret;
}

int
packet_open(char *post)
{
	struct ifreq ifr;
	int sock, flags;
	struct sockaddr_ll sa = {0};

	sock = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
	if (sock == -1) {
		/* probably missing CAP_NET_RAW */
		perror("socket AF_PACKET");
		return -1;
	}
	strncpy(ifr.ifr_name, post, IFNAMSIZ);
	if (ioctl(sock, SIOCGIFINDEX, &ifr) == -1)
		return -1;

	sa.sll_family = AF_PACKET;
	sa.sll_ifindex = ifr.ifr_ifindex;
	sa.sll_halen = ETH_ALEN;
	sa.sll_protocol = htons(ETH_P_ALL);
	if (bind(sock, (struct sockaddr *)&sa, sizeof(sa)) == -1) {
		perror("bind AF_PACKET");
		return -1;
	}
	flags = fcntl(sock, F_GETFL, 0);
	if (flags == -1)
		return -1;
	if (fcntl(sock, F_SETFL, flags | O_NONBLOCK) == -1)
		return -1;
	return sock;
}

int
os_open(char *pre, char *post)
{

	/* eg tun:tap0 for tap0 */
	if (strcmp(pre, "tun") == 0 || strcmp(pre, "tap") == 0) {
		struct ifreq ifr;
		int fd;

		strncpy(ifr.ifr_name, post, IFNAMSIZ);
		ifr.ifr_flags = IFF_TAP | IFF_NO_PI;

		fd = open("/dev/net/tun", O_RDWR | O_NONBLOCK);
		if (fd == -1) {
			perror("open /dev/net/tun");
			return -1;
		}

		if (ioctl(fd, TUNSETIFF, &ifr) == -1) {
			perror("TUNSETIFF");
			return -1;
		}

		return fd;
	}

	/* eg packet:eth0 for packet socket on eth0 */
	if (strcmp(pre, "packet") == 0) {
		return packet_open(post);
	}

	/* docker networking is packet socket plus fixed addresses */
	if (strcmp(pre, "docker") == 0) {
		int sock;
		struct ifreq ifr;
		char addr[INET_ADDRSTRLEN];
		struct sockaddr_in *sa;
		struct in_addr gw;
		uint32_t mask;
		int n = 32;

		if (strlen(post) == 0)
			post = "eth0";

		sock = packet_open(post);
		if (sock == -1)
			return -1;
		memcpy(ifr.ifr_ifrn.ifrn_name, post, IF_NAMESIZE);
		if (ioctl(sock, SIOCGIFADDR, &ifr) == -1)
			return -1;
		
		sa = (struct sockaddr_in *)&ifr.ifr_ifru.ifru_addr;
		inet_ntop(AF_INET, &sa->sin_addr, addr, sizeof(addr));
	
		setenv("FIXED_ADDRESS", addr, 1);

		if (ioctl(sock, SIOCGIFNETMASK, &ifr) == -1)
			return -1;

		sa = (struct sockaddr_in *)&ifr.ifr_ifru.ifru_addr;
		mask = ~htonl(sa->sin_addr.s_addr);

		while (mask) {
			mask >>= 1;
			n--;
		}

		snprintf(addr, sizeof(addr), "%d", n);

		setenv("FIXED_MASK", addr, 1);

		if (get_gateway(&gw) == -1)
			return -1;

		inet_ntop(AF_INET, &gw, addr, sizeof(addr));

		setenv("FIXED_GATEWAY", addr, 1);

		/* delete address from interface */
		sa = (struct sockaddr_in *) &ifr.ifr_ifru.ifru_addr;
		memset(&sa->sin_addr, 0, sizeof(struct in_addr));
		if (ioctl(sock, SIOCSIFADDR, &ifr) == -1) {
			perror("ioctl SIOCSIFADDR (need CAP_NET_ADMIN?)");
			return -1;
		}

		return sock;
	}

	fprintf(stderr, "platform does not support %s:%s\n", pre, post);
	exit(1);
}
