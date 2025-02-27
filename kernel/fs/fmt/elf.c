#include "fs/fmt.h"
#include "fs/vfs.h"

#include "util/printk.h"
#include "util/math.h"
#include "util/mem.h"

#include "mm/region.h"
#include "mm/vmm.h"

#include "errno.h"
#include "types.h"

/*

 * ELF64 type definitions: https://uclibc.org/docs/elf-64-gen.pdf
 * ELF64 structure definitions: https://gist.github.com/x0nu11byt3/bcb35c3de461e5fb66173071a2379779

 * other resources:
 * - https://wiki.osdev.org/ELF
 * - https://wiki.osdev.org/ELF_Tutorial (this uses ELF32)

*/

#define elf_info(f, ...) pinfo("ELF: " f, ##__VA_ARGS__)
#define elf_fail(f, ...) pfail("ELF: " f, ##__VA_ARGS__)
#define elf_debg(f, ...) pdebg("ELF: " f, ##__VA_ARGS__)

/*

 * ELF header
 * -----------------------------------------------------------------------
 * ELF header is located at the start of the ELF binary and it stores
 * important information such as the target instruction set architecture

*/
struct elf_header {
#define ELF_NIDENT 16
  uint8_t  ident[ELF_NIDENT];
  uint16_t type;
  uint16_t machine;
  uint32_t version;
  uint64_t entry;
  uint64_t phoff;
  uint64_t shoff;
  uint32_t flags;
  uint16_t ehsize;
  uint16_t phentsize;
  uint16_t phnum;
  uint16_t shentsize;
  uint16_t shnum;
  uint16_t shstrndx;
} __attribute__((packed));

enum elf_ident {
  ELF_IDENT_MAG0       = 0, // 0x7F
  ELF_IDENT_MAG1       = 1, // 'E'
  ELF_IDENT_MAG2       = 2, // 'L'
  ELF_IDENT_MAG3       = 3, // 'F'
  ELF_IDENT_CLASS      = 4, // Architecture (32/64)
  ELF_IDENT_DATA       = 5, // Byte Order
  ELF_IDENT_VERSION    = 6, // ELF Version
  ELF_IDENT_OSABI      = 7, // OS Specific
  ELF_IDENT_ABIVERSION = 8, // OS Specific
  ELF_IDENT_PAD        = 9  // Padding
};

#define ELF_MAGIC                                                                                                      \
  ("\x7f"                                                                                                              \
   "ELF")                        // ik its formatted weirdly, see https://gcc.gnu.org/bugzilla/show_bug.cgi?id=33167
#define ELF_CLASS_64        (2)  // x64
#define ELF_DATA_2LSB       (1)  // little endian
#define ELF_TYPE_DYN        (3)  // dynamic ELF
#define ELF_MACHINE_x86_64  (62) // AMD x86-64
#define ELF_VERSION_CURRENT (1)  // current ELF format version

bool __elf_check_magic(struct elf_header *header) {
  char   *c = ELF_MAGIC;
  uint8_t i = 0;

  for (; *c != 0; i++, c++)
    if (header->ident[i] != *c)
      return false;
  return true;
}

/*

 * ELF section header
 * -----------------------------------------------------------------------
 * ELF binary contains a section header table, and each header contains
 * different information about different sections in the ELF binary
 * this table is located at the offset specified by the ELF header (shoff)

*/
struct elf_section_header {
  uint32_t name;
  uint32_t type;
  uint64_t flags;
  uint64_t addr;
  uint64_t offset;
  uint64_t size;
  uint32_t link;
  uint32_t info;
  uint64_t addralign;
  uint64_t entsize;
} __attribute__((packed));

#define ELF_SECTION_NUM_UNDEF (0)      // undefined
#define ELF_SECTION_NUM_ABS   (0xFFF1) // absolute

enum elf_section_header_types {
  ELF_SECTION_HEADER_NULL     = 0, // null section
  ELF_SECTION_HEADER_PROGBITS = 1, // program information
  ELF_SECTION_HEADER_SYMTAB   = 2, // symbol table
  ELF_SECTION_HEADER_STRTAB   = 3, // string table
  ELF_SECTION_HEADER_RELA     = 4, // relocation (with addend)
  ELF_SECTION_HEADER_NOBITS   = 8, // not present in file
  ELF_SECTION_HEADER_REL      = 9, // relocation (no addend)
};

enum elf_section_header_attr {
  ELF_SECTION_ATTR_WRITE = 1, // writeable
  ELF_SECTION_ATTR_ALLOC = 2, // in-memory
};

