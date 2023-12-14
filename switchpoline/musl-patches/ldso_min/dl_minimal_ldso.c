#define _BSD_SOURCE
#include <stddef.h>
#include <fcntl.h>
#include <sys/mman.h>
#include "dynlink.h"
#include "libc.h"
#include "../src/internal/syscall.h"
#include "../src/internal/pthread_impl.h"

#define mmap __mmap
#define mprotect __mprotect

#ifndef START
#define START "_dlstart"
#endif

#define SHARED

#define ASLR_ENABLED 1

#include "crt_arch.h"

static unsigned char *min_library_address;

#ifndef GETFUNCSYM
#define GETFUNCSYM(fp, sym, got) do { \
	hidden void sym(); \
	static void (*static_func_ptr)() = sym; \
	__asm__ __volatile__ ( "" : "+m"(static_func_ptr) : : "memory"); \
	*(fp) = static_func_ptr; } while(0)
#endif





static inline void test_write(const char *s) {
    size_t l = strlen(s);
    __syscall3(SYS_write, 2, s, l);
    __syscall3(SYS_write, 2, "\n", 1);
}

static char test_write_buffer[256];
static inline void test_write_hex(const void *s, int len) {
    for (int i = 0; i < len; i++) {
        test_write_buffer[i*2] = "0123456789abcdef"[((unsigned char*)s)[i]>>4];
        test_write_buffer[i*2+1] = "0123456789abcdef"[((unsigned char*)s)[i]&0xf];
    }
    test_write_buffer[len*2] = '\n';
    __syscall3(SYS_write, 2, test_write_buffer, len*2 + 1);
}
static inline void test_write_size(size_t s) {
    for (int i = 0; i < 8; i++) {
        test_write_buffer[i*2+2] = "0123456789abcdef"[((s >> ((7-i)*8))&0xff)>>4];
        test_write_buffer[i*2+3] = "0123456789abcdef"[(s >> ((7-i)*8))&0xf];
    }
    test_write_buffer[0] = '0';
    test_write_buffer[1] = 'x';
    test_write_buffer[18] = '\n';
    __syscall3(SYS_write, 2, test_write_buffer, 19);
}

static inline void memcpy(void *dst, void *src, size_t len) {
  char *d = dst;
  char *s = src;
  while (len > 0) {
    *(d++) = *(s++);
    len--;
  }
}



#if defined(__aarch64__) || defined(_M_ARM64)
// This construct avoids the indirect call when transferring control to libc.
// Libc is out of range for direct branches.
// We trigger a signal and change the signal context in its handler.
// The kernel will restore the pc (and other registers) when returning from the signal.
#include <signal.h>
#include <ucontext.h>
#include "ksigaction.h"

hidden void __restore();
hidden void __restore_rt();

int __libc_sigaction_slim(int sig, const struct sigaction *restrict sa, struct sigaction *restrict old) {
	struct k_sigaction ksa, ksa_old;
	if (sa) {
		ksa.handler = sa->sa_handler;
		ksa.flags = sa->sa_flags | SA_RESTORER;
		ksa.restorer = (sa->sa_flags & SA_SIGINFO) ? __restore_rt : __restore;
		memcpy(&ksa.mask, &sa->sa_mask, _NSIG/8);
	}
	int r = __syscall(SYS_rt_sigaction, sig, sa?&ksa:0, old?&ksa_old:0, _NSIG/8);
	if (old && !r) {
		old->sa_handler = ksa_old.handler;
		old->sa_flags = ksa_old.flags;
		memcpy(&old->sa_mask, &ksa_old.mask, _NSIG/8);
	}
}

static size_t call_function_ptr_data[3];
// this function is invoked as signal handler
static void __call_function_ptr_handler(int signal, siginfo_t *info, void *ctx) {
	ucontext_t *uctx = (ucontext_t*) ctx;
	uctx->uc_mcontext.pc = call_function_ptr_data[0];
	uctx->uc_mcontext.regs[0] = call_function_ptr_data[1];
	uctx->uc_mcontext.regs[1] = call_function_ptr_data[2];
}
// register & invoke the signal handler
static inline void call_function_ptr(void (*ptr)(size_t *, size_t *), size_t *arg1, size_t *arg2) {
  call_function_ptr_data[0] = (size_t) ptr;
  call_function_ptr_data[1] = (size_t) arg1;
  call_function_ptr_data[2] = (size_t) arg2;
  struct sigaction sa;
  for (int i = 0; i < sizeof(struct sigaction); i++)
    ((char*) &sa)[i] = 0;
  sa.sa_sigaction = __call_function_ptr_handler;
  sa.sa_flags = SA_SIGINFO | SA_RESETHAND;
  __libc_sigaction_slim(SIGUSR1, &sa, 0);
  int pid = __syscall(SYS_getpid);
  __syscall2(SYS_kill, pid, SIGUSR1);
}
#else
static inline void call_function_ptr(void (*ptr)(size_t *, size_t *), size_t *arg1, size_t *arg2) {
  ptr(arg1, arg2);
}
#endif




