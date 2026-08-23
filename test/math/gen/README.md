# Generating the reference tables

`../exp-ref.h`, `../hypot-ref.h`, `../expm1-ref.h` and `../erf-ref.h` hold correctly rounded
results for a fixed set of inputs, and the budgets in the matching
`../tst-*-ulp.c` come from the same run. The guest cannot compute a reference of its own: uClibc-ng's
`long double` is a wrapper around `double` on most ports, so `expl` is the very
function under test.

Each header carries two tables, one for `double` and one for `float`. The `float`
entry points are not separate implementations — `libm/w_expf.c`,
`libm/w_hypotf.c` and `libm/float_wrappers.c` compute in `double` and convert —
but they need their own inputs, because `expf(x)` is the correctly rounded `exp`
of the `float` x, not of the `double` it was derived from.

These generators run on the **host** and expect the implementations linked in under
distinct names:

| symbol | where it comes from |
|---|---|
| `fd_exp`, `fd_hypot`, `fd_expm1`, `fd_erf`, `fd_erfc` | uClibc-ng `libm/e_exp.c`, `libm/e_hypot.c`, `libm/s_expm1.c`, `libm/s_erf.c` (fdlibm, the SMALL variant) |
| `exp`, `gl_hypot`, `gl_expm1` | `libm/optimized/e_exp.c` (Arm), `libm/optimized/e_hypot.c` and `libm/optimized/s_expm1.c` (glibc) |
| `cr_exp`, `cr_hypot`, `cr_expm1`, `cr_erf`, `cr_erfc` | `libm/accurate/…` (CORE-MATH) — these are the reference |
| `cr_expf`, `cr_hypotf`, `cr_expm1f`, `cr_erff`, `cr_erfcf` | CORE-MATH `src/binary32/…` — the reference for the float tables |
| `arm_erf` | `libm/optimized/s_erf.c` (Arm), which needs `erf_data.c` beside it |

Building them means compiling those sources for the host with the function renamed,
which needs a little scaffolding, because the in-tree files expect the uClibc-ng
build environment:

- `libm/e_*.c` want `math_private.h`; a short header providing `GET_HIGH_WORD`,
  `SET_HIGH_WORD`, `EXTRACT_WORDS`, `INSERT_WORDS`, `u_int32_t` and empty
  `libm_hidden_def`/`attribute_hidden` is enough.
- `libm/optimized/e_exp.c` needs musl's helpers, which sit next to it in
  `musl-support.h`, plus `exp_data.c`.
- `libm/accurate/e_hypot.c` compiles as is; `-DUCLIBC_HYPOT_FORCE_SPLIT` selects the
  path without a 128-bit integer, which is how the 32-bit emulation was checked
  against the `__int128` one.
- CORE-MATH's binary32 files compile as they are.
- The accurate files call fma, so link one in: on a host whose libm has a
  conforming fma that is automatic, elsewhere take libm/s_fma.c.

Build with `-fexcess-precision=standard`, or x87 excess precision will make the
counts wrong on 32-bit x86 hosts.

Run without arguments to see how often each variant misses the reference — those are
the numbers the budgets are set from. Run with any argument to print the tables; the
comment at the top of each header is written by hand and has to be prepended again.

Whoever regenerates a table should say in the header comment what the point set is
and why, the way the current ones do: for `hypot` the double pairs are deliberately
biased towards hard cases, or the variants cannot be told apart at all. The float
tables need no such bias — every variant hits every float point, because computing in
`double` leaves the error far below the `float` rounding boundary.
