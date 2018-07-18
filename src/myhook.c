#define _GNU_SOURCE

#include <stdio.h>
#include <dlfcn.h>
#include <sys/mman.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdint.h>
#include "myhook.h"


static void *tmp;
static size_t mapping_size;
char buf_exe[0x100];

static int g_fd = 0;

void num_dump(unsigned long tb);
static void* (*real_malloc)(size_t);
static void (*real_free)(void*);
static void* (*real_calloc)(size_t, size_t);
static __thread int no_hook;


static void __attribute__((constructor)) init(void) {
    my_puts("start");
    real_calloc= dlsym (RTLD_NEXT, "calloc");
    real_malloc = dlsym (RTLD_NEXT, "malloc");
    real_free = dlsym (RTLD_NEXT, "free");
    
    if (!real_calloc || !real_malloc || !real_free) {
        write(1, fail_msg, strlen(fail_msg));
        _exit(1);
    }

}


void my_puts(char *msg) {
    size_t length;
    length = my_strlen(msg);
    write(1, msg, length); 
}

size_t my_strlen(char* buf) {
    size_t i = 0;
    while(buf[i] != '\0') {
       i++;
    }
    return i; 
}

void write_file(int type, size_t nmemb, size_t size, void* ptr) {
    char buf[0x10];
    ssize_t len;
    if (g_fd == 0) {
        len = readlink("/proc/self/exe", buf_exe, sizeof(buf_exe)-1);
        if (len < 0) {
            _exit(2);
        }
        buf_exe[len] = '\0';
        memcpy(log_name, log_dir, strlen(log_dir));
        for (int i = len-1; i != 0 ; i--) {
            if (buf_exe[i] == '/') {
                memcpy(log_name+strlen(log_dir), buf_exe+i+1, len-i); 
                my_puts(buf_exe+i+1);
                break;
            } 
        }

        g_fd = open(log_name, O_RDWR|O_APPEND|O_CREAT);
        if (g_fd < 0) {
            _exit(3);
        }
    }
    
    switch (type) {
        case MALLOC:
            write(g_fd, called_malloc, strlen(called_malloc));
            write(g_fd, msg_size, strlen(msg_size));
            u64tohex(size, buf);          
            write(g_fd, buf, 0x10);
            write(g_fd, "\n", 1);
            u64tohex((uint64_t)ptr, buf);          
            write(g_fd, msg_ptr, strlen(msg_ptr));
            write(g_fd, buf, 0x10);
            write(g_fd, "\n", 1);
            break;
        
        case CALLOC:
            write(g_fd, called_calloc, strlen(called_calloc));
            write(g_fd, msg_nmemb, strlen(msg_nmemb));
            u64tohex(nmemb, buf);
            write(g_fd, buf, 0x10);
            write(g_fd, "\n", 1);
            write(g_fd, msg_size, strlen(msg_size));
            u64tohex(size, buf);          
            write(g_fd, buf, 0x10);
            write(g_fd, "\n", 1);
            u64tohex((uint64_t)ptr, buf);          
            write(g_fd, msg_ptr, strlen(msg_ptr));
            write(g_fd, buf, 0x10);
            write(g_fd, "\n", 1);
            break;

        case FREE:
            write(g_fd, called_free, strlen(called_free));
            u64tohex((uint64_t)ptr, buf);          
            write(g_fd, msg_ptr, strlen(msg_ptr));
            write(g_fd, buf, 0x10);
            write(g_fd, "\n", 1);
            break;            
    };
    
}


void *malloc(size_t size) {
    if (real_malloc == NULL) {
        real_malloc = dlsym (RTLD_NEXT, "malloc");
        if (real_malloc == NULL) {
            write(1, fail_msg, strlen(fail_msg));
            _exit(1);
        }
    }
    void* ptr;
    if (no_hook) {
        return real_malloc(size);
    }
    
    no_hook = 1;
    ptr = real_malloc(size);
    write_file(MALLOC, 0xdeadbeef, size, ptr);
    no_hook = 0;
    return ptr;
}

void free(void* ptr) {
    // write(1, "free incoming\n", strlen("free incoming\n"));
    if (real_free == NULL) {
        real_free = dlsym(RTLD_NEXT, "free");
    }
    if (ptr == tmp) {
        munmap(ptr, mapping_size);
    } else {
        real_free(ptr);
    }
    write_file(FREE, 0xdeadbeef, 0xfeedbabe, ptr);
    return;
}

void *m_calloc(size_t nmemb, size_t size) {
    mapping_size = (nmemb * size) + 0x1000;
    if (!tmp) {
        if ((tmp = mmap(0, mapping_size, PROT_READ|PROT_WRITE, MAP_ANONYMOUS|MAP_PRIVATE, -1, 0 ))== MAP_FAILED) { 
            write(1, fail_msg, strlen(fail_msg));
            _exit(1);
        }
    }
    return tmp;
}

void *calloc(size_t nmemb, size_t size) {
    void *ptr;

    if (!real_calloc) {
        real_calloc = dlsym(RTLD_NEXT, "calloc");
        return m_calloc(nmemb, size);
    }
    ptr = real_calloc(nmemb, size);
    write_file(CALLOC, nmemb, size, ptr); 
    return ptr;
}

void u64tohex(uint64_t tb, char buf[17]) {
  const char sym[] = "0123456789abcdef";
  const size_t n = 16; 
  size_t i;

  for (i=1; i <= n; i++) {
    char c = sym[tb % 0x10];
    buf[n-i] = c;
    tb >>= 4;
  }
  buf[n] = '\0';
}

void num_dump(uint64_t tb) { // for dump register/int/long
  char buf[17];
  u64tohex(tb, buf);
  buf[sizeof(buf)-1] = '\n';
  write(1, buf, sizeof(buf));
}
