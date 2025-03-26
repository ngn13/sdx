// clang-format off

/*

 * sdx | simple and dirty UNIX
 * written by ngn (https://ngn.tf) (2025)

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

#include "boot/multiboot.h"
#include "boot/boot.h"

#include "core/acpi.h"
#include "util/string.h"
#include "util/printk.h"
#include "util/panic.h"

#include "sched/sched.h"
#include "sched/task.h"

#include "core/serial.h"
#include "core/pci.h"
#include "core/pic.h"
#include "core/tty.h"
#include "core/im.h"

#include "mm/pmm.h"
#include "mm/vmm.h"

#include "syscall.h"
#include "fs/vfs.h"
#include "video.h"

void entry() {
  int32_t err = 0;

  // initialize serial communication ports (UART)
  if ((err = serial_init()) != 0)
    pfail("Failed to initalize the serial communication: %s", strerror(err));

  // load multiboot data
  if ((err = mb_load((void *)BOOT_MB_INFO_VADDR)) != 0)
    panic("Failed to load multiboot data: %s", strerror(err));

  // initialize virtual memory manager
  if ((err = vmm_init()) != 0)
    panic("Failed to initialize virtual memory manager: %s", strerror(err));

  // initialize physical memory manager (so we can start mapping & allocating memory)
  if ((err = pmm_init()) != 0)
    panic("Failed to initialize physical memory manager: %s", strerror(err));

  // initialize framebuffer video driver
  if ((err = video_init(VIDEO_MODE_FRAMEBUFFER)) != 0)
    pfail("Failed to initialize the framebuffer video mode: %s", strerror(err));

  // enable the cursor
  video_cursor_show();

  /*

   * initialize the interrupt manager (IM)

   * by default all the interrupts are handled by the default handler
   * this can be changed using the other IM functions

  */
  im_init();

  /*

   * initialize the programmable interrupt controller (PIC)

   * we need to enable this before enabling interrupts otherwise
   * since the vector offset is not set we would get a random exception
   * interrupt from the PIC

  */
  if (!pic_init())
    panic("Failed to initialize the PIC");

  if (!pic_enable())
    panic("Failed to enable the PIC");

  // enable the interrupts
  im_enable();

  // initialize the scheduler
  if ((err = sched_init()) != 0)
    panic("Failed to start the scheduler: %s", strerror(err));

  // make current task (us) critikal
  sched_prio(TASK_PRIO_CR1TIKAL);

  /*

   * load ACPI, some devices we are gonna load/register next may need to use
   * some of the ACPI functions, so let's get this done first

  */
  if ((err = acpi_load()) != 0)
    pfail("Failed to load ACPI: %s", strerror(err));

  // initialize peripheral component interconnect (PCI) devices
  pci_init();

  // register filesystem devices
  if ((err = serial_register()) != 0)
    pfail("Failed to register serial devices: %s", strerror(err));

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
    if (fs_new(&rootfs, FS_TYPE_DETECT, part) != 0)
      continue;

    // check if the filesystem can be used a root filesystem
    if (fs_is_rootfs(rootfs) == 0)
      break;

    // if the filesystem cannot be used a root filesystem, free it
    fs_free(rootfs);
    rootfs = NULL;
  }

  if (NULL == rootfs)
    panic("No available root filesystem");

  pdebg("Loaded a %s root filesystem from 0x%x", fs_name(rootfs), part);
  pinfo("Mounting the root %s filesystem", fs_name(rootfs));

  // mount the root filesystem
  if ((err = vfs_mount("/", rootfs)) != 0)
    panic("Failed to mount the root filesystem: %s", strerror(err));

  /*

   * setup the user system calls (syscalls)
   * we'll need them before starting userland processes

  */
  if ((err = sys_setup()) != 0)
    panic("Failed to setup the user calls: %s", strerror(err));

  // execute the init program
  if ((err = sys_exec("/init", NULL, NULL)) < 0)
    panic("Failed to execute init: %s", strerror(err));
}
