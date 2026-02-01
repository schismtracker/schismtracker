#ifndef DATE
# define DATE(ver, y, m, d, schismstr, ctimestampstr, cdatestr)
#endif

/* epoch */
DATE(0x0000, 2009, 10, 31, "20091031", "Sat Oct 31 00:00:00 2009", "Oct 31 2009")
/* basic values */
DATE(0x1730, 2026, 1, 31, "20260131", "Sat Jan 31 00:00:00 2026", "Jan 31 2026")
DATE(0x1472, 2024, 2, 29, "20240229", "Thu Feb 29 00:00:00 2024", "Feb 29 2024")
/* edge case: multiple of 100 is not a leap year */
DATE(0x80E0, 2100, 2, 28, "21000228", "Sun Feb 28 00:00:00 2100", "Feb 28 2100")
DATE(0x80E1, 2100, 3, 1,  "21000301", "Mon Mar  1 00:00:00 2100", "Mar  1 2100")
/* edge case: multiple of 400 is a leap year */
DATE(0x22CE4, 2400, 2, 28, "24000228", "Mon Feb 28 00:00:00 2400", "Feb 28 2400")
DATE(0x22CE5, 2400, 2, 29, "24000229", "Tue Feb 29 00:00:00 2400", "Feb 29 2400")
DATE(0x22CE6, 2400, 3, 1,  "24000301", "Wed Mar  1 00:00:00 2400", "Mar  1 2400")

#undef DATE
