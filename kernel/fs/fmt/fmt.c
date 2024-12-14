#include "fs/fmt.h"
#include "mm/pm.h"

#include "util/mem.h"
#include "errno.h"

#define fmt_info(f, ...) pinfo("Format: (0x%x) " f, node, ##__VA_ARGS__)
#define fmt_fail(f, ...) pfail("Format: (0x%x) " f, node, ##__VA_ARGS__)

struct fmt {
  const char *name;
  int32_t (*load)(vfs_node_t *, fmt_info_t *);
};

struct fmt fmt_table[] = {
    {.name = "ELF", .load = elf_load},
    {.name = NULL,  .load = NULL    },
};

int32_t fmt_load(vfs_node_t *node, fmt_info_t *info) {
  if (NULL == node || NULL == info)
    return -EINVAL;

  struct fmt *fmt = fmt_table;
  int32_t     err = 0;
  bzero(info, sizeof(fmt_info_t));

  for (; fmt->name != NULL; fmt++) {
    if ((err = fmt->load(node, info)) == 0) {
      fmt_info("%s loader successfuly loaded the node", fmt->name);
      return 0;
    }

    fmt_free(info);

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

void fmt_free(fmt_info_t *info) {
  if (NULL == info)
    return;

  if (info->addr != 0 && info->pages != 0)
    pm_free(info->addr, info->pages);

  bzero(info, sizeof(fmt_info_t));
}
