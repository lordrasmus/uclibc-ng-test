#include "printf_fp.c"

int main()
{
	const double v[] = {
		-NAN,
		-INFINITY,
		-0x1.1p40,
		-2.75,
		-1,
		-9 / 1024.,
		0,
		9 / 1024.,
		1,
		0x1.23456789abcdep0,
		0x1.f8p0,
		2.75,
		0x1.1p40,
		INFINITY,
		NAN,
	};

	test_float("aA", v, ARRAY_SIZE(v));
	return 0;
}
