# Generating the reference tables

`../exp-ref.h` and `../hypot-ref.h` hold correctly rounded results for a fixed set
of inputs, and the budgets in `../tst-exp-ulp.c` and `../tst-hypot-ulp.c` come from
the same run. The guest cannot compute a reference of its own: uClibc-ng's
`long double` is a wrapper around `double` on most ports, so `expl` is the very
function under test.

These generators run on the **host** and expect the three implementations linked in
under distinct names:

| symbol | where it comes from |
|---|---|
| `fd_exp`, `fd_hypot` | uClibc-ng `libm/e_exp.c`, `libm/e_hypot.c` (fdlibm, the SMALL variant) |
| `exp`, `gl_hypot` | `libm/optimized/e_exp.c` (Arm), `libm/optimized/e_hypot.c` (glibc) |
| `cr_exp`, `cr_hypot` | `libm/accurate/…` (CORE-MATH) — this one is the reference |

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

Run without arguments to see how often each variant misses the reference — those are
the numbers the budgets are set from. Run with any argument to print the table.

Whoever regenerates a table should say in the header comment what the point set is
and why, the way the current ones do: for `hypot` the pairs are deliberately biased
towards hard cases, or the variants cannot be told apart at all.
