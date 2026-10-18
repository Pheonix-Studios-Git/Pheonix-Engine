#pragma once

#include <pstdlib.h>
#include <pio.h>

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
typedef size_t usize_t;
typedef uptr_t uintptr_t;
typedef ptr_t intptr_t;

extern FILE* stdin;
extern FILE* stdout;
extern FILE* stderr;

#ifdef __cplusplus
extern "C" {
#endif

void __plib_init_standard_streams(void);

void* malloc(size_t size);
void* calloc(size_t nmemb, size_t size);
void* realloc(void* ptr, size_t size);
void free(void* ptr);

void* memset(void* dest, int value, size_t size);
void* memcpy(void* dest, const void* src, size_t size);
void* memmove(void* dest, const void* src, size_t size);
int memcmp(const void* a, const void* b, size_t size);

char* strcpy(char* dest, const char* src);

FILE* fopen(const char* path, const char* mode);
int fclose(FILE* file);
size_t fread(void* buffer, size_t size, size_t count, FILE* file);
size_t fwrite(const void* buffer, size_t size, size_t count, FILE* file);
int fseek(FILE* file, long offset, int origin);
void rewind(FILE* file);
long ftell(FILE* file);
int feof(FILE* file);
int ferror(FILE* file);

int fgetc(FILE* file);
char* fgets(char* str, int count, FILE* stream);

int setenv(const char* name, const char* value, int overwrite);

#ifdef __cplusplus
}
#endif