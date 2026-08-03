#include <inttypes.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define ARRAY_SIZE(A) (sizeof(A) / sizeof(*(A)))
#define FIELD(B, code) (((code) / BASE_##B) % VARS_##B)

#define BASE_0 1
#define VARS_0 2

#define BASE_1 (BASE_0 * VARS_0)
#define VARS_1 2

#define BASE_2 (BASE_1 * VARS_1)
#define VARS_2 2

#define BASE_3 (BASE_2 * VARS_2)
#define VARS_3 2

#define BASE_4 (BASE_3 * VARS_3)
#define VARS_4 2

#define BASE_5 (BASE_4 * VARS_4)
#define VARS_5 3

#define BASE_6 (BASE_5 * VARS_5)
#define VARS_6 5

#define N_CODES (BASE_6 * VARS_6)


/*
 *   0  1  2  3  4  5      6          7
 * %[#][0][-][ ][+][width][.precision]conversion
 *
 * 0, 1, 2, 3, 4: bit flags
 * 5: absent/narrow/wide
 * 6: absent/'.' is present, precision is absent/zero/small/big
 */
static void make_format(char *fmt, int code, char conv)
{
	*fmt++ = '%';
	if (FIELD(0, code))
		*fmt++ = '#';
	if (FIELD(1, code))
		*fmt++ = '0';
	if (FIELD(2, code))
		*fmt++ = '-';
	if (FIELD(3, code))
		*fmt++ = ' ';
	if (FIELD(4, code))
		*fmt++ = '+';
	switch (FIELD(5, code)) {
	case 0:
		break;
	case 1:
		*fmt++ = '1';
		break;
	case 2:
		*fmt++ = '4';
		*fmt++ = '0';
		break;
	}
	if (FIELD(6, code))
		*fmt++ = '.';
	switch (FIELD(6, code)) {
	case 0:
	case 1:
		break;
	case 2:
		*fmt++ = '0';
		break;
	case 3:
		*fmt++ = '1';
		break;
	case 4:
		*fmt++ = '2';
		*fmt++ = '0';
		break;
	}
	*fmt++ = conv;
	*fmt++ = 0;
}


#define CRC32_POLY 0xedb88320

static inline uint32_t crc32(uint32_t crc, const void *p, size_t sz)
{
	const uint8_t *data = p;

	while (sz --> 0) {
		crc ^= *data++;
		for (int i = 0; i < 8; ++i) {
			if (crc & 1)
				crc = (crc >> 1) ^ CRC32_POLY;
			else
				crc >>= 1;
		}
	}
	return crc;
}

static void test_float(const char *conv, const double v[], int n)
{
	uint32_t crc = 0xffffffff;

	for (int i = 0; conv[i]; ++i) {
		for (int code = 0; code < N_CODES; ++code) {
			char fmt[20];
			char out[256];

			make_format(fmt, code, conv[i]);
			crc = crc32(crc, fmt, strlen(fmt));
			for (int j = 0; j < n; ++j) {
				snprintf(out, sizeof(out), fmt, v[j]);
				crc = crc32(crc, out, strlen(out));
#ifdef DEBUG
				printf("%-20s -> %s\n", fmt, out);
#endif
			}
		}
	}
	printf("CRC = %#010" PRIx32 "\n", crc);
}
