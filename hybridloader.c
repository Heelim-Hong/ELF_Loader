#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <elf.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdbool.h>
#include <signal.h>

#define ELFMAG "\177ELF"
#define SELFMAG 4

#define PAGE_SIZE 4096
#define STACK_SIZE (PAGE_SIZE * 10)
#define STACK_START 0x10000000
#define STACK_TOP (STACK_START + STACK_SIZE)
#define ELF_MIN_ALIGN PAGE_SIZE
#define MAX_ARG_STRLEN PAGE_SIZE
#define PH_TABLE_SIZE 15

/* include/linux/auxvec.h */
#define AT_VECTOR_SIZE_BASE 20

/* include/linux/mm_types.h */
#define AT_VECTOR_SIZE (2 * (AT_VECTOR_SIZE_BASE + 1))

/* linux/fs/binfmt_elf.c */
#define ELF_PAGESTART(_v) ((_v) & ~(unsigned long)(ELF_MIN_ALIGN - 1))
#define ELF_PAGEOFFSET(_v) ((_v) & (ELF_MIN_ALIGN - 1))
#define ELF_PAGEALIGN(_v) (((_v) + ELF_MIN_ALIGN - 1) & ~(ELF_MIN_ALIGN - 1))

/* linux/fs/binfmt_elf.c */
#define STACK_ADD(sp, items) ((Elf64_Addr *)(sp) - (items))
#define STACK_ROUND(sp, items) (((unsigned long)((Elf64_Addr *)(sp) - (items))) & ~15UL)
#define STACK_ALLOC(sp, len) ( \
	{                          \
		sp -= len;             \
		sp;                    \
	})

/* include/asm/exec.h */
#define arch_align_stack(p) ((unsigned long)(p) & ~0xf)

int8_t *sp, *stack_top;
int8_t *arg_start, *arg_end, *envp_start, *envp_end;
Elf64_Phdr ph_table[PH_TABLE_SIZE];
int fd;

