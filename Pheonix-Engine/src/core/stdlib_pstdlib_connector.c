#include <core/stdlib_pstdlib_connector.h>
#include <stdio.h>

typedef struct __PLibFileNode {
    FILE* token;
    PIO_Stream* stream;
    struct __PLibFileNode* next;
} __PLibFileNode;

FILE* stdin = NULL;
FILE* stdout = NULL;
FILE* stderr = NULL;

static __PLibFileNode* __plib_files;

static __PLibFileNode* __plib_find_file(FILE* file) {
    for (__PLibFileNode* node = __plib_files; node; node = node->next) {
        if (node->token == file) return node;
    }
    return NULL;
}

static PIO_Stream* __plib_file_to_pstream(FILE* file) {
    __PLibFileNode* node = __plib_find_file(file);
    return node ? node->stream : NULL;
}

static FILE* __plib_pstream_to_file(PIO_Stream* stream) {
    if (!stream) return NULL;

    __PLibFileNode* node = alloc(sizeof(*node));
    if (!node) return NULL;

    node->token = (FILE*)(void*)node;
    node->stream = stream;
    node->next = __plib_files;
    __plib_files = node;

    return node->token;
}

static bool __plib_remove_file(FILE* file, PIO_Stream** stream) {
    __PLibFileNode** link = &__plib_files;

    while (*link) {
        __PLibFileNode* node = *link;

        if (node->token == file) {
            *link = node->next;
            if (stream) *stream = node->stream;

            dealloc(node);
            return true;
        }

        link = &node->next;
    }

    return false;
}

void __plib_init_standard_streams(void) {
    stdin = __plib_pstream_to_file(s_stdin);
    stdout = __plib_pstream_to_file(s_stdout);
    stderr = __plib_pstream_to_file(s_stderr);
}

void* malloc(usize_t size) {
    return alloc((usize_t)size);
}

void* calloc(usize_t nmemb, usize_t size) {
    if (size != 0 && nmemb > UINT16_MAX / size) return NULL;
    return zalloc((usize_t)(nmemb * size));
}

void* realloc(void* ptr, usize_t size) {
    return ralloc(ptr, (usize_t)size);
}

void free(void* ptr) {
    if (ptr) (void)dealloc(ptr);
}

void* memset(void* dest, int value, usize_t size) {
    if (!dest && size != 0) return NULL;

    if (!fillbuf(dest, (byte_t)(unsigned char)value, (usize_t)size)) return NULL;
    return dest;
}

void* memcpy(void* dest, const void* src, usize_t size) {
    if ((!dest || !src) && size != 0) return NULL;

    if (!copybuf(dest, src, (usize_t)size)) return NULL;
    return dest;
}

void* memmove(void* dest, const void* src, usize_t size) {
    if ((!dest || !src) && size != 0) return NULL;

    if (!movebuf(dest, src, (usize_t)size)) return NULL;
    return dest;
}

int memcmp(const void* a, const void* b, usize_t size) {
    return cmpbuf(a, b, (usize_t)size);
}

char* strcpy(char* dest, const char* src) {
	if (!dest || !src) return NULL;

    if (!strcopy(dest, src)) return NULL;
    return dest;
}

static bool __plib_convert_mode(const char* mode, PStreamFlags* flags) {
    if (!mode || !flags || mode[0] == '\0') return false;

    PStreamFlags result = 0;
    switch (mode[0]) {
        case 'r': result |= PSTREAM_FLAG_READ; break;
        case 'w': result |= PSTREAM_FLAG_WRITE; break;
        case 'a': result |= PSTREAM_FLAG_WRITE | PSTREAM_FLAG_APPEND; break;
        
		default: return false;
    }

    bool plus = false;
    bool binary = false;
    for (const char* p = mode + 1; *p != '\0'; p++) {
        switch (*p) {
            case '+': {
                if (plus) return false;
                plus = true;
                result |= PSTREAM_FLAG_READ | PSTREAM_FLAG_WRITE;
                break;
			}

            case 'b': {
                if (binary) return false;
                binary = true;
                result |= PSTREAM_FLAG_BINARY;
                break;
			}

            default: return false;
        }
    }

    *flags = result;
    return true;
}

