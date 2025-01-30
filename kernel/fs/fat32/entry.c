#include "fs/fat32.h"

#include "util/string.h"
#include "util/bit.h"
#include "util/mem.h"

#include "errno.h"
#include "limits.h"

struct fat32_lfn {
  uint8_t  order;
  uint16_t chars_first[5];
  uint8_t  attr; // always FAT32_ATTR_LFN
  uint8_t  type; // always zero
  uint8_t  checksum;
  uint16_t chars_mid[6];
  uint16_t first_cluster; // always zero
  uint16_t chars_last[2];
} __attribute__((packed));

#define FAT32_CHARS_PER_LFN  13
#define fat32_lfn_order(l)   (l->order & 0b11111)
#define fat32_lfn_is_last(l) (bit_get(l->order, 6))

uint64_t fat32_cluster_next(fs_t *fs, uint64_t cluster) {
  // https://wiki.osdev.org/FAT#FAT_32_and_exFAT
  uint8_t  fat_table[fs_sector_size(fs)];
  uint32_t offset = (cluster * 4) % fs_sector_size(fs);

  if (!__fat32_read_raw(fat32_data()->fat_sector, 1, fat_table)) {
    fat32_debg("failed to read FAT");
    return 0;
  }

  cluster = *(uint32_t *)&fat_table[offset];
  cluster &= 0x0FFFFFFF; // remove high 4 bits

  if (cluster >= 0x0FFFFFF8 || cluster == 0x0FFFFFF7)
    return 0; // we reached the end or a "bad" cluster

  return cluster; // next cluster number
}

// assumes the buffer is at least FAT32_CHARS_PER_LFN long
uint8_t __fat32_lfn_read(struct fat32_lfn *lfn, char *buf) {
  if (NULL == lfn || NULL == buf)
    return NULL;

  uint8_t i = 0, e = 0, c = 0;

  for (i = 0; i < sizeof(lfn->chars_first) / 2 && (c = lfn->chars_first[i] & 0x00ff) != 0 && c != 0xff; i++)
    buf[e++] = c;

  for (i = 0; i < sizeof(lfn->chars_mid) / 2 && (c = lfn->chars_mid[i] & 0x00ff) != 0 && c != 0xff; i++)
    buf[e++] = c;

  for (i = 0; i < sizeof(lfn->chars_last) / 2 && (c = lfn->chars_last[i] & 0x00ff) != 0 && c != 0xff; i++)
    buf[e++] = c;

  return e;
}

uint16_t __fat32_lfn_calc_size(struct fat32_lfn *lfn, uint8_t last_size) {
  // we need the last logical, first physical LFN entry to calculate the size
  if (!fat32_lfn_is_last(lfn))
    return 0;

  if (fat32_lfn_order(lfn) <= 0)
    return 0; // invalid LFN order

  uint64_t size = (fat32_lfn_order(lfn) - 1) * FAT32_CHARS_PER_LFN; // calculate the required size for next LFNs
  size += last_size;                                                // add current (last) LFNs size

  return size;
}

