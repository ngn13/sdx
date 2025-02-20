#pragma once
#include "types.h"
#include "util/printk.h"

#define disk_debg(f, ...) pdebg("Disk: " f, ##__VA_ARGS__)
#define disk_info(f, ...) pinfo("Disk: " f, ##__VA_ARGS__)
#define disk_fail(f, ...) pfail("Disk: " f, ##__VA_ARGS__)

typedef enum disk_type {
  DISK_TYPE_UNKNOWN = 0, // unknown type
  DISK_TYPE_OPTICAL = 2, // optical disc drive
  DISK_TYPE_HDD     = 3, // hard disk drives
  DISK_TYPE_SSD     = 4, // solid state drives
} disk_type_t;

typedef enum disk_controller {
  DISK_CONTROLLER_AHCI,
} disk_controller_t;

typedef enum disk_op {
  DISK_OP_READ  = 0,
  DISK_OP_WRITE = 1,
  DISK_OP_INFO  = 2,
} disk_op_t;

typedef struct disk_part {
  // partition info
  uint64_t start;    // start of the partition
  uint64_t size;     // end of the partition
  bool     bootable; // is the partition bootable

  // other stuff
  bool              available; // is the partition available
  struct disk      *disk;      // pointer to the disk object that the partition belongs to
  struct disk_part *next;      // next partition
} disk_part_t;

typedef struct disk {
  disk_type_t type; // type of the disk

  disk_controller_t controller; // controller used to communicate with the disk
  void             *data;       // data used by the controller for communcation

  bool read_only; // is the disk read-only
  bool available; // is the disk available for commands

  uint64_t sector_size; // size of a sector (in bytes)
  uint64_t size;        // size of the disk (in sectors)

  disk_part_t *parts;      // partitions
  uint32_t     part_count; // partition count

  struct disk *next; // next disk in the list
} disk_t;

disk_part_t *disk_part_add(disk_t *disk, uint64_t start, uint64_t size); // add a new partition
void         disk_part_clear(disk_t *disk);                              // remove no longer available partitions
bool         disk_part_scan(disk_t *disk);                               // scan the disk for available partitions

disk_t *disk_add(disk_controller_t controller, void *data); // create a disk and it to the list
void    disk_remove(disk_t *disk);                          // remove a disk from the list
bool    disk_do(disk_t *disk, disk_op_t op, uint64_t lba, uint64_t sector_count, uint8_t *buf); // do a disk operation
disk_part_t *disk_next(disk_part_t *part); // get the next partition after the provided "pre" partition, if "pre" is
                                           // NULL then returns the first partition
#define disk_read_raw(disk, lba, sector_count, buf) disk_do(disk, DISK_OP_READ, lba, sector_count, buf)
bool disk_read_lba(disk_t *disk, uint64_t lba, uint64_t size, uint8_t *buf); // read from disk (starting from a LBA)
bool disk_read(disk_t *disk, uint64_t offset, uint64_t size, uint8_t *buf);  // read from disk (starting from an offset)

#define disk_write_raw(disk, lba, sector_count, buf) disk_do(disk, DISK_OP_WRITE, lba, sector_count, buf)
bool disk_write_lba(disk_t *disk, uint64_t lba, uint64_t size, uint8_t *buf); // write to a disk (starting from a LBA)
bool disk_write(disk_t *disk, uint64_t offset, uint64_t size, uint8_t *buf);  // write to disk (starting from an offset)
