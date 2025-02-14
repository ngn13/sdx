# sdx | shitty and dirty UNIX for x86_64

![test workflow status](https://img.shields.io/github/actions/workflow/status/ngn13/sdx/test.yml?label=tests)

a simple, work-in-progress UNIX-like hobby operating system for `x86_64` architecture, written in C and
assembly and uses GRUB multiboot2

to be clear i have absolutely no idea what i am doing, so i'm learning stuff along the way, which is the
main motivation of this project, to learn OS development

### roadmap/todo
- [x] multiboot
- [x] paging
- [x] long mode entry
- [x] video/framebuffer functions
- [x] printk/printf
- [x] simple heap implementation
- [x] IDT
- [x] PIC
- [x] PCI
- [X] AHCI
- [x] serial
- [ ] IDE
- [ ] FAT32
- [x] scheduler
- [x] userland
- [x] TSS
- [x] SYSCALL/SYSRET
- [ ] libc
- [ ] init
- [ ] possibly a shell and more programs?

### dependencies
here's a list of depends you'll need for building from source, running and testing:

| Name                                                   | Reason                                              | Note                                                                              |
| ------------------------------------------------------ | --------------------------------------------------- | --------------------------------------------------------------------------------- |
| [Git](https://git-scm.com/)                            | For obtaning the source code                        | You can also download the source code as an archive using github's interface      |
| [GNU binutils](https://www.gnu.org/software/binutils/) | Needed for cross-compilation                        | Ideally version 2.43, can be built with `make tools`, see [building](#building)   |
| [GNU GCC](https://gcc.gnu.org/)                        | For cross-compilation                               | Ideally version 14.2.0, can also be built with `make tools`                       |
| [Python](https://www.python.org/)                      | For generating the configuration header             | You'll need python3                                                               |
| [GPT fdisk](https://www.rodsbooks.com/gdisk/)          | For creating the disk image                         |                                                                                   |
| [dosfstools](https://github.com/dosfstools/dosfstools) | For formatting the disk image                       |                                                                                   |
| [GNU GRUB](https://www.gnu.org/software/grub/)         | The bootloader                                      | Make sure it has multiboot2 support                                               |
| [QEMU](https://www.qemu.org/)                          | For booting the image in a safe virtual environment | Ideally with GTK display support                                                  |
| [PCRE2](https://github.com/PCRE2Project/pcre2)         | To check QEMU's serial logs                         | Only needed for `pcre2grep`, in some distros package is called `pcre2-utils`      |

### building
start by obtaining the source code, you can just clone the repository:
```bash
git clone https://github.com/ngn13/sdx
```

the cross-compilation tools may be available in your distribution's package repository, or you can build them
from the scratch using the automated script:
```bash
make tools
```
this script will build the necessary tools and install them to `/opt/cross/bin`, if you have your cross tools
in an another directory, you'll need to pass the `CROSSDIR` option to make in order to change the  path used for
the build, or you can directly specify the path for GNU `gcc`, `ld` and `ar` with `CC`, `LD` and `AR` options.

after installing these tools, you should generate a create a configuration file, you can just copy the default:
```bash
make config
```

then you can use `make` to build all the binaries (they will be placed in `dist/`)
```bash
make
```

and you can create a raw disk image (`dist/sdx.img`) using the scripts again:
```bash
make image
```

### running
for safety, it's suggested you only run this in a virtual system, you can use QEMU, which is most likely available in your
distribution's package repository:
```bash
make qemu
```
this will run QEMU with GTK display, if you don't have the GTK display installed, you run the script manually to run QEMU with
no display and only with serial output:
```bash
./scripts/qemu.sh --log --no-gtk
```

### testing
after running the QEMU script, you can check the serial output's logs (`serial.log`) to make sure that everything went good:
```bash
make test
```

### debugging
if you use the QEMU setup detailed in the previous section, then you can debug the system using a remote GDB setup:
```bash
make debug
```

### contributing
if you want to help me check off some of the shit on the roadmap or if you just want to make fun of my dogshit code or feel
free to create an issue/PR

### resources
here is an awesome list of resources/documentation that i use (ill keep extending this as i find more resources),
this list doesn't contain any spec documents, however specs are referenced in the source code

- [OSDev wiki](https://wiki.osdev.org/)
- [Osdev-Notes](https://github.com/dreamportdev/Osdev-Notes)
- [Write your own Operating System](https://www.youtube.com/playlist?list=PLHh55M_Kq4OApWScZyPl5HhgsTJS9MZ6M)
- [GCC's assembler syntax](https://www.felixcloutier.com/documents/gcc-asm.html)
- [GNU Assembler Examples](https://cs.lmu.edu/~ray/notes/gasexamples/)
- [The 32 bit x86 C Calling Convention](https://aaronbloomfield.github.io/pdr/book/x86-32bit-ccc-chapter.pdf)
- [The 64 bit x86 C Calling Convention](https://aaronbloomfield.github.io/pdr/book/x86-64bit-ccc-chapter.pdf)

also there's a great community with bunch of helpfull people in [#osdev](ircs://irc.libera.chat/#osdev) channel on libera
