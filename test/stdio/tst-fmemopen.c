#include <features.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static char *text_input = "1 23 43";

static const char *good_answer = "1 529 1849 ";


static int
check (const char *what, const char *buf, size_t size,
       const char *want, size_t want_size)
{
    size_t i;

    if (size == want_size && memcmp(buf, want, want_size) == 0)
        return 0;

    printf("%s: size=%zu, expected %zu\n  got     ", what, size, want_size);
    for (i = 0; i < size; i++)
        printf(" %02x", (unsigned char) buf[i]);
    printf("\n  expected");
    for (i = 0; i < want_size; i++)
        printf(" %02x", (unsigned char) want[i]);
    puts("");
    return 1;
}

/* POSIX says *sizep is "the smaller of the current buffer length and the
   number of bytes between the beginning of the buffer and the current file
   position indicator" -- the position, not the largest offset ever written. */
static int
seek_tests (void)
{
    FILE *out;
    char *ptr;
    size_t size;
    int bad = 0;

    /* A seek past the end moves the position, so the size follows it even
       though nothing was written out there.  The gap reads as zero.  */
    out = open_memstream(&ptr, &size);
    if (out == NULL) {
        perror("open_memstream");
        return 1;
    }
    fwrite("abc", 1, 3, out);
    if (fseek(out, 10, SEEK_SET) != 0) {
        perror("fseek past end");
        fclose(out);
        return 1;
    }
    fclose(out);
    bad |= check("seek past end, nothing written", ptr, size,
                 "abc\0\0\0\0\0\0\0", 10);
    free(ptr);

    /* Same seek, then one byte at the new position.  */
    out = open_memstream(&ptr, &size);
    if (out == NULL) {
        perror("open_memstream");
        return 1;
    }
    fwrite("abc", 1, 3, out);
    fseek(out, 10, SEEK_SET);
    fputc('X', out);
    fclose(out);
    bad |= check("seek past end, one byte written", ptr, size,
                 "abc\0\0\0\0\0\0\0X", 11);
    free(ptr);

    /* Backwards: the size follows the position down, so the tail of the
       longer write is gone.  A stream reused this way must not report the
       old length and hand back stale bytes.  */
    out = open_memstream(&ptr, &size);
    if (out == NULL) {
        perror("open_memstream");
        return 1;
    }
    fwrite("abcdef", 1, 6, out);
    fseek(out, 2, SEEK_SET);
    fputc('Z', out);
    fclose(out);
    bad |= check("seek back, one byte written", ptr, size, "abZ", 3);
    free(ptr);

    return bad;
}

static int
do_test (void)
{
    FILE *out, *in;
    int v, s;
    size_t size;
    char *ptr;

#if defined __UCLIBC__ && !defined __UCLIBC_HAS_GLIBC_CUSTOM_STREAMS__
    /* fmemopen and open_memstream are that option.  */
    return 23;			/* SKIP */
#endif

    in = fmemopen(text_input, strlen(text_input), "r");
    if (in == NULL) {
        perror("fmemopen");
	return 1;
    }

    out = open_memstream(&ptr, &size);
    if (out == NULL) {
        perror("open_memstream");
	return 1;
    }

    for (;;) {
        s = fscanf(in, "%d", &v);
        if (s <= 0)
            break;

        s = fprintf(out, "%d ", v * v);
        if (s == -1) {
            puts("fprintf failed");
	    return 1;
	}
    }
    fclose(in);
    fclose(out);

    if (size != strlen(good_answer) || strcmp(good_answer, ptr) != 0) {
	printf("failed: size=%zu; ptr=%s\n", size, ptr);
	free(ptr);
	return 1;
    }
    free(ptr);

    return seek_tests();
}

#define TEST_FUNCTION do_test ()
#include "../test-skeleton.c"
