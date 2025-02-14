#include "boot/multiboot.h"
#include "errno.h"

void *mb_data = NULL;

int32_t mb_load(void *data) {
  if (NULL == (mb_data = data))
    return -ENOMEM;

  if ((*(uint64_t *)mb_data) & 7)
    return -EINVAL;

  return 0;
}

void *mb_get(uint32_t type) {
  if (NULL == mb_data)
    return NULL;

  uint32_t              total_size = *(uint32_t *)mb_data;
  struct multiboot_tag *tag        = NULL;

  // https://www.gnu.org/software/grub/manual/multiboot2/html_node/kernel_002ec.html#kernel_002ec
  for (tag = mb_data + 8; tag->type != MULTIBOOT_TAG_TYPE_END;
      tag  = (void *)((uint8_t *)tag + ((tag->size + 7) & ~7))) {
    if (type == tag->type)
      return tag;
  }

  // tag not found
  return NULL;
}