FILE* fopen(const char* path, const char* mode) {
    if (!path || !mode) return NULL;

    PStreamFlags flags;
    if (!__plib_convert_mode(mode, &flags)) return NULL;

    PIO_Stream* stream = sopen_file(path, flags);
    if (!stream) return NULL;

    FILE* file = __plib_pstream_to_file(stream);
    if (!file) {
        (void)sclose(stream);
        return NULL;
    }

    return file;
}

int fclose(FILE* file) {
    PIO_Stream* stream = NULL;

    if (!__plib_remove_file(file, &stream)) return EOF;
    return sclose(stream) ? 0 : EOF;
}

usize_t fread(void* buffer, usize_t size, usize_t count, FILE* file) {
    if (size == 0 || count == 0) return 0;
    if (!buffer || count > USIZE_MAX / size) return 0;

    PIO_Stream* stream = __plib_file_to_pstream(file);
    if (!stream) return 0;

    usize_t requested = size * count;
    usize_t bytes = (usize_t)sread(stream, buffer, (usize_t)requested);
    if (bytes > requested) bytes = requested;

    return bytes / size;
}

usize_t fwrite(const void* buffer, usize_t size, usize_t count, FILE* file) {
    if (size == 0 || count == 0) return 0;
    if (!buffer || count > USIZE_MAX / size) return 0;

    PIO_Stream* stream = __plib_file_to_pstream(file);
    if (!stream) return 0;

    usize_t requested = size * count;
    usize_t bytes = (usize_t)swrite(stream, buffer, (usize_t)requested);

    if (bytes > requested) bytes = requested;
    return bytes / size;
}

int fseek(FILE* file, long offset, int origin) {
    PIO_Stream* stream = __plib_file_to_pstream(file);
    if (!stream) return -1;

    return sseek(stream, offset, origin) ? 0 : -1;
}

void rewind(FILE* file) {
    PIO_Stream* stream = __plib_file_to_pstream(file);

    if (stream) (void)sseek(stream, 0, SEEK_SET);
}

long ftell(FILE* file) {
    PIO_Stream* stream = __plib_file_to_pstream(file);
    if (!stream) return -1L;

    ulen_t pos = stell(stream);
    if (pos > (ulen_t)INT64_MAX) return -1L;
    return (long)pos;
}

int feof(FILE* file) {
    PIO_Stream* stream = __plib_file_to_pstream(file);
    if (!stream) return 0;

    return seof(stream) ? 1 : 0;
}

int ferror(FILE* file) {
    PIO_Stream* stream = __plib_file_to_pstream(file);
    if (!stream) return 1;

    return slast_err(stream) != PIO_ERR_NONE;
}

int fgetc(FILE* file) {
    PIO_Stream* stream = __plib_file_to_pstream(file);
    if (stream == NULL) return EOF;

    unsigned char ch;
    usize_t bytes = sread(stream, &ch, 1);
    if (bytes != 1) return EOF;

    return (int)ch;
}

char* fgets(char* str, int count, FILE* file) {
    if (!str || count <= 0) return NULL;
    if (count == 1) {
        str[0] = '\0';
        return str;
    }

    PIO_Stream* stream = __plib_file_to_pstream(file);
    if (!stream) return NULL;

    int i = 0;
    while (i < count - 1) {
        unsigned char ch;
        usize_t n = sread(stream, &ch, 1);

        if (n != 1) {
            if (i == 0) return NULL;
            break;
        }

        str[i++] = (char)ch;

        if (ch == '\n') break;
    }

    str[i] = '\0';
    return str;
}

int setenv(const char* name, const char* value, int overwrite) {
    return assignenv(name, value, overwrite != 0) ? 0 : -1;
}