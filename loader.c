#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <elf.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#define ELFMAG "\177ELF"
#define SELFMAG 4

#define PAGE_SIZE 4096
#define STACK_SIZE (PAGE_SIZE *10)
#define STACK_START 0x10000000
#define STACK_TOP (STACK_START+STACK_SIZE)
#define ELF_MIN_ALIGN	PAGE_SIZE
#define MAX_ARG_STRLEN	PAGE_SIZE

/* include/linux/auxvec.h */
#define AT_VECTOR_SIZE_BASE	20

/* include/linux/mm_types.h */
#define AT_VECTOR_SIZE (2*(AT_VECTOR_SIZE_BASE + 1))

/* linux/fs/binfmt_elf.c */
#define ELF_PAGESTART(_v) ((_v) & ~(unsigned long)(ELF_MIN_ALIGN-1))
#define ELF_PAGEOFFSET(_v) ((_v) & (ELF_MIN_ALIGN-1))
#define ELF_PAGEALIGN(_v) (((_v) + ELF_MIN_ALIGN - 1) & ~(ELF_MIN_ALIGN - 1))

/* linux/fs/binfmt_elf.c */
#define STACK_ADD(sp, items) ((Elf64_Addr *)(sp) - (items))
#define STACK_ROUND(sp, items) (((unsigned long) ((Elf64_Addr *) (sp) - (items))) &~ 15UL)
#define STACK_ALLOC(sp, len) ({ sp -= len ; sp; })

/* include/asm/exec.h */
#define arch_align_stack(p) ((unsigned long)(p) & ~0xf)

int8_t *sp, *stack_top;
int8_t *arg_start, *arg_end, *envp_start, *envp_end;

// setting up the stack 
int create_elf_tables(int argc, char *envp[], Elf64_Ehdr *ep)
{   
	int items, envc = 0;
	int i;
	int8_t *p;
	Elf64_auxv_t elf_info[AT_VECTOR_SIZE];
	Elf64_auxv_t *auxv;
	int ei_index = 0;

	memset(elf_info, 0, sizeof(elf_info));

    /* In some cases(e.g. Hyper-Threading), we want to avoid L1 
     * evictions by the processes running on the saem package, One
     * thing we can do is to shuffle the initial stack for them. 
     */ 
	sp = (int8_t *) arch_align_stack(sp);

	// Copy Loaders AT_VECTOR 
	while (*envp++ != NULL)
		;
	for (auxv = (Elf64_auxv_t *) envp; auxv->a_type != AT_NULL;
			auxv++, ei_index++) {
		elf_info[ei_index] = *auxv;
		if (auxv->a_type == AT_PHDR)
			elf_info[ei_index].a_un.a_val = 0;
        // NEW_AUX_ENT(AT_ENTRY, exec->e_entry)
		else if (auxv->a_type == AT_ENTRY)
			elf_info[ei_index].a_un.a_val = ep->e_entry;
        // NEW_AUX_ENT(AT_PHNUM, exec->e_phnum)
		else if (auxv->a_type == AT_PHNUM)
			elf_info[ei_index].a_un.a_val = ep->e_phnum;
	}

	// Advance past the AT_NULL entry.
	ei_index += 2;
	sp = (int8_t *) STACK_ADD(sp, ei_index * 2);

	envc = 2;
	items = (argc + 1) + (envc + 1) + 1;
	sp = (int8_t *) STACK_ROUND(sp, items);
	stack_top = sp;

	// Now, let's put argc (and argv, envp if appropriate) on the stack
	*((long *) sp) = (long) argc - 1;
	sp += 8;

	// Populate list of argv pointers back to argv strings. 
	p = arg_start;
    while(--argc)
    {
        size_t len; 

        *((unsigned long *) sp) = (unsigned long) p; 
        len = strnlen(p, MAX_ARG_STRLEN); 
        sp += 8; 
        p += len+1;
    }
	*((unsigned long *) sp) = NULL;
	sp += 8;

	// Populate list of envp pointers back to envp strings.
	p = envp_start;
    while(envc --)
    {
        size_t len;

		*((unsigned long *) sp) = (unsigned long) p;
		len = strnlen(p, MAX_ARG_STRLEN);
		sp += 8;
		p += len;
    }
	*((unsigned long *) sp) = NULL;
	sp += 8;

	// Put the elf_info on the stack in the right place.
	memcpy(sp, elf_info, sizeof(Elf64_auxv_t) * ei_index);

	return 0;
}

