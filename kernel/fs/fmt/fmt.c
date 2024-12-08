#include "fs/fmt.h"
#include "errno.h"

#define fmt_info(f, ...) pinfo("Format (0x%x): " f, node, ##__VA_ARGS__)
#define fmt_fail(f, ...) pfail("Format (0x%x): " f, node, ##__VA_ARGS__)

struct fmt {
  const char *name;
  int32_t (*load)(vfs_node_t *, void **);
};

struct fmt fmt_table[] = {
    {.name = "ELF", .load = elf_load},
    {.name = NULL,  .load = NULL    },
};

int32_t fmt_load(vfs_node_t *node, void *addr) {
  if (NULL == node || NULL == addr)
    return -EINVAL;

  struct fmt *fmt = fmt_table;
  int32_t     err = 0;

  for (; fmt->name != NULL; fmt++) {
    if ((err = elf_load(node, addr)) == 0) {
      fmt_info("%s loader successfuly loaded the node", fmt->name);
      return 0;
    }

    /*

     * non-format error means that the selected format is true
     * however while loading it, we encountered another error

    */
    if (err != ENOEXEC) {
      fmt_fail("%s loader encountered an error: %s", fmt->name, strerror(err));
      return err;
    }
  }

  fmt_fail("no compatible format for the node");
  return -ENOEXEC;
}