int64_t __fat32_entry_name(fs_t *fs, struct fat32_dir_entry **entry, char *name, int64_t size) {
  uint16_t lfn_size = 0, lfn_total = 0, name_index = 0;
  char     lfn_buffer[FAT32_CHARS_PER_LFN + 1];

  // just to prevent any possible memory issues
  lfn_buffer[FAT32_CHARS_PER_LFN] = 0;

#define name_check(s)                                                                                                  \
  do {                                                                                                                 \
    if (s > NAME_MAX)                                                                                                  \
      return -ENAMETOOLONG;                                                                                            \
    else if (s > size)                                                                                                 \
      return -EOVERFLOW;                                                                                               \
  } while (0);
#define name_append(c)                                                                                                 \
  do {                                                                                                                 \
    name_check(name_index + 1);                                                                                        \
    name[name_index++] = c;                                                                                            \
  } while (0);
#define name_copy(b, s)                                                                                                \
  do {                                                                                                                 \
    name_check(name_index + s);                                                                                        \
    memcpy(name + name_index, b, s);                                                                                   \
  } while (0);

parse_entry:
  if ((*entry)->attr != FAT32_ATTR_LFN) {
    // did we already obtained the name from LFNs?
    if (name_index != 0)
      goto ret;

    // no? alr lets just parse the directory entry's name
    for (uint8_t i = 0; i < sizeof((*entry)->name) && (*entry)->name[i] != 0x20; i++) {
      if (i == 8) // last 3 are the extension (ik FAT32 is weird)
        name_append('.');
      name_append((*entry)->name[i]);
    }

  ret:
    name_append(0); // add NULL termiantor
    return name_index;
  }

  // copy current LFN to the lfn_buffer
  lfn_size = __fat32_lfn_read((void *)(*entry), lfn_buffer);

  // calculate the total size that all the LFNs are gonna take, will return non-zero on success
  if (lfn_total == 0) {
    lfn_total  = __fat32_lfn_calc_size((void *)(*entry), lfn_size);
    name_index = lfn_total; // last entry first
  }

  // if we have the size calculated, copy the current LFN buffer to the name
  if (lfn_total != 0) {
    name_index -= lfn_size;
    name_copy(lfn_buffer, lfn_size);

    // we got all the LFNs, now lets just make sure name_index points to the end
    if (name_index == 0)
      name_index = lfn_total;
  }

  // get the next entry
  while (true) {
    (*entry)++;

    if (fat32_entry_is_last(*entry))
      return -ERANGE;

    if (!fat32_entry_is_unused(*entry))
      break;
  }

  goto parse_entry;
}

int64_t fat32_entry_get(fs_t *fs, uint64_t cluster, uint64_t offset, int64_t size, void *buffer) {
  // https://wiki.osdev.org/FAT#Reading_Directories
  char                    cluster_buffer[fat32_data_bytes_per_cluster()];
  struct fat32_dir_entry *entry = NULL;
  int32_t                 err   = 0;

next_cluster:
  if (!__fat32_read_cluster(cluster, cluster_buffer)) {
    fat32_debg("failed to read the directory entries at cluster: %u", cluster);
    return -EFAULT;
  }

  for (entry = (void *)cluster_buffer;; entry++) {
    if (fat32_entry_is_last(entry)) {
      if ((cluster = fat32_cluster_next(fs, cluster)) == 0)
        return -ERANGE; // we reached the end without getting to the offset
      goto next_cluster;
    }

    if (fat32_entry_is_unused(entry))
      continue; // skip unused entry

    if (offset != 0 && entry->attr == FAT32_ATTR_LFN)
      continue; // skip LFNs that doesn't belong to us

    if (0 == offset--)
      break;
  }

  /*

   * at this point "dirent" points to entry at the offset
   * or it points to a LFN that belongs to the entry we want

  */
  return __fat32_entry_name(fs, &entry, buffer, size);
}

int32_t fat32_entry_from(fs_t *fs, uint64_t cluster, char *name, struct fat32_dir_entry *entry) {
  int64_t name_size = strlen(name) + 1;

  if (name_size > NAME_MAX + 1)
    return -ENAMETOOLONG;

  char                    name_buffer[name_size], cluster_buffer[fat32_data_bytes_per_cluster()];
  struct fat32_dir_entry *cur = NULL;

next_cluster:
  if (!__fat32_read_cluster(cluster, cluster_buffer)) {
    fat32_debg("failed to read the directory entries at cluster: %u", cluster);
    return -EFAULT;
  }

  for (cur = (void *)cluster_buffer;; cur++) {
    if (fat32_entry_is_last(cur)) {
      if ((cluster = fat32_cluster_next(fs, cluster)) == 0)
        return -ENOENT; // not found
      goto next_cluster;
    }

    if (fat32_entry_is_unused(cur))
      continue; // skip unused entry

    if (__fat32_entry_name(fs, &cur, name_buffer, name_size) < 0)
      continue; // move onto the next entry

    fat32_debg("%s == %s", name_buffer, name);
    fat32_debg("%x,%x,%x,%x,%x == %x,%x,%x,%x,%x",
               name_buffer[0], name_buffer[1], name_buffer[2], name_buffer[3], name_buffer[4],
               name[0], name[1], name[2], name[3], name[4]);

    if (strcmp(name_buffer, name) == 0){
      fat32_debg("found name: %s", name_buffer);
      break;
    }
  }

  memcpy(entry, cur, sizeof(struct fat32_dir_entry));
  fat32_debg("return 0");
  return 0;
}
