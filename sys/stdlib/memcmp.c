#include <string.h> /* size_t */
int memcmp(const void *s1, const void *s2, size_t n);
int memcmp(const void *s1, const void *s2, size_t n)
{
	register unsigned char *c1 = (unsigned char *) s1;
	register unsigned char *c2 = (unsigned char *) s2;

	while (n-- > 0)
		if (*c1++ != *c2++)
			return c1[-1] > c2[-1] ? 1 : -1;
	return 0;
}
