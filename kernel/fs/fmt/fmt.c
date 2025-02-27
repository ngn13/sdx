#include "fs/fmt.h"
#include "mm/region.h"
#include "mm/vmm.h"

#include "util/list.h"
#include "util/mem.h"

#include "types.h"
#include "errno.h"

#define fmt_info(f, ...) pinfo("Format: " f, ##__VA_ARGS__)
#define fmt_fail(f, ...) pfail("Format: " f, ##__VA_ARGS__)

struct fmt {
  const char *name;
  int32_t (*load)(vfs_node_t *, fmt_t *);
};

struct fmt fmt_table[] = {
    {.name = "ELF", .load = elf_load},
    {.name = NULL,  .load = NULL    },
};

int32_t fmt_load(vfs_node_t *node, fmt_t *fmt) {
  if (NULL == node || NULL == fmt)
    return -EINVAL;

  bzero(fmt, sizeof(fmt_t));
  struct fmt *cur = fmt_table;
  int32_t     err = 0;

  for (; cur->name != NULL; cur++) {
    // attempt to load the node using the current loader
    if ((err = cur->load(node, fmt)) == 0) {
      fmt_info("%s loader successfuly loaded 0x%p", cur->name, node);
      return 0;
    }

    /*

     * if something went wrong, free the fmt structure, so next
     * time we use a different loader, we can use the same structure

    */
    fmt_free(fmt);

    /*

     * non-format error means that the selected format is true
     * however while loading it, we encountered another error

    */
    if (err != -ENOEXEC) {
      fmt_fail("%s loader encountered an error loading 0x%p: %s", cur->name, node, strerror(err));
      return err;
    }
  }

  fmt_fail("no compatible format for the node");
  return -ENOEXEC;
}

void fmt_free(fmt_t *fmt) {
  if (NULL == fmt)
    return;

  // unmap all the memory regions
  region_each(&fmt->mem) region_unmap(fmt->mem);

  // free all the regions
  slist_clear(&fmt->mem, region_free, region_t);

  // clear the format data
  bzero(fmt, sizeof(fmt_t));
}
