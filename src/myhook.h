#ifndef MY_ALLOC_H
#define MY_ALLOC_H

#include <stdarg.h>


static void __attribute__((constructor)) init(void);
void *malloc(size_t size);
void free(void* ptr);
void *calloc(size_t nmemb, size_t size);

void *m_calloc(size_t nmemb, size_t size);
void my_puts(char *msg);
size_t my_strlen(char *buf);
void write_file(int type, size_t nmemb, size_t size, void* ptr);
int ssprintf(char* buf, char* fmt, ...);
int vtsprintf(char* buff,char* fmt,va_list arg);
void u64tohex(uint64_t tb, char buf[17]);

//const char* log_name = "trace.log";
char log_name[0x100];
const char* log_dir = "/tmp/trace_heap/";
// static int fd;

const char* start_msg = "start hooking\n";
const char* fail_msg = "failure somethig\n";
const char* called_malloc = "called malloc\n";
const char* called_free = "called free\n";
const char* called_calloc = "called calloc\n";
const char* msg_size = "size: ";
const char* msg_ptr = "ptr: ";
const char* msg_nmemb = "nmemb: ";

static void* (*real_malloc)(size_t);
static void (*real_free)(void*);
static void* (*real_calloc)(size_t, size_t);
static __thread int no_hook;

enum flag {MALLOC, FREE, CALLOC};

#endif
