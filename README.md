Simple data image dumper and installer
======================================

This is a simple tool mainly aimed to creating disk image dump that can
with careful planning and execution be installed to a live system (with
disks mounted read only).

This horrible thing is useful in some low end cloud providers that
won't let you choose your OS images freely, but happen to have such
image that the user can reboot as read only.


CAUTION
=======

USE THIS THING WITH EXTREME CAUTION. IF YOU DON'T KNOW WHAT YOU ARE
DOING, YOU'LL DESTROY YOUR DATA. EVEN IF YOU KNOW, WHAT YOU ARE DOING,
YOU CAN DESTROY YOUR DATA. IF YOUR RISING STAR IS NEAR URANUS, YOU CAN
DESTROY YOUR DATA.

YOU HAVE BEEN WARNED.


Usage
=====

Let's assume, you have a VirtualBox VDI image that has your entire
bootable system. Remember to shut down the virtual machine before you
start.

```sh
$ qemu-img convert -f vdi ../somewhere/my-bootable-disk.vdi -O raw my-bootable-disk.raw

$ create-dump my-bootable-disk.raw my-bootable-disk.dump

$ rm my-bootable-disk.raw
```

Now that you have a dump file my-bootable-disk.dump let's assume that
your cheap cloud server is running in IP address 1.2.3.4 and has been
modified so that everything is mounted read only (and where possible
unmounted). Remember also to disable swap and lvm writes etc. Disable
all possible daemons from the server.

Before you made the server read only, you naturally remembered to copy
or compile install-dump binary there. If you didn't, mount a tmpfs
partition somewhere and copy install-dump binary there (assuming you
have compatible binary at hand). Furthermore, let's assume that your
boot disk is /dev/sda.

```sh
$ cat my-bootable-disk.dump | gzip | ssh 1.2.3.4 'gunzip - | /wherever/it/is/install-dump -r /dev/sda
```

At this point, the die has been cast and if the dump installation
breaks or system crashes or something, you have most probably killed
your cloud server for good and must start over by installing a cloud
vendor provided image and setting it read only etc.

Since there is a live filesystem is running, the system may crash and
all weird stuff may happen. But if the system really is read-only, the
dump should be written and system rebooted.


Author
======

Timo J. Rinne <tri@iki.fi>


License
=======

GPL-2.0