static void *mmap_fixed(void *p, size_t n, int prot, int flags, int fd, off_t off)
{
    static int no_map_fixed;
    char *q;
    if (!n) return p;
    if (!no_map_fixed) {
        q = mmap(p, n, prot, flags|MAP_FIXED, fd, off);
        if (!DL_NOMMU_SUPPORT || q != MAP_FAILED || errno != EINVAL)
            return q;
        no_map_fixed = 1;
    }
    /* Fallbacks for MAP_FIXED failure on NOMMU kernels. */
    if (flags & MAP_ANONYMOUS) {
        memset(p, 0, n);
        return p;
    }
    ssize_t r;
    if (__syscall3(SYS_lseek, fd, off, SEEK_SET) < 0) return MAP_FAILED;
    for (q=p; n; q+=r, off+=r, n-=r) {
        r = __syscall3(SYS_read, fd, q, n);
        if (r < 0 && errno != EINTR) return MAP_FAILED;
        if (!r) {
            memset(q, 0, n);
            break;
        }
    }
    return p;
}

struct MapInfos {
    void *map;
    size_t base;
    size_t dynv;
};

static inline struct MapInfos map_library(int fd) {
    Ehdr buf[(896+sizeof(Ehdr))/sizeof(Ehdr)];
    void *allocated_buf=0;
    size_t phsize;
    size_t addr_min=SIZE_MAX, addr_max=0, map_len;
    size_t this_min, this_max;
    size_t nsegs = 0;
    off_t off_start;
    Ehdr *eh;
    Phdr *ph, *ph0;
    unsigned prot;
    unsigned char *map=MAP_FAILED, *base;
    size_t dyn=0;
    size_t tls_image=0;
    size_t i;

    ssize_t l = __syscall3(SYS_read, fd, buf, sizeof buf);
    eh = buf;
    if (l<0) return (struct MapInfos){-11, 0, 0};
    if (l<sizeof *eh || (eh->e_type != ET_DYN && eh->e_type != ET_EXEC))
        return (struct MapInfos){-1, 0, 0};
    phsize = eh->e_phentsize * eh->e_phnum;

    if (phsize > sizeof buf - sizeof *eh) {
        return (struct MapInfos){-2, 0, 0};
    } else if (eh->e_phoff + phsize > l) {
        l = pread(fd, buf+1, phsize, eh->e_phoff);
        if (l < 0) return (struct MapInfos){-9, 0, 0};
        if (l != phsize) return (struct MapInfos){-10, 0, 0};
        ph = ph0 = (void *)(buf + 1);
    } else {
        ph = ph0 = (void *)((char *)buf + eh->e_phoff);
    }
    for (i=eh->e_phnum; i; i--, ph=(void *)((char *)ph+eh->e_phentsize)) {
        if (ph->p_type == PT_DYNAMIC) {
            dyn = ph->p_vaddr;
        } else if (ph->p_type == PT_TLS) {
            tls_image = ph->p_vaddr;
            //dso->tls.align = ph->p_align;
            //dso->tls.len = ph->p_filesz;
            //dso->tls.size = ph->p_memsz;
        } else if (ph->p_type == PT_GNU_RELRO) {
            //dso->relro_start = ph->p_vaddr & -PAGE_SIZE;
            //dso->relro_end = (ph->p_vaddr + ph->p_memsz) & -PAGE_SIZE;
        } else if (ph->p_type == PT_GNU_STACK) {
            if (ph->p_memsz > __default_stacksize) {
                __default_stacksize =
                        ph->p_memsz < DEFAULT_STACK_MAX ?
                        ph->p_memsz : DEFAULT_STACK_MAX;
            }
        }
        if (ph->p_type != PT_LOAD) continue;
        nsegs++;
        if (ph->p_vaddr < addr_min) {
            addr_min = ph->p_vaddr;
            off_start = ph->p_offset;
            prot = (((ph->p_flags&PF_R) ? PROT_READ : 0) |
                    ((ph->p_flags&PF_W) ? PROT_WRITE: 0) |
                    ((ph->p_flags&PF_X) ? PROT_EXEC : 0));
        }
        if (ph->p_vaddr+ph->p_memsz > addr_max) {
            addr_max = ph->p_vaddr+ph->p_memsz;
        }
    }
    if (!dyn) return (struct MapInfos){-3, 0, 0};
    addr_max += PAGE_SIZE-1;
    addr_max &= -PAGE_SIZE;
    addr_min &= -PAGE_SIZE;
    off_start &= -PAGE_SIZE;
    map_len = addr_max - addr_min + off_start;
    /* The first time, we map too much, possibly even more than
     * the length of the file. This is okay because we will not
     * use the invalid part; we just need to reserve the right
     * amount of virtual address space to map over later. */
    size_t map_here = addr_min;
    if (addr_min == 0) {
        map_here = ((size_t) min_library_address - map_len) & -16384;
    }

    // Poor man's ASLR
    if (ASLR_ENABLED) {
      uint32_t aslr_offset;
      __syscall3(SYS_getrandom, &aslr_offset, sizeof(uint32_t), 0);
      aslr_offset &= 0x3ffc000; // 12 bits, page-aligned
      map_here -= aslr_offset;
      if (addr_min != 0) {
        addr_min -= aslr_offset;
        addr_max -= aslr_offset;
      }
    }

    map = DL_NOMMU_SUPPORT
          ? mmap((void *)map_here, map_len, PROT_READ|PROT_WRITE|PROT_EXEC,
                 MAP_PRIVATE|MAP_ANONYMOUS, -1, 0)
          : mmap((void *)map_here, map_len, prot,
                 MAP_PRIVATE, fd, off_start);
    if (map==MAP_FAILED) return (struct MapInfos){-4, 0, 0};
    if (addr_min == 0 && map == map_here) {
        min_library_address = (unsigned char *) map_here;
    }
    //dso->map = map;
    //dso->map_len = map_len;
    /* If the loaded file is not relocatable and the requested address is
     * not available, then the load operation must fail. */
    if (eh->e_type != ET_DYN && addr_min && map!=(void *)addr_min) {
        return (struct MapInfos){-5, 0, 0};
    }
    base = map - addr_min;
    void *dso_phdr = 0;
    size_t dso_phnum = 0;
    for (ph=ph0, i=eh->e_phnum; i; i--, ph=(void *)((char *)ph+eh->e_phentsize)) {
        if (ph->p_type != PT_LOAD) continue;
        /* Check if the programs headers are in this load segment, and
         * if so, record the address for use by dl_iterate_phdr. */
        if (!dso_phdr && eh->e_phoff >= ph->p_offset
            && eh->e_phoff+phsize <= ph->p_offset+ph->p_filesz) {
            dso_phdr = (void *)(base + ph->p_vaddr + (eh->e_phoff-ph->p_offset));
            dso_phnum = eh->e_phnum;
            //dso->phentsize = eh->e_phentsize;
        }
        this_min = ph->p_vaddr & -PAGE_SIZE;
        this_max = ph->p_vaddr+ph->p_memsz+PAGE_SIZE-1 & -PAGE_SIZE;
        off_start = ph->p_offset & -PAGE_SIZE;
        prot = (((ph->p_flags&PF_R) ? PROT_READ : 0) |
                ((ph->p_flags&PF_W) ? PROT_WRITE: 0) |
                ((ph->p_flags&PF_X) ? PROT_EXEC : 0));
        /* Reuse the existing mapping for the lowest-address LOAD */
        if ((ph->p_vaddr & -PAGE_SIZE) != addr_min || DL_NOMMU_SUPPORT)
            if (mmap_fixed(base+this_min, this_max-this_min, prot, MAP_PRIVATE|MAP_FIXED, fd, off_start) == MAP_FAILED)
                return (struct MapInfos){-6, 0, 0};
        if (ph->p_memsz > ph->p_filesz && (ph->p_flags&PF_W)) {
            size_t brk = (size_t)base+ph->p_vaddr+ph->p_filesz;
            size_t pgbrk = brk+PAGE_SIZE-1 & -PAGE_SIZE;
            memset((void *)brk, 0, pgbrk-brk & PAGE_SIZE-1);
            if (pgbrk-(size_t)base < this_max && mmap_fixed((void *)pgbrk, (size_t)base+this_max-pgbrk, prot, MAP_PRIVATE|MAP_FIXED|MAP_ANONYMOUS, -1, 0) == MAP_FAILED)
                return (struct MapInfos){-7, 0, 0};
        }
    }
    done_mapping:
    //dso->base = base;
    //dso->dynv = laddr(dso, dyn);
    //if (dso->tls.size) dso->tls.image = laddr(dso, tls_image);
    return (struct MapInfos){map, base, base + dyn};
}



