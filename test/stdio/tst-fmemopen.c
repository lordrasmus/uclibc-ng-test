/* fmemopen() and open_memstream(), the latter against every normative
   sentence of its POSIX description.  Where POSIX says implementation-defined
   the test accepts either answer and reports which one it saw, so it stays a
   conformance test rather than a record of one implementation's choices.  */

#include <features.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#if !defined __UCLIBC__ || defined __UCLIBC_HAS_WCHAR__
#include <wchar.h>
#define HAVE_FWIDE 1
#endif

static char *text_input = "1 23 43";

static const char *good_answer = "1 529 1849 ";

static int errors;

static void
fail (const char *what)
{
    printf("FAIL: %s\n", what);
    errors++;
}

static void
dump (const char *label, const char *buf, size_t n)
{
    size_t i;

    printf("      %s", label);
    for (i = 0; i < n; i++)
        printf(" %02x", (unsigned char) buf[i]);
    puts("");
}

/* "the variable pointed to by sizep shall contain the smaller of the current
   buffer length and the number of bytes ... between the beginning of the
   buffer and the current file position" */
static void
check_size (const char *what, size_t got, size_t want)
{
    if (got != want) {
        printf("FAIL: %s: size %zu, POSIX requires %zu\n", what, got, want);
        errors++;
    }
}

/* "a null character ... shall be appended" and "the terminating null is not
   included in the calculation of the buffer length" */
static void
check_term (const char *what, const char *buf, size_t size)
{
    if (buf[size] != 0) {
        printf("FAIL: %s: no null at the reported size %zu, found 0x%02x\n",
               what, size, (unsigned char) buf[size]);
        errors++;
    }
}

static void
check_bytes (const char *what, const char *buf, const char *want, size_t n)
{
    if (memcmp(buf, want, n) != 0) {
        printf("FAIL: %s: contents differ\n", what);
        dump("got  ", buf, n);
        dump("want ", want, n);
        errors++;
    }
}

/* The original round trip: read through fmemopen, write through
   open_memstream.  */
static int
round_trip (void)
{
    FILE *out, *in;
    int v, s;
    size_t size;
    char *ptr;

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
        printf("FAIL: round trip: size=%zu; ptr=%s\n", size, ptr);
        free(ptr);
        return 1;
    }
    free(ptr);
    return 0;
}

/* "The stream shall be opened for writing and shall be seekable", "shall be
   byte-oriented", "The position shall be initially set to zero", "The length
   shall be initially set to zero". */
static void
initial_state (void)
{
    FILE *f;
    char *ptr;
    size_t size;

    f = open_memstream(&ptr, &size);
    if (f == NULL) {
        perror("open_memstream");
        errors++;
        return;
    }
    if (ftell(f) != 0)
        fail("a fresh stream is not positioned at zero");
    if (fseek(f, 0, SEEK_SET) != 0)
        fail("a fresh stream is not seekable");
#ifdef HAVE_FWIDE
    if (fwide(f, 0) >= 0)
        fail("open_memstream is not byte-oriented");
#endif
    if (fflush(f) != 0)
        fail("fflush on a fresh stream failed");
    else {
        check_size("fresh stream", size, 0);
        check_term("fresh stream", ptr, size);
    }
    fclose(f);
    free(ptr);
}

/* "Each write to the stream shall start at the current position and move this
   position by the number of successfully written bytes", plus the size and the
   null after a flush.  */
static void
write_and_flush (void)
{
    FILE *f;
    char *ptr;
    size_t size;

    f = open_memstream(&ptr, &size);
    if (f == NULL) {
        perror("open_memstream");
        errors++;
        return;
    }
    fwrite("abc", 1, 3, f);
    if (ftell(f) != 3)
        fail("the position did not move by the bytes written");
    if (fflush(f) == 0) {
        check_size("after writing 3 bytes", size, 3);
        check_term("after writing 3 bytes", ptr, size);
        check_bytes("after writing 3 bytes", ptr, "abc", 3);
    } else
        fail("fflush after a write failed");
    fclose(f);
    free(ptr);
}

/* "can be used to set the file position beyond the current buffer length.  It
   is implementation-defined whether this extends the buffer to the new length.
   If it extends the buffer, the added buffer contents shall be set to null
   bytes ... if it does not extend the buffer, then if data is later written at
   this point, the buffer contents in the gap shall be set to null bytes". */
static void
seek_past_end (void)
{
    FILE *f;
    char *ptr;
    size_t size;
    int extends;

    f = open_memstream(&ptr, &size);
    if (f == NULL) {
        perror("open_memstream");
        errors++;
        return;
    }
    fwrite("abc", 1, 3, f);
    if (fseek(f, 10, SEEK_SET) != 0) {
        fail("cannot seek beyond the buffer length");
        fclose(f);
        free(ptr);
        return;
    }
    if (ftell(f) != 10)
        fail("the position after seeking beyond the end is wrong");
    if (fflush(f) != 0) {
        fail("fflush after seeking beyond the end failed");
        fclose(f);
        free(ptr);
        return;
    }

    /* Either answer conforms: 10 if the seek extended the buffer, 3 if it did
       not, since the size is the smaller of the length and the position. */
    extends = (size == 10);
    if (!extends && size != 3) {
        printf("FAIL: seek beyond the end: size %zu, POSIX allows 3 or 10\n",
               size);
        errors++;
    } else
        printf("info: a seek beyond the end %s the buffer (size %zu)\n",
               extends ? "extends" : "does not extend", size);
    check_term("seek beyond the end", ptr, size);
    if (extends)
        check_bytes("seek beyond the end", ptr, "abc\0\0\0\0\0\0\0", 10);

    /* Writing at the new position: the gap must read as null bytes either way,
       and the size is then the position in both variants. */
    fputc('X', f);
    if (fflush(f) == 0) {
        check_size("write behind the gap", size, 11);
        check_term("write behind the gap", ptr, size);
        check_bytes("write behind the gap", ptr, "abc\0\0\0\0\0\0\0X", 11);
    } else
        fail("fflush after writing behind the gap failed");
    fclose(f);
    free(ptr);
}

