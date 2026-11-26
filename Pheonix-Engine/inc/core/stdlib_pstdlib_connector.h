#pragma once

#ifdef INT_MAX
	#undef INT_MAX
#endif
#ifdef INT_MIN
	#undef INT_MIN
#endif
#ifdef UINT_MAX
	#undef UINT_MAX
#endif

#include <stddef.h> // for size_t
#include <pio.h>
#include <pstdlib.h>

struct _IO_FILE;
typedef struct _IO_FILE FILE;

typedef i64 int64_t;
typedef i32 int32_t;
typedef i16 int16_t;
typedef i8 int8_t;
typedef u64 uint64_t;
typedef u32 uint32_t;
typedef u16 uint16_t;
typedef u8 uint8_t;
typedef uptr_t uintptr_t;
typedef ptr_t intptr_t;

extern FILE* stdin;
extern FILE* stdout;
extern FILE* stderr;

#ifdef __cplusplus
extern "C" {
#endif

void __stdlib_plib_connector_init(int argc, char** argv);

void* malloc(size_t size);
void* calloc(size_t nmemb, size_t size);
void* realloc(void* ptr, size_t size);
void free(void* ptr);

void* memset(void* dest, int value, size_t size);
void* memcpy(void* dest, const void* src, size_t size);
void* memmove(void* dest, const void* src, size_t size);
int memcmp(const void* a, const void* b, size_t size);

char* strrchr(const char* str, int ch);
char* strcpy(char* dest, const char* src);
char* strncpy(char* dest, const char* src, size_t n);
double strtod(const char* str, char** endptr);
float strtof(const char* str, char** endptr);
long strtol(const char* restrict nptr, char** restrict endptr, int base);

unsigned long strtoul(const char* restrict nptr, char** restrict endptr, int base);
long long strtoll(const char* restrict nptr, char** restrict endptr, int base);
unsigned long long strtoull(const char* restrict nptr, char** restrict endptr, int base);
char* strcat(char* restrict dest, const char* restrict src);
int tolower(int ch);

FILE* fopen(const char* path, const char* mode);
int fclose(FILE* file);
size_t fread(void* buffer, size_t size, size_t count, FILE* file);
size_t fwrite(const void* buffer, size_t size, size_t count, FILE* file);
int fseek(FILE* file, long offset, int origin);
void rewind(FILE* file);
long ftell(FILE* file);
int feof(FILE* file);
int ferror(FILE* file);
int fflush(FILE* file);

int getc(FILE* file);
int fgetc(FILE* file);
int ungetc(int c, FILE* file);
char* fgets(char* str, int count, FILE* file);
int setenv(const char* name, const char* value, int overwrite);
int abs(int x);

#ifdef __cplusplus
}
#endif

#ifdef USE_DETOUR
	#include <detour.h>

	extern const DTLinker *detour_api;

	void* dlopen(const char* filename, int flags);
	void* dlsym(void* handle, const char* symbol);
	int dlclose(void* handle);
	char* dlerror(void);
#endif