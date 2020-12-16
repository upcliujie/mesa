/*
 * Copyright © 2020 Google, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "isa.h"

// TODO deduplicate readfile()

#define CHUNKSIZE 32

static void *
readfile(const char *path, int *sz)
{
	char *buf = NULL;
	int fd, ret, n = 0;

	fd = open(path, O_RDONLY);
	if (fd < 0) {
		*sz = 0;
		return NULL;
	}

	while (1) {
		buf = realloc(buf, n + CHUNKSIZE);
		ret = read(fd, buf + n, CHUNKSIZE);
		if (ret < 0) {
			free(buf);
			*sz = 0;
			close(fd);
			return NULL;
		} else if (ret < CHUNKSIZE) {
			n += ret;
			*sz = n;
			close(fd);
			return buf;
		} else {
			n += CHUNKSIZE;
		}
	}
}

int
main(int argc, char **argv)
{
	const char *infile = argv[1];
	const char *outfile = argv[2];
	int cat = atoi(argv[3]);
	int sz;
	uint64_t *instrs = readfile(infile, &sz);

	int num_instr = sz / 8;

	int fd = open(outfile, O_WRONLY | O_CREAT, 0660);

	printf("filtering cat%d\n", cat);
	int n = 0;
	for (int i = 0; i  < num_instr; i++) {
		if (((instrs[i] >> 61) & 0x7) != cat)
			continue;
		write(fd, &instrs[i], sizeof(instrs[i]));
		n++;
	}

	close(fd);

	printf("wrote %d instructions\n", n);

	return 0;
}
