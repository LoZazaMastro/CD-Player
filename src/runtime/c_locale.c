// CD Player - Process-local C numeric locale for the JSON and archive runtimes.
// Copyright (C) 2026 Andrea.
// SPDX-License-Identifier: GPL-3.0-or-later
#include <locale.h>
#include <limits.h>

/* The freestanding entry point does not run the desktop locale bootstrap.
 * Supply the complete C locale in this app rather than requiring that state
 * from the console libc before settings or metadata are parsed. */
struct lconv *localeconv(void)
{
    static char decimal[] = ".";
    static char empty[] = "";
    static struct lconv locale = {
        .decimal_point = decimal, .thousands_sep = empty, .grouping = empty,
        .int_curr_symbol = empty, .currency_symbol = empty,
        .mon_decimal_point = empty, .mon_thousands_sep = empty, .mon_grouping = empty,
        .positive_sign = empty, .negative_sign = empty,
        .int_frac_digits = CHAR_MAX, .frac_digits = CHAR_MAX,
        .p_cs_precedes = CHAR_MAX, .p_sep_by_space = CHAR_MAX,
        .n_cs_precedes = CHAR_MAX, .n_sep_by_space = CHAR_MAX,
        .p_sign_posn = CHAR_MAX, .n_sign_posn = CHAR_MAX,
        .int_p_cs_precedes = CHAR_MAX, .int_n_cs_precedes = CHAR_MAX,
        .int_p_sep_by_space = CHAR_MAX, .int_n_sep_by_space = CHAR_MAX,
        .int_p_sign_posn = CHAR_MAX, .int_n_sign_posn = CHAR_MAX
    };
    return &locale;
}
