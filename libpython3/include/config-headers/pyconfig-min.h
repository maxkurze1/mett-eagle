#ifndef Py_PYCONFIG_H
#define Py_PYCONFIG_H

/* The size of `double', as computed by sizeof. */
#define SIZEOF_DOUBLE 8

/* The size of `float', as computed by sizeof. */
#define SIZEOF_FLOAT 4

/* The size of `fpos_t', as computed by sizeof. */
#define SIZEOF_FPOS_T 16

/* The size of `int', as computed by sizeof. */
#define SIZEOF_INT 4

/* The size of `long', as computed by sizeof. */
#define SIZEOF_LONG 8

/* The size of `long double', as computed by sizeof. */
#define SIZEOF_LONG_DOUBLE 16

/* The size of `long long', as computed by sizeof. */
#define SIZEOF_LONG_LONG 8

/* The number of bytes in an off_t. */
#define SIZEOF_OFF_T SIZEOF_LONG

/* The size of `pid_t', as computed by sizeof. */
#define SIZEOF_PID_T SIZEOF_INT

/* The number of bytes in a pthread_t. */
#define SIZEOF_PTHREAD_T 8

/* The size of `short', as computed by sizeof. */
#define SIZEOF_SHORT 2

/* The size of `size_t', as computed by sizeof. */
#define SIZEOF_SIZE_T 8

/* The number of bytes in a time_t. */
#define SIZEOF_TIME_T SIZEOF_LONG

/* The size of `uintptr_t', as computed by sizeof. */
#define SIZEOF_UINTPTR_T 8

/* The size of `void *', as computed by sizeof. */
#define SIZEOF_VOID_P 8

/* The size of `wchar_t', as computed by sizeof. */
#define SIZEOF_WCHAR_T 4

/* The size of `_Bool', as computed by sizeof. */
#define SIZEOF__BOOL 1

/* #undef _POSIX_THREADS */

#define HAVE_PTHREAD_H 1
#define HAVE_STDARG_PROTOTYPES 1
#define ALIGNOF_SIZE_T 8
#define ALIGNOF_LONG 8

#endif