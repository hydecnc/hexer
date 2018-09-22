/*-
 * Copyright (c) 2016, 2018 Peter Pentchev
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *    If you modify any part of HEXER and redistribute it, you must add
 *    a notice to the `README' file and the modified source files containing
 *    information about the  changes you made.  I do not want to take
 *    credit or be blamed for your modifications.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *    If you modify any part of HEXER and redistribute it in binary form,
 *    you must supply a `README' file containing information about the
 *    changes you made.
 * 3. The name of the developer may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * HEXER WAS DEVELOPED BY SASCHA DEMETRIO.
 * THIS SOFTWARE SHOULD NOT BE CONSIDERED TO BE A COMMERCIAL PRODUCT.
 * THE DEVELOPER URGES THAT USERS WHO REQUIRE A COMMERCIAL PRODUCT
 * NOT MAKE USE OF THIS WORK.
 *
 * DISCLAIMER:
 * THIS SOFTWARE IS PROVIDED BY THE DEVELOPER ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE DEVELOPER BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#if defined(TEST_SIGTYPE_INT)
#include <signal.h>
#elif defined(TEST_ALLOCA_H)
#include <alloca.h>
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

#if defined(TEST_SIGTYPE_INT)
	buf[0] = '0' + (signal(SIGINT, handler) != SIG_ERR);
#elif defined(TEST_ALLOCA_H)
	buf[0] = '1';
#elif defined(TEST_VASPRINTF)
	const char * const res = run_vasprintf(buf, 1);
	buf[0] = res[0];
#else
#error No option to test
#endif

	return buf[0];
}