/*

 * ELF program header
 * -----------------------------------------------------------------------
 * The binary also contains a program header table, and this table is
 * located at the offset specified by the ELF header's "phoff" entry, it
 * has "phnum" entries and each header's size is specified by "phentsize",
 * each program header in this table defines a different memory segment,
 * which ultimately tells us how we should load the ELF into the memory.

*/
struct elf_program_header {
  uint32_t type;
  uint32_t flags;
  uint64_t offset;
  uint64_t vaddr;
  uint64_t paddr;
  uint64_t filesz;
  uint64_t memsz;
  uint64_t align;
} __attribute__((packed));

#define ELF_PH_TYPE_NULL         0          // program header table entry unused
#define ELF_PH_TYPE_LOAD         1          // loadable program segment
#define ELF_PH_TYPE_DYNAMIC      2          // dynamic linking information
#define ELF_PH_TYPE_INTERP       3          // program interpreter
#define ELF_PH_TYPE_NOTE         4          // auxiliary information
#define ELF_PH_TYPE_SHLIB        5          // reserved
#define ELF_PH_TYPE_PHDR         6          // entry for header table itself
#define ELF_PH_TYPE_TLS          7          // thread-local storage segment
#define ELF_PH_TYPE_NUM          8          // number of defined types
#define ELF_PH_TYPE_LOOS         0x60000000 // start of OS-specific
#define ELF_PH_TYPE_GNU_EH_FRAME 0x6474e550 // GCC .eh_frame_hdr segment
#define ELF_PH_TYPE_GNU_STACK    0x6474e551 // indicates stack executability
#define ELF_PH_TYPE_GNU_RELRO    0x6474e552 // read-only after relocation
#define ELF_PH_TYPE_LOSUNW       0x6ffffffa
#define ELF_PH_TYPE_SUNWBSS      0x6ffffffa // sun Specific segment
#define ELF_PH_TYPE_SUNWSTACK    0x6ffffffb // stack segment
#define ELF_PH_TYPE_HISUNW       0x6fffffff
#define ELF_PH_TYPE_HIOS         0x6fffffff // end of OS-specific
#define ELF_PH_TYPE_LOPROC       0x70000000 // start of processor-specific
#define ELF_PH_TYPE_HIPROC       0x7fffffff // end of processor-specific

#define ELF_PH_FLAGS_X (1 << 0) // executable
#define ELF_PH_FLAGS_W (1 << 1) // writable
#define ELF_PH_FLAGS_R (1 << 2) // readable

/*

 * ELF structure is used a little context to pass all the required
 * stuff to all the different functions, just to keep the argument list
 * and the function definitions clean (only used in this file)

*/
struct elf {
  void             *entry;  // entry point for the ELF
  region_t         *mem;    // memory regions used for loading the ELF
  vfs_node_t       *node;   // VFS file node to load
  struct elf_header header; // ELF header
  uint32_t          ph_pos; // current program header position
};

#define __elf_read(elf, offset, size, buffer) vfs_read(elf->node, offset, size, buffer)

const char *__elf_check(struct elf *elf) {
  if (!__elf_check_magic(&elf->header))
    return false;

  if (elf->header.ident[ELF_IDENT_CLASS] != ELF_CLASS_64)
    return "unsupported class";

  if (elf->header.ident[ELF_IDENT_DATA] != ELF_DATA_2LSB)
    return "unsupported byte order";

  if (elf->header.machine != ELF_MACHINE_x86_64)
    return "unsupported machine";

  if (elf->header.ident[ELF_IDENT_VERSION] != ELF_VERSION_CURRENT)
    return "unsupported version";

  return NULL;
}

int64_t __elf_section(struct elf *elf, struct elf_section_header *section, uint64_t index) {
  uint64_t pos = elf->header.shoff + (sizeof(struct elf_section_header) * index);
  return __elf_read(elf, pos, sizeof(struct elf_section_header), section);
}

/*int64_t __elf_load_strtable(struct elf *elf) {
  if (elf->header.shstrndx == ELF_SECTION_NUM_UNDEF)
    return 0; // strtable index is undefined (no strtable)

  struct elf_section_header strtable_header;
  int64_t                   err = 0;

  if ((err = __elf_section(elf, &strtable_header, elf->header.shstrndx)) < 0)
    return err; // failed to read the header

  if ((elf->strtable = vmm_alloc(strtable_header.size)) == NULL)
    return -ENOMEM; // memory allocation failed

  return elf_read(elf, strtable_header.offset, strtable_header.size, elf->strtable);
}*/