// fs/binfmt_elf.c set_brk()
int map_bss(unsigned long start, unsigned long end, int prot)
{   
	start = ELF_PAGEALIGN(start);
	end = ELF_PAGEALIGN(end);
    // Map anonymous pages, if needed, and clear the area
	size_t size = end - start; 
	if (end > start) {
		return (int) mmap((void *) start, size, prot, MAP_FIXED | MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
	}
	return 0;
}

// fs/binfmt_elf.c
void *elf_map(Elf64_Addr addr, int prot, int type, int fd, Elf64_Phdr *eppnt)
{   
	unsigned long size = eppnt->p_filesz + ELF_PAGEOFFSET(eppnt->p_vaddr);
	unsigned long off = eppnt->p_offset - ELF_PAGEOFFSET(eppnt->p_vaddr);
	addr = ELF_PAGESTART(addr);
	size = ELF_PAGEALIGN(size);

    /* mmap() will return -EINVAL if given a zero size, but a
	 * segment with zero filesize is perfectly valid */
	if (!size)
		return (void *) addr;
	
	return mmap((void *) addr, size, prot, type, fd, off);
}

// linux/fs/binfmt_elf.c
int padzero(unsigned long elf_bss)
{    
	unsigned long nbyte;

	nbyte = ELF_PAGEOFFSET(elf_bss);
	if (nbyte) {
		nbyte = ELF_MIN_ALIGN - nbyte;
		memset((void *) elf_bss, 0, nbyte);
	}
	return 0;
}

int load_elf_binary(int fd, Elf64_Ehdr *ep, int argc, char *envp[])
{
	Elf64_Phdr phdr;
	Elf64_Addr entry;
	unsigned long elf_bss, elf_brk;
	int bss_prot = 0;
	int i;

	// lseek(fd, ep->e_phoff, SEEK_SET);
	elf_bss = 0;
	elf_brk = 0;
	for (i = 0; i < ep->e_phnum; i++) {
		int elf_prot = 0, elf_flags;
		unsigned long k;
		Elf64_Addr vaddr;

		// lseek(fd, ep->e_phoff + i*ep->e_phentsize, SEEK_SET);
		lseek(fd, ep->e_phoff + i*sizeof(Elf64_Phdr), SEEK_SET);

		memset(&phdr, 0, sizeof(Elf64_Phdr));
		if (read(fd, &phdr, sizeof(Elf64_Phdr)) < 0) {
            printf("Read erorr on phdr\n");
			return -1;
		}
		
		if (phdr.p_type != PT_LOAD)
			continue;

		if (elf_brk > elf_bss) {
			if (map_bss(elf_bss, elf_brk, bss_prot) < 0) {
                printf("Map_bss error\n");
				return -1;
			}

			padzero(elf_bss);
		}

		if (phdr.p_flags & PF_R) elf_prot |= PROT_READ;
		if (phdr.p_flags & PF_W) elf_prot |= PROT_WRITE;
		if (phdr.p_flags & PF_X) elf_prot |= PROT_EXEC;

		vaddr = phdr.p_vaddr;

		if (elf_map(vaddr, elf_prot, MAP_PRIVATE | MAP_FIXED | MAP_EXECUTABLE, fd, &phdr) < 0) {
            printf("Elf_map error\n");
			return -1;
		}
		
        /*
		 * Find the end of the file mapping for this phdr, and
		 * keep track of the largest address we see for this.
		 */
		k = phdr.p_vaddr + phdr.p_filesz;
		if (k > elf_bss)
			elf_bss = k;
        /*
		 * Do the same thing for the memory mapping - between
		 * elf_bss and last_bss is the bss section.
		 */
		k = phdr.p_vaddr + phdr.p_memsz;
		if (k > elf_brk) {
			bss_prot = elf_prot;
			elf_brk = k;
		}
	}

	if (map_bss(elf_bss, elf_brk, bss_prot) < 0) {
        printf("Map_bss error\n");
		return -1;
	}
	if (elf_bss != elf_brk)
		padzero(elf_bss);

	entry = ep->e_entry;

    // Setting up the rest of its stack(in its new randomized loacation)
	create_elf_tables(argc, envp, ep);

    /*
	Use inline assembly to clean up registers state
	Zero all register contents before starting the program under test.
	Libc checks rdx, and if it is non-zero it interprets it as a pointer during program shutdown.
	*/
	asm("movq $0, %rax"); // rax : accumulator register
	asm("movq $0, %rbx"); // rbx : base register
	asm("movq $0, %rcx"); 
	asm("movq $0, %rdx"); // rdx : data register 
	asm("movq %0, %%rsp" : : "r" (stack_top)); // rsp : stack pointer register
    /* Jump to test program transferring control to the entry point via jmp instruction */ 
	asm("jmp *%0" : : "c" (entry)); 
	printf("never reached\n");

	return 0;
}

void show_elf_header(Elf64_Ehdr *ep)
{
	printf("e_ident: %s\n", ep->e_ident);
	printf("e_entry: %p\n", ep->e_entry);
	printf("e_phoff: %lu\n", ep->e_phoff);
	printf("e_shoff: %lu\n", ep->e_shoff);
	printf("sizeof Elf64_Ehdr: %lu\n", sizeof(Elf64_Ehdr));
	printf("e_ehsize: %u\n", ep->e_ehsize);
	printf("e_phentsize: %u\n", ep->e_phentsize);
	printf("e_phnum: %u\n", ep->e_phnum);
}

int main(int argc, char *argv[], char *envp[])
{
	Elf64_Ehdr elf_header;
	Elf64_auxv_t *auxv;
	Elf64_Addr loader_entry;
	char **p;
	int fd;

	if (argc < 2) {
        printf("./loader exe_file\n"); 
		exit(EXIT_FAILURE);
	}

	if ((fd = open(argv[1], O_RDWR)) < 0) {
        printf("File open failed\n"); 
		exit(EXIT_FAILURE);
	}
    
    // Read the ELF header
	if (read(fd, &elf_header, sizeof(Elf64_Ehdr)) < 0) {
        printf("Read Error\n"); 
		exit(EXIT_FAILURE);
	}

	if (memcmp(&elf_header, ELFMAG, SELFMAG) != 0) {
        printf("File format error\n");
		exit(EXIT_FAILURE);
	}

	// Check whether entry point is overlapped with loaded program
	p = envp;
	while (*p++ != NULL)
		;
	for (auxv = (Elf64_auxv_t *) envp; auxv->a_type != AT_NULL; auxv++) {
		if (auxv->a_type == AT_ENTRY) {
			loader_entry = auxv->a_un.a_val;	
		}
	}

	if (elf_header.e_entry == loader_entry) {
        printf("Entry point is overlapped\n"); 
		exit(EXIT_FAILURE);
	}

	// show_elf_header(&elf_header);

    /* Setup stack 
     * 
     * ---------Memory limit---------
     * NULL pointer
     * program_filename string
     * envp[envc-1] string
     * ...
     * envp[1] string
     * envp[0] string
     * argv[argc-1] string
     * ...
     * argv[1] string
     * argv[0] string
     * ------------------------------
     */
    size_t len;
	int i;

	sp = mmap((void *) STACK_START, STACK_SIZE, PROT_READ | PROT_WRITE, MAP_FIXED | MAP_ANONYMOUS | MAP_GROWSDOWN | MAP_PRIVATE, -1 ,0);
    if (sp == MAP_FAILED) {
        printf("mmap error in %s\n", __func__);
		return -1;
	}
	memset(sp, 0, STACK_SIZE);
	sp = (Elf64_Addr) STACK_TOP;
	// NULL pointer 
	STACK_ADD(sp, 1); 

	// push env to stack 
	len = strnlen("ENVVAR2=2", MAX_ARG_STRLEN);
	sp -= (len + 1);
	envp_end = sp;
	memcpy(sp, "ENVVAR2=2", len + 1);
	len = strnlen("ENVVAR1=1", MAX_ARG_STRLEN);
	sp -= (len + 1);
	memcpy(sp, "ENVVAR=1", len + 1);
	envp_start = sp;

	// push args to stack 
	for (i = argc - 1; i > 0; i--) {
		len = strnlen(argv[i], MAX_ARG_STRLEN);
		sp = ((char *) sp) - (len + 1);
		if (i == argc - 1)
			arg_end = sp;
		if (i == 1)
			arg_start = sp;
		memcpy(sp, argv[i], len + 1);
	}

	load_elf_binary(fd, &elf_header, argc, envp);
}