int create_elf_tables(int argc, char *envp[], Elf64_Ehdr *ep)
{
	int items, envc = 0;
	int i;
	int8_t *p;
	Elf64_auxv_t elf_info[AT_VECTOR_SIZE];
	Elf64_auxv_t *auxv;
	int ei_index = 0;

	memset(elf_info, 0, sizeof(elf_info));

	// Rounds down the existing stack position to a 16-byte boundary
	sp = (int8_t *)arch_align_stack(sp);

	while (*envp++ != NULL)
		;
	for (auxv = (Elf64_auxv_t *)envp; auxv->a_type != AT_NULL;
		 auxv++, ei_index++)
	{
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

	ei_index += 2;
	sp = (int8_t *)STACK_ADD(sp, ei_index * 2);

	envc = 2;
	items = (argc + 1) + (envc + 1) + 1;
	sp = (int8_t *)STACK_ROUND(sp, items);
	stack_top = sp;

	*((long *)sp) = (long)argc - 1;
	sp += 8;

	p = arg_start;
	while (--argc)
	{
		size_t len;

		*((unsigned long *)sp) = (unsigned long)p;
		len = strnlen(p, MAX_ARG_STRLEN);
		sp += 8;
		p += len + 1;
	}
	*((unsigned long *)sp) = NULL;
	sp += 8;

	p = envp_start;
	while (envc--)
	{
		size_t len;

		*((unsigned long *)sp) = (unsigned long)p;
		len = strnlen(p, MAX_ARG_STRLEN);
		sp += 8;
		p += len;
	}
	*((unsigned long *)sp) = NULL;
	sp += 8;

	memcpy(sp, elf_info, sizeof(Elf64_auxv_t) * ei_index);

	return 0;
}

// fs/binfmt_elf.c set_brk()
int map_bss(unsigned long addr, int prot, int page_num)
{
	int flags;
	addr = ELF_PAGESTART(addr);
	size_t size = page_num * PAGE_SIZE;

	return (int)mmap((void *)addr, size, prot, MAP_FIXED | MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
}

// fs/binfmt_elf.c
void *elf_map(Elf64_Addr addr, int prot, int type, int fd, Elf64_Phdr *eppnt)
{
	unsigned long size = eppnt->p_filesz + ELF_PAGEOFFSET(eppnt->p_vaddr);
	unsigned long off = eppnt->p_offset - ELF_PAGEOFFSET(eppnt->p_vaddr);
	addr = ELF_PAGESTART(addr);

	if (!size)
		return (void *)addr;

	return mmap((void *)addr, size, prot, type, fd, off);
}

// linux/fs/binfmt_elf.c
int padzero(unsigned long elf_bss)
{
	unsigned long nbyte;

	nbyte = ELF_PAGEOFFSET(elf_bss);
	if (nbyte)
	{
		nbyte = ELF_MIN_ALIGN - nbyte;
		memset((void *)elf_bss, 0, nbyte);
	}
	return 0;
}

int load_elf_binary(int fd, Elf64_Ehdr *ep, int argc, char *envp[])
{
	Elf64_Addr entry;
	unsigned long elf_bss, elf_brk;
	int bss_prot = 0;
	int i;

	lseek(fd, ep->e_phoff, SEEK_SET);
	elf_bss = 0;
	elf_brk = 0;

	for (i = 0; i < ep->e_phnum; i++)
	{
		if (read(fd, &ph_table[i], sizeof(Elf64_Phdr)) < 0)
		{
			printf("Read error on phdr\n");
			return -1;
		}
	}

	for (; i < PH_TABLE_SIZE; i++)
		ph_table[i].p_type = PT_NULL;

	for (i = 0; i < ep->e_phnum; i++)
	{
		int elf_prot = 0, elf_flags;
		unsigned long k;
		Elf64_Addr vaddr;

		if (ph_table[i].p_type != PT_LOAD)
			continue;

		if (ph_table[i].p_flags & PF_R)
			elf_prot |= PROT_READ;
		if (ph_table[i].p_flags & PF_W)
			elf_prot |= PROT_WRITE;
		if (ph_table[i].p_flags & PF_X)
			elf_prot |= PROT_EXEC;

		vaddr = ph_table[i].p_vaddr;

		if (elf_map(vaddr, elf_prot, MAP_PRIVATE | MAP_FIXED | MAP_EXECUTABLE, fd, &ph_table[i]) < 0)
		{
			printf("Elf map error\n");
			return -1;
		}

		k = ph_table[i].p_vaddr + ph_table[i].p_filesz;
		if (k > elf_bss)
			elf_bss = k;

		k = ph_table[i].p_vaddr + ph_table[i].p_memsz;
		if (k > elf_brk)
		{
			bss_prot = elf_prot;
			elf_brk = k;
		}
	}

	if (elf_bss != elf_brk)
		padzero(elf_bss);

	entry = ep->e_entry;

	create_elf_tables(argc, envp, ep);

	asm("movq $0, %rax"); // rax : accumulator register
	asm("movq $0, %rbx"); // rbx : base register
	asm("movq $0, %rcx");
	asm("movq $0, %rdx"); // rdx : data register
	asm("movq %0, %%rsp"
		:
		: "r"(stack_top)); // rsp : stack pointer register
	asm("jmp *%0"
		:
		: "c"(entry));
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

void signal_handler(int sig, siginfo_t *si, void *unused)
{
	bool is_feasible = false;
	int elf_prot = 0;
	Elf64_Addr addr = (Elf64_Addr)si->si_addr;
	Elf64_Phdr *ph;
	int i;

	for (i = 0; i < PH_TABLE_SIZE; i++)
	{
		ph = &ph_table[i];
		if (ph->p_type != PT_LOAD)
			continue;

		if ((ph->p_vaddr <= addr) && addr <= (ph->p_vaddr + ph->p_memsz))
		{
			is_feasible = true;
			break;
		}
	}

	if (!is_feasible)
	{
		printf("Unvalid memory reference\n");
		exit(EXIT_FAILURE);
	}

	if (ph->p_flags & PF_R)
		elf_prot |= PROT_READ;
	if (ph->p_flags & PF_W)
		elf_prot |= PROT_WRITE;
	if (ph->p_flags & PF_X)
		elf_prot |= PROT_EXEC;

	int page_num = 1;

	if (ELF_PAGEALIGN(addr) < ph->p_vaddr + ph->p_memsz)
		page_num++;

	if (map_bss(addr, elf_prot, page_num) < 0)
	{
		fprintf(stderr, "bss map error\n");
		exit(EXIT_FAILURE);
	}
}

int main(int argc, char *argv[], char *envp[])
{
	Elf64_Ehdr elf_header;
	Elf64_auxv_t *auxv;
	Elf64_Addr loader_entry;
	char **p;
	// int fd;
	struct sigaction sig;

	if (argc < 2)
	{
		printf("./loader exe_file\n");
		exit(EXIT_FAILURE);
	}

	if ((fd = open(argv[1], O_RDWR)) < 0)
	{
		printf("File open failed\n");
		exit(EXIT_FAILURE);
	}

	// Read the ELF header
	if (read(fd, &elf_header, sizeof(Elf64_Ehdr)) < 0)
	{
		printf("Read Error\n");
		exit(EXIT_FAILURE);
	}

	if (memcmp(&elf_header, ELFMAG, SELFMAG) != 0)
	{
		printf("File format error\n");
		exit(EXIT_FAILURE);
	}

	p = envp;
	while (*p++ != NULL)
		;
	for (auxv = (Elf64_auxv_t *)envp; auxv->a_type != AT_NULL; auxv++)
	{
		if (auxv->a_type == AT_ENTRY)
		{
			loader_entry = auxv->a_un.a_val;
		}
	}

	if (elf_header.e_entry == loader_entry)
	{
		printf("Entry point is overlapped\n");
		exit(EXIT_FAILURE);
	}

	size_t len;
	int i;

	sp = mmap((void *)STACK_START, STACK_SIZE, PROT_READ | PROT_WRITE, MAP_FIXED | MAP_ANONYMOUS | MAP_GROWSDOWN | MAP_PRIVATE, -1, 0);
	if (sp == MAP_FAILED)
	{
		printf("mmap error in %s\n", __func__);
		return -1;
	}
	memset(sp, 0, STACK_SIZE);
	sp = (Elf64_Addr)STACK_TOP;

	STACK_ADD(sp, 1);

	len = strnlen("ENVVAR2=2", MAX_ARG_STRLEN);
	sp -= (len + 1);
	envp_end = sp;
	memcpy(sp, "ENVVAR2=2", len + 1);
	len = strnlen("ENVVAR1=1", MAX_ARG_STRLEN);
	sp -= (len + 1);
	memcpy(sp, "ENVVAR=1", len + 1);
	envp_start = sp;

	for (i = argc - 1; i > 0; i--)
	{
		len = strnlen(argv[i], MAX_ARG_STRLEN);
		sp = ((char *)sp) - (len + 1);
		if (i == argc - 1)
			arg_end = sp;
		if (i == 1)
			arg_start = sp;
		memcpy(sp, argv[i], len + 1);
	}

	memset(&sig, 0, sizeof(sig));
	sig.sa_flags = SA_SIGINFO | SA_RESTART;
	// sigemptyset(&sig.sa_mask);
	// sig.sa_sigaction = signal_handler;
	sig.sa_handler = signal_handler;
	if (sigaction(SIGSEGV, &sig, NULL) == -1)
	{
		printf("SIGACTION FAILURE\n");
		exit(EXIT_FAILURE);
	}

	load_elf_binary(fd, &elf_header, argc, envp);
}
