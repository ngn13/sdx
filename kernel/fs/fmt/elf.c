#include "fs/fmt.h"
#include "fs/vfs.h"

#include "util/printk.h"
#include "util/mem.h"

#include "errno.h"
#include "types.h"

#define elf_info(f, ...) pinfo("ELF (0x%x): " f, node, ##__VA_ARGS__)
#define elf_fail(f, ...) pfail("ELF (0x%x): " f, node, ##__VA_ARGS__)
#define elf_debg(f, ...) pdebg("ELF (0x%x): " f, node, ##__VA_ARGS__)

// mostly based on https://wiki.osdev.org/ELF_Tutorial

struct elf_header {
#define ELF_NIDENT 16
  uint8_t  ident[ELF_NIDENT];
  uint16_t type;
  uint16_t machine;
  uint32_t version;
  uint32_t entry;
  uint32_t phoff;
  uint32_t shoff;
  uint32_t flags;
  uint16_t ehsize;
  uint16_t phentsize;
  uint16_t phnum;
  uint16_t shentsize;
  uint16_t shnum;
  uint16_t shstrndx;
};

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

struct elf_section_header {
  uint32_t name;
  uint32_t type;
  uint32_t flags;
  uint32_t addr;
  uint32_t offset;
  uint32_t size;
  uint32_t link;
  uint32_t info;
  uint32_t addralign;
  uint32_t entsize;
};

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

bool __elf_check_magic(struct elf_header *header) {
  char   *c = ELF_MAGIC;
  uint8_t i = 0;

  for (; *c != 0; i++, c++)
    if (header->ident[i] != *c)
      return false;
  return true;
}

struct elf {
  vfs_node_t       *node;
  struct elf_header header;
  char             *strtable;
};

#define elf_read(elf, offset, size, buffer) vfs_read(elf->node, sizeof(struct elf_header) + offset, size, buffer)

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
  return elf_read(elf, pos, sizeof(struct elf_section_header), section);
}

int64_t __elf_load_strtable(struct elf *elf) {
  if (elf->header.shstrndx == ELF_SECTION_NUM_UNDEF)
    return 0; // strtable index is undefined (no strtable)

  struct elf_section_header strtable_header;
  int64_t                   err = 0;

  if ((err = __elf_section(elf, &strtable_header, elf->header.shstrndx)) < 0)
    return err; // failed to read the header

  if ((elf->strtable = vmm_alloc(strtable_header.size)) == NULL)
    return -ENOMEM; // memory allocation failed

  return elf_read(elf, strtable_header.offset, strtable_header.size, elf->strtable);
}

int32_t __elf_load_dyn(struct elf *elf, void **addr) {
  return -ENOSYS;
}

int32_t elf_load(vfs_node_t *node, void **addr) {
  if (NULL == node || NULL == addr)
    return -EINVAL;

  struct elf  elf;
  const char *err_msg = NULL;
  int64_t     err     = 0;

  bzero(&elf, sizeof(elf));

  if ((err = vfs_read(node, 0, sizeof(elf.header), &elf.header) < 0)) {
    elf_fail("failed to read the header");
    return err;
  }

  if ((err_msg = __elf_check(&elf)) != NULL) {
    elf_debg("invalid header: %s", err_msg);
    return -ENOEXEC;
  }

  if ((err = __elf_load_strtable(&elf)) < 0) {
    elf_debg("failed to load the strtable: %s", strerror(err));
    return err;
  }

  switch (elf.header.type) {
  case ELF_TYPE_DYN:
    return __elf_load_dyn(&elf, addr);

  default:
    elf_debg("unsupported type");
    return -ENOEXEC;
  }

  return -ENOSYS;
}