int64_t __elf_ph_next(struct elf *elf, struct elf_program_header *header) {
  if (elf->header.phnum <= elf->ph_pos) {
    elf_debg("reached the last program header (%u)", elf->header.phnum);
    return 0;
  }

  if (elf->header.phentsize != sizeof(struct elf_program_header)) {
    elf_debg("program header size is invalid (%u/%u)", elf->header.phentsize, sizeof(struct elf_program_header));
    return -ENOEXEC;
  }

  uint64_t offset = elf->header.phoff + (elf->header.phentsize * elf->ph_pos++);
  return __elf_read(elf, offset, sizeof(struct elf_program_header), header);
}

int32_t __elf_load_dyn(struct elf *elf) {
  struct elf_program_header header;

  region_t *mem  = NULL;
  uint8_t   type = 0;
  void     *pos  = NULL;
  uint64_t  size = 0;
  int64_t   err  = 0;

  // loop through the program headers one more time
  while ((err = __elf_ph_next(elf, &header)) > 0) {
    // check the header type (only care about LOAD)
    if (header.type != ELF_PH_TYPE_LOAD)
      continue;

    // check the size of the header (ignore empty)
    if (header.memsz == 0)
      continue;

    elf_debg("obtained %u. program header", elf->ph_pos);
    pdebg("     |- Type: 0x%x", header.type);
    pdebg("     |- Offset: 0x%x", header.offset);
    pdebg("     |- Vaddr: 0x%p", header.vaddr);
    pdebg("     |- Paddr: 0x%p", header.paddr);
    pdebg("     |- Filesz: 0x%x", header.filesz);
    pdebg("     |- Memsz: 0x%x", header.memsz);
    pdebg("     `- Align: 0x%x", header.align);

    // check the alignment
    /*if (header.offset != 0 && header.vaddr % header.offset != header.align) {
      elf_debg("invalid program header alignment");
      return -ENOEXEC;
    }*/

    // align the size to a page
    size = vmm_align(header.memsz);
    type = REGION_TYPE_CODE;

    // see if we can disable R/W permissions for this segment's pages
    if (!(header.flags & ELF_PH_FLAGS_R) && !(header.flags & ELF_PH_FLAGS_W))
      type = REGION_TYPE_RDONLY;

    // see if we should disable the execution for this segment's pages
    else if (!(header.flags & ELF_PH_FLAGS_X))
      type = REGION_TYPE_DATA;

    // create the new region
    if ((mem = region_new(type, VMM_VMA_USER, pos, vmm_calc(size))) == NULL) {
      elf_debg("failed to create new memory region");
      return -ENOMEM;
    }

    // map the new memory region
    if ((err = region_map(mem)) != 0) {
      elf_debg("failed to map %s memory region @ 0x%p (%u pages): %s",
          region_name(mem),
          mem->vaddr,
          mem->num,
          strerror(err));
      return err;
    }

    // add memory region to the list
    region_add(&elf->mem, mem);

    // set the first position
    if (NULL == pos)
      pos = mem->vaddr;

    // load filesz bytes from file to the allocated memory
    if (header.filesz != 0 && (err = __elf_read(elf, header.offset, header.filesz, pos)) < 0) {
      elf_debg("failed to load program header from the file: %s", strerror(err));
      return -EIO;
    }

    // zero out the rest of the memory
    for (uint64_t i = header.filesz; i < header.memsz; i++)
      ((uint8_t *)pos)[i] = 0;

    // see if this section contains the entrypoint
    if (header.vaddr < elf->header.entry && header.vaddr + header.memsz > elf->header.entry)
      elf->entry = pos + elf->header.entry;

    // move to the next position
    pos += size;
  }

  // check if we failed to get the next program header
  if (err < 0) {
    elf_debg("failed to read the %d. program header: %s", elf->ph_pos, strerror(err));
    return err;
  }

  // check if found the entrypoint
  if (elf->entry == 0) {
    elf_fail("failed to find the entry point");
    return -ENOEXEC;
  }

  return 0;
}

int32_t elf_load(vfs_node_t *node, fmt_t *fmt) {
  if (NULL == node || NULL == fmt)
    return -EINVAL;

  struct elf  elf;
  int64_t     err     = 0;
  const char *err_msg = NULL;

  bzero(&elf, sizeof(struct elf));
  elf.node = node;

  if ((err = vfs_read(node, 0, sizeof(struct elf_header), &elf.header)) < 0) {
    elf_fail("failed to read the header: %s", strerror(err));
    return err;
  }

  if ((err_msg = __elf_check(&elf)) != NULL) {
    elf_debg("invalid header: %s", err_msg);
    return -ENOEXEC;
  }

  switch (elf.header.type) {
  case ELF_TYPE_DYN:
    err = __elf_load_dyn(&elf);
    break;

  default:
    elf_debg("unsupported type");
    return -ENOEXEC;
  }

  fmt->entry = elf.entry;
  fmt->mem   = elf.mem;

  return err;
}
