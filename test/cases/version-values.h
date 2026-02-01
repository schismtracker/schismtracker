#ifndef DATE
# define DATE(ver, y, m, d)
#endif
DATE(0x0000, 2009, 10, 31)
/* basic values */
DATE(0x1730, 2026, 1, 31)
DATE(0x1472, 2024, 2, 29)
/* edge case: multiple of 100 is not a leap year */
DATE(0x80E0, 2100, 2, 28)
DATE(0x80E1, 2100, 3, 1)
/* edge case: multiple of 400 is a leap year */
DATE(0x22CE4, 2400, 2, 28)
DATE(0x22CE5, 2400, 2, 29)
DATE(0x22CE6, 2400, 3, 1)
#undef DATE