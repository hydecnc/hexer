#if defined(TEST_STRCMP)
#include <string.h>
#elif defined(TEST_STRCASECMP)
#include <string.h>
#include <strings.h>
#elif defined(TEST_SIGTYPE_INT)
#include <signal.h>
#elif defined(TEST_MEMMOVE)
#include <string.h>
#elif defined(TEST_FLOAT_H)
#include <float.h>
#elif defined(TEST_ALLOCA_H)
#include <alloca.h>
#elif defined(TEST_STRERROR)
#include <stdio.h>
#include <string.h>
#elif defined(TEST_VASPRINTF)
#define _GNU_SOURCE
#include <stdarg.h>
#include <stdio.h>
#else
#error No option to test
#endif

#if defined(TEST_SIGTYPE_INT)
static int handler(void)
{
	return 1;
}
#endif

#if defined(TEST_VASPRINTF)
static const char *run_vasprintf(char * const dst, ...)
{
	char *res;
	int ret;
	va_list v;

	va_start(v, dst);
	ret = vasprintf(&res, "%d", v);
	va_end(v);

	return (ret == -1? "0": res);
}
#endif

int main(void)
{
	char buf[128] = "1";

#if defined(TEST_STRCMP)
	buf[0] = '0' + (strcmp("a", "b") != 0);
#elif defined(TEST_STRCASECMP)
	buf[0] = '0' + (strcasecmp("a", "b") != 0);
#elif defined(TEST_SIGTYPE_INT)
	buf[0] = '0' + (signal(SIGINT, handler) != SIG_ERR);
#elif defined(TEST_MEMMOVE)
	buf[1] = '1';
	buf[2] = '\0';
	memmove(buf, buf + 1, 2);
#elif defined(TEST_FLOAT_H)
	buf[0] = '1';
#elif defined(TEST_ALLOCA_H)
	buf[0] = '1';
#elif defined(TEST_STRERROR)
	const char *res = strerror(2);
	buf[0] = res[0];
#elif defined(TEST_VASPRINTF)
	const char * const res = run_vasprintf(buf, 1);
	buf[0] = res[0];
#else
#error No option to test
#endif

	return buf[0];
}
