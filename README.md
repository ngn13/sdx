# sdx | simple/shitty and dirty UNIX for x86_64
as of oct 30 ive been working on this UNIX like OS for over 2 months now, and i decided
to publish the git repo even tho the project itself is not even close to a stable release yet

tons of shit missing and still bunch of shit i need to do so im actively working on this project

### roadmap
this is not an actual roadmap idk wtf im doing i just add shit here
when i complete it

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
- [ ] IDE
- [ ] FAT32
- [ ] scheduler
- ...

### building
start by obtaining the source code, you can just clone the repository:
```bash
git clone https://github.com/ngn13/sdx
```

for building from source, you will need cross-compilation tools for x86_64, this tools may be available
in your distribution's package repository, or you can build them from the scratch using the automated script:
```bash
make tools
```
this script will build the necessary tools and install them to `/opt/cross/bin`

after installing these tools, you can use `make` to build all the binaries:
```bash
make
```

and you can create a raw disk image using the scripts again:
```bash
make image
```

### running
for safety, it's suggested you only run this in a virtual system, you can use QEMU, which is most likely available in your
distribution's package repository:
```bash
make qemu
```

### debugging
if you use the QEMU setup detailed in the previous section, then you can debug the system using a remote GDB setup:
```bash
make debug
```
you can also use the QEMU monitor

### contributing
if you want to help me check off some of the shit on the roadmap or if you just want to make fun of my dogshit code or
if you want to discuss about the [best cars moive](kernel/main.c) feel free to create an issue/PR


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

also there's a great community with bunch of helpfull people in [#osdev](ircs://irc.libera.chat/#osdev) chanel on libera