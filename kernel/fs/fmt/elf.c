#include "util/printk.h"

#include "fs/fmt.h"
#include "fs/vfs.h"

#include "errno.h"
#include "types.h"

#define elf_info(f, ...) pinfo("ELF (0x%x): " f, node, ##__VA_ARGS__)
#define elf_fail(f, ...) pfail("ELF (0x%x): " f, node, ##__VA_ARGS__)
#define elf_debg(f, ...) pdebg("ELF (0x%x): " f, node, ##__VA_ARGS__)

// structures based on https://wiki.osdev.org/ELF_Tutorial

struct elf_header {
#define ELF_NIDENT 16
  uint8_t  e_ident[ELF_NIDENT];
  uint16_t e_type;
  uint16_t e_machine;
  uint32_t e_version;
  uint32_t e_entry;
  uint32_t e_phoff;
  uint32_t e_shoff;
  uint32_t e_flags;
  uint16_t e_ehsize;
  uint16_t e_phentsize;
  uint16_t e_phnum;
  uint16_t e_shentsize;
  uint16_t e_shnum;
  uint16_t e_shstrndx;
};

enum elf_ident {
  EI_MAG0       = 0, // 0x7F
  EI_MAG1       = 1, // 'E'
  EI_MAG2       = 2, // 'L'
  EI_MAG3       = 3, // 'F'
  EI_CLASS      = 4, // Architecture (32/64)
  EI_DATA       = 5, // Byte Order
  EI_VERSION    = 6, // ELF Version
  EI_OSABI      = 7, // OS Specific
  EI_ABIVERSION = 8, // OS Specific
  EI_PAD        = 9  // Padding
};

#define ELFMAG0 0x7F // e_ident[EI_MAG0]
#define ELFMAG1 'E'  // e_ident[EI_MAG1]
#define ELFMAG2 'L'  // e_ident[EI_MAG2]
#define ELFMAG3 'F'  // e_ident[EI_MAG3]

#define elf_check_magic(header)                                                                                        \
  (header->e_ident[EI_MAG0] == ELFMAG0 && header->e_ident[EI_MAG1] == ELFMAG1 &&                                       \
      header->e_ident[EI_MAG2] == ELFMAG2 && header->e_ident[EI_MAG3] == ELFMAG3)

#define ELFCLASS64  (2)  // x64
#define ELFDATA2LSB (1)  // little endian
#define ET_REL      (1)  // relocateable file
#define EM_x86_64   (62) // AMD x86-64
#define EV_CURRENT  (1)  // ELF version

const char *__elf_check(struct elf_header *header) {
  if (!elf_check_magic(header))
    return false;

  if (header->e_ident[EI_CLASS] != ELFCLASS64)
    return "unsupported class";

  if (header->e_ident[EI_DATA] != ELFDATA2LSB)
    return "unsupported byte order";

  if (header->e_machine != EM_x86_64)
    return "unsupported machine";

  if (header->e_ident[EI_VERSION] != EV_CURRENT)
    return "unsupported version";

  if (header->e_type != ET_REL)
    return "unsupported type";

  return NULL;
}

int32_t elf_load(vfs_node_t *node, void **addr) {
  struct elf_header header;
  const char       *err_msg = NULL;
  int32_t           err     = 0;

  if ((err = vfs_read(node, 0, sizeof(header), &header) < 0)) {
    elf_fail("failed to read the header");
    return err;
  }

  if ((err_msg = __elf_check(&header)) != NULL) {
    elf_debg("invalid header: %s", err_msg);
    return -ENOEXEC;
  }

  return -ENOSYS;
}
