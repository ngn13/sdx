// clang-format off

/*

 * sdx | simple and dirty UNIX
 * written by ngn (https://ngn.tf) (2024)

 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.

*/

// clang-format on

#include "core/im.h"
#include "core/pci.h"
#include "core/pic.h"
#include "core/serial.h"
#include "core/sched.h"
#include "core/proc.h"

#include "boot/end.h"
#include "boot/multiboot.h"

#include "mm/pm.h"
#include "mm/vmm.h"

#include "fs/vfs.h"
#include "fs/fmt.h"

#include "video.h"

#include "util/panic.h"
#include "util/printk.h"
#include "util/asm.h"

#include "errno.h"
#include "types.h"

void entry() {
  /*

   * here, we initialize video memory in width the framebuffer mode
   * framebuffer info is obtained from the multiboot data
   * see boot/multiboot.S

   * also cars 2 is the best cars movie btw

  */
  if (!video_init(VIDEO_MODE_FRAMEBUFFER))
    panic("Failed to initialize the framebuffer video mode");

  // initialize serial communication ports (UART)
  serial_init();

  /*

   * obtain the available memory region start and the end address

   * for the start: we check if the page table's end address is larger then
   * the end address for the kernel, if so, we use the page table's end address

   * for the end: we check if the last mapped memory address is larger then the
   * available memory region's end address, if so, we use the region's address

  */
  uint64_t avail_start = pm_end > end_addr ? pm_end : end_addr;
  uint64_t avail_end   = pm_mapped > mb_mem_avail_limit ? mb_mem_avail_limit : pm_mapped;

  /*

   * paging is done in before switching to long mode
   * here we are just doing some extra checks and logging

   * see boot/paging.S and boot/multiboot.S

  */
  if (!pm_init(avail_start, avail_end))
    panic("Failed to initialize the paging manager");

  /*

   * initialize the interrupt manager (IM)

   * by default all the interrupts are handled by the default handler
   * this can be changed using the other IM functions

  */
  im_init();

  /*

   * initialize the programmable interrupt controller (PIC)

   * we need to enable this before enabling interrupts
   * otherwise since the vector offset is not set we would get a random
   * exception interrupt from the PIC

  */
  if (!pic_init())
    panic("Failed to initialize the PIC");

  if (!pic_enable())
    panic("Failed to enable the PIC");

  // enable the interrupts
  im_enable();

  // initialize the scheduler
  if (sched_init() != 0)
    panic("Failed to start the scheduler");

  // make current task (us) critikal
  sched_level(current, TASK_LEVEL_CRITIKAL);

  // initialize peripheral component interconnect (PCI) devices
  pci_init();

  /*

   * look for an available root filesystem and mount it

   * to do so we will loop over all the disk partitions with disk_next()
   * then we will attempt to load a filesystem from the partitions

   * if we successfully load a filesystem from a partition, we will
   * check the filesysteö is a possible root filesystem using fs_is_rootfs()

   * if we end up finding a possible root filesystem, then we will mount
   * the filesystem to the root (/) using vfs_mount()

  */
  pinfo("Looking for an available root filesystem partition");

  disk_part_t *part   = NULL;
  fs_t        *rootfs = NULL;

  while ((part = disk_next(part)) != NULL) {
    // attempt to create a filesystem from the partition
    if ((rootfs = fs_new(FS_TYPE_DETECT, part)) == NULL)
      continue;

    if (fs_is_rootfs(rootfs) == 0)
      break;

    fs_free(rootfs);
    rootfs = NULL;
  }

  if (NULL == rootfs)
    panic("No available root filesystem");

  pdebg("Loaded a %s root filesystem from 0x%x", fs_name(rootfs), part);
  pdebg("Mounting the root filesystem");

  if (vfs_mount("/", rootfs) != 0)
    panic("Failed to mount the root filesystem");

  /*

   * initialize the process list
   * this will also execute the /init process

  */
  proc_init();

  /*

   * after executing /init we don't have anything to do now
   * we will just wait for userland system calls

   * so we can just kill the current task, which will happen next
   * time sched() gets called, we can manually call it or we can just wait

  */
  sched_kill(current);
  _hang(); // wait forever
}