/* The mandated half of the size rule: with the position moved back, the size
   is the position and not the length.  glibc leaves the old byte where the
   null belongs here, so this is the one place the two disagree.  */
static void
seek_back (void)
{
    FILE *f;
    char *ptr;
    size_t size;

    f = open_memstream(&ptr, &size);
    if (f == NULL) {
        perror("open_memstream");
        errors++;
        return;
    }
    fwrite("abcdef", 1, 6, f);
    fseek(f, 2, SEEK_SET);
    if (fflush(f) == 0) {
        check_size("position moved back", size, 2);
        check_term("position moved back", ptr, size);
        check_bytes("position moved back", ptr, "ab", 2);
    } else
        fail("fflush after seeking back failed");

    fputc('Z', f);
    if (fflush(f) == 0) {
        check_size("overwrite after seeking back", size, 3);
        check_term("overwrite after seeking back", ptr, size);
        check_bytes("overwrite after seeking back", ptr, "abZ", 3);
    } else
        fail("fflush after overwriting failed");
    fclose(f);
    free(ptr);
}

/* Back and forward again without writing.  The size is mandated; what the
   bytes in between hold is not specified, so it is only reported -- an
   implementation may keep the old data or may have put the terminator there.  */
static void
seek_back_then_forward (void)
{
    FILE *f;
    char *ptr;
    size_t size;

    f = open_memstream(&ptr, &size);
    if (f == NULL) {
        perror("open_memstream");
        errors++;
        return;
    }
    fwrite("abcdef", 1, 6, f);
    fseek(f, 2, SEEK_SET);
    fseek(f, 5, SEEK_SET);
    if (fflush(f) == 0) {
        check_size("back then forward", size, 5);
        check_term("back then forward", ptr, size);
        check_bytes("back then forward", ptr, "ab", 2);
        printf("info: after moving back over data and forward again, "
               "bytes 2..4 are%s preserved\n",
               memcmp(ptr + 2, "cde", 3) == 0 ? "" : " not");
        dump("bytes 0..4:", ptr, 5);
    } else
        fail("fflush after seeking back and forward failed");
    fclose(f);
    free(ptr);
}

/* "If fseek() ... is called with SEEK_END ... it is implementation-defined
   whether the file position is adjusted relative to the current buffer length
   or relative to the buffer size that would be set by an fflush() call".  */
static void
seek_end (void)
{
    FILE *f;
    char *ptr;
    size_t size;
    long pos;

    f = open_memstream(&ptr, &size);
    if (f == NULL) {
        perror("open_memstream");
        errors++;
        return;
    }
    fwrite("abcdef", 1, 6, f);
    fseek(f, 2, SEEK_SET);
    if (fseek(f, 0, SEEK_END) != 0)
        fail("SEEK_END failed");
    else {
        pos = ftell(f);
        if (pos == 2)
            puts("info: SEEK_END uses the size an fflush() would report");
        else if (pos == 6)
            puts("info: SEEK_END uses the current buffer length");
        else {
            printf("FAIL: SEEK_END landed on %ld, POSIX allows 2 or 6\n", pos);
            errors++;
        }
    }
    fclose(f);
    free(ptr);
}

/* "After a successful fclose(), the pointer referenced by bufp can be passed
   to free()", and the buffer it points at is the one that was written.  */
static void
after_close (void)
{
    FILE *f;
    char *ptr;
    size_t size;

    f = open_memstream(&ptr, &size);
    if (f == NULL) {
        perror("open_memstream");
        errors++;
        return;
    }
    fwrite("abcdef", 1, 6, f);
    if (fclose(f) != 0)
        fail("fclose failed");
    check_size("after fclose", size, 6);
    check_term("after fclose", ptr, size);
    check_bytes("after fclose", ptr, "abcdef", 6);
    free(ptr);
}

static int
do_test (void)
{
#if defined __UCLIBC__ && !defined __UCLIBC_HAS_GLIBC_CUSTOM_STREAMS__
    /* fmemopen and open_memstream are that option. */
    return 23;			/* SKIP */
#endif

    if (round_trip() != 0)
        return 1;

    initial_state();
    write_and_flush();
    seek_past_end();
    seek_back();
    seek_back_then_forward();
    seek_end();
    after_close();

    if (errors > 0)
        printf("%d POSIX requirement%s not met\n",
               errors, errors == 1 ? "" : "s");
    return errors != 0;
}

#define TEST_FUNCTION do_test ()
#include "../test-skeleton.c"
