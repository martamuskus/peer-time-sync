#ifndef MIM_ERR_H
#define MIM_ERR_H

// Print information about a system error and quits.
[[noreturn]] void syserr(const char* fmt, ...);

// Print information about an error and quits.
[[noreturn]] void fatal(const char* fmt, ...);

#endif
