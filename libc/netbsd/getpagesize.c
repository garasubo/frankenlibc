#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#define CTL_HW 6
#define HW_PAGESIZE 7

int __franken_sysctl(const int *, unsigned int, void *, size_t *, const void *, size_t);

int
getpagesize(void)
{
	const int ctl[2] = {CTL_HW, HW_PAGESIZE};
	static int pgsize = 0;
	size_t lenp = sizeof(int);
	int res;

	if (pgsize > 0)
		return pgsize;

	res = __franken_sysctl(ctl, 2, &pgsize, &lenp, NULL, 0);
	if (res == -1)
		abort();

	return pgsize;
}
