#ifndef _FAULT_H_
#define _FAULT_H_

#ifdef __cplusplus
    #define DMQ_NORETURN [[noreturn]]
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
    #define DMQ_NORETURN _Noreturn
#elif defined(__GNUC__) || defined(__clang__)
    #define DMQ_NORETURN __attribute__((noreturn))
#elif defined(_MSC_VER)
    #define DMQ_NORETURN __declspec(noreturn)
#else
    #define DMQ_NORETURN
#endif

#ifdef __cplusplus
extern "C" {
#endif

// Hide Windows C_ASSERT redefinition warning
#pragma warning(disable:4005)

// Used for compile-time checking for array sizes. On Windows VC++, you get 
// an "error C2118: negative subscript" error.
#ifndef C_ASSERT
//#define C_ASSERT(expr)  {char uname[(expr)?1:-1];uname[0]=0;}		// Original macro
#define C_ASSERT(expr)  ((void)sizeof(char[(expr) ? 1 : -1]))		// New macro to fix GCC warning
#endif

#define ASSERT() \
	FaultHandler(__FILE__, (unsigned short) __LINE__)

#define ASSERT_TRUE(condition) \
	do {if (!(condition)) FaultHandler(__FILE__, (unsigned short) __LINE__);} while (0)

	/// Handles all software assertions in the system.
	/// @param[in] file - the file name that the software assertion occurred on
	/// @param[in] line - the line number that the software assertion occurred on
	DMQ_NORETURN void FaultHandler(const char* file, unsigned short line);

#ifdef __cplusplus
}
#endif

#endif 