static char start_buffer[256];

__attribute__((visibility("hidden")))
void _dlstart_c(size_t *sp, size_t *dynv) {
	size_t i, aux[AUX_CNT], dyn[DYN_CNT];

	int argc = *sp;
	char **argv = (void *)(sp+1);

	for (i=argc+1; argv[i]; i++);
	size_t *auxv = (void *)(argv+i+1);

    for (i=0; i<AUX_CNT; i++) aux[i] = 0;
	for (i=0; auxv[i]; i+=2)
        if (auxv[i]<AUX_CNT && auxv[i] >= 0) {
            aux[auxv[i]] = auxv[i + 1];
        }

    for (i=0; i<DYN_CNT; i++) dyn[i] = 0;
    for (i=0; dynv[i]; i+=2) if (dynv[i]<DYN_CNT)
            dyn[dynv[i]] = dynv[i+1];

    libc.page_size = aux[AT_PAGESZ];
    if (libc.page_size == 0)
      libc.page_size = 4096;
    min_library_address = (unsigned char*) ((uintptr_t) aux[AT_PHDR] & -16384l);


    // get program header of app and decode dynamic loading section -> get library path (rpath, runpath)
    size_t phnum = aux[AT_PHNUM];
    size_t phentsize = aux[AT_PHENT];
    Phdr *ph = (void *)aux[AT_PHDR];
    size_t app_base = (size_t) ph;
    size_t *app_dynv = 0;
    size_t app_dyn[DYN_CNT];

    for (i=phnum; i--; ph = (void *)((char *)ph + phentsize)) {
        if (ph->p_type == PT_PHDR) {
            app_base = aux[AT_PHDR] - ph->p_vaddr;
            break;
        }
    }
    for (i=phnum; i--; ph = (void *)((char *)ph + phentsize)) {
        if (ph->p_type == PT_DYNAMIC) {
            app_dynv = ph->p_vaddr + app_base;
            break;
        }
    }

    for (i=0; i<DYN_CNT; i++) app_dyn[i] = 0;
    for (i=0; app_dynv[i]; i+=2) if (app_dynv[i]<DYN_CNT)
            app_dyn[app_dynv[i]] = app_dynv[i+1];

    char *library_path = app_dyn[DT_RPATH]
            ? (char *) app_base + app_dyn[DT_RPATH] + app_dyn[DT_STRTAB]
            : (char *) app_base + app_dyn[DT_RUNPATH] + app_dyn[DT_STRTAB];

    // Now find the libc (and open it)!
    int fd = -1;
    while (library_path[0] != '\0') {
        char *s = strchr(library_path, ':');
        int len = s ? s - library_path : strlen(library_path);
        strncpy(start_buffer, library_path, len);
        strcpy(start_buffer + len, "/libc.so");
        // test_write(start_buffer);
        fd = sys_open(start_buffer, O_RDONLY|O_CLOEXEC, O_RDWR);
        if (fd > 0)
            break;

        library_path += len;
        if (library_path[0] == ':')
            library_path++;
    }
    if (fd < 0) {
        test_write("Could not find libc!");
        __syscall1(SYS_exit_group, 1);
    }

    // map the libc to memory
    struct MapInfos infos = map_library(fd);
    if (infos.map < 0) {
        test_write("Could not map library!");
        __syscall1(SYS_exit_group, 1);
    }
    __syscall1(SYS_close, fd);
    /*test_write("libc has been loaded:");
    test_write_size(infos.map);
    test_write_size(infos.base);
    test_write_size(infos.dynv);*/

    // patch the auxv
    for (i=0; auxv[i]; i+=2) {
        if (auxv[i] == AT_BASE) {
            auxv[i+1] = (size_t) infos.map;
        }
    }

    // resolve symbols, pass the min_library_address to libc
    for (i=0; i<DYN_CNT; i++) dyn[i] = 0;
    size_t *libc_dynv = (size_t*) infos.dynv;
    for (i=0; libc_dynv[i]; i+=2) if (libc_dynv[i]<DYN_CNT)
            dyn[libc_dynv[i]] = libc_dynv[i+1];
    Sym *symbols = infos.base + dyn[DT_SYMTAB];
    char *strings = infos.base + dyn[DT_STRTAB];
    void (*libc_dlstart_c)(size_t *, size_t *) = 0;
    size_t libc_min_library_address = 0;
    for (int i = 1; i < 10000; i++) {
        if (strcmp(strings + symbols[i].st_name, "_dlstart_c_public") == 0)
            libc_dlstart_c = infos.base + symbols[i].st_value;
        if (strcmp(strings + symbols[i].st_name, "min_library_address") == 0)
            libc_min_library_address = infos.base + symbols[i].st_value;
        if (libc_dlstart_c && libc_min_library_address)
            break;
    }
    /*test_write("Symbols resolved:");
    test_write_size(libc_dlstart_c);
    test_write_size(libc_min_library_address);*/

    *((size_t*) libc_min_library_address) = min_library_address;

    // delegate it!
    //libc_dlstart_c(sp, libc_dynv);
    call_function_ptr(libc_dlstart_c, sp, libc_dynv);
    // __syscall1(SYS_exit_group, 0);
}
