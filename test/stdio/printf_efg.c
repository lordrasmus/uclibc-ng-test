#include "printf_fp.c"

int main()
{
	const double v[] = {
		-NAN,
		-INFINITY,
		-0x1.1p29,
		-2.75,
		-1,
		-7 / 64.,
		0,
		7 / 64.,
		1,
		0x1.f8p0,
		2.75,
		0x1.1p29,
		INFINITY,
		NAN,
	};

	test_float("eEfFgG", v, ARRAY_SIZE(v));
	return 0;
}
