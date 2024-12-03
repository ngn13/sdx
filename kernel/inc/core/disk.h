#pragma once
#include "types.h"

typedef enum disk_type {
  DISK_TYPE_UNKNOWN = 0, // unknown type
  DISK_TYPE_OPTICAL = 2, // optical disc drive
  DISK_TYPE_HDD     = 3, // hard disk drives
  DISK_TYPE_SSD     = 4, // solid state drives
} disk_type_t;

typedef enum disk_controller {
  DISK_CONTROLLER_AHCI = 0,
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

disk_part_t *disk_part_next(disk_t *disk, disk_part_t *part);
disk_part_t *disk_part_add(disk_t *disk, uint64_t start, uint64_t size);
void         disk_part_clear(disk_t *disk);

disk_t *disk_add(disk_controller_t controller, void *data);
void    disk_remove(disk_t *disk);
disk_t *disk_next(disk_t *disk);
bool    disk_do(disk_t *disk, disk_op_t op, uint64_t offset, uint64_t size, uint8_t *buf);
bool    disk_scan(disk_t *disk);
