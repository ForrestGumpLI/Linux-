# kdump在arm32上应用

## 内核编译

首先需要将主机kernel(捕获内核与主机内核l一致)配置开启

- CONFIG_KEXEC=y
- CONFIG_KEXEC_CORE=y
- CONFIG_ARM_ATAG_DTB_COMPAT=y
- CONFIG_CRASH_DUMP=y
- CONFIG_AUTO_ZRELADDR=y
- CONFIG_PROC_VMCORE=y
- CONFIG_DEBUG_INFO=y

CONFIG_KEXEC&CONFIG_KEXEC_CORE这两个配置是开启内核的kdump功能，以及kexec_load系统调用。

CONFIG_ARM_ATAG_DTB_COMPAT配置优先使用启动内核时传入的bootargs，否则使用设备树里的bootargs，kdump触发时启动捕获内核需要传递bootargs给捕获内核，所有优先使用传入的bootargs。

CONFIG_CRASH_DUMP生成kdump文件。CONFIG_AUTO_ZRELADDR自动指定zImage的解压地址，一般内核直接指定解压地址就行了，不过捕获内核的加载地址与主机内核不一致，设置为自动指定就可以用同一个内核。

CONFIG_PROC_VMCORE开启/proc/vmcore。CONFIG_DEBUG_INFO编译内核时加入调试信息（即编译选项增加-g，方便后续分析）。

内核编译完成后需要保存一下几个文件

- uImage
- zImage
- vmlinux
- *.dtb

uImage主机内核使用，zImage与*.dtb为捕获内核，dtb文件为当前内核使用的设备树编译后的文件。vmlinux生成kdump后分析使用。

## 捕获内核加载

首先交叉编译kexec命令

https://www.kernel.org/pub/linux/utils/kernel/kexec/kexec-tools-2.0.15.tar.xz

源码包

编译命令

```
LDFLAGS=-static ./configure --host=arm-none-linux-gnueabi
make
```

编译完成后build/sbin/kexec为kexec命令，拷贝至设备中。

启动主机内核

在原有的bootargs上需要增加crashkernel=64M，告诉主机内核为捕获内核保留64M内存否则无法加载捕获内核，32M也可以。

然后加载捕获内核

```
kexec --type zImage -p ./zImage --dtb=./*.dtb --append="console=ttyAMA0,115200n8 maxcpus=1 reset_devices"
```

这里的zImage与*.dtb就是刚刚编译内核保存的文件，append指定捕获内核的bootargs，可以在这基础上增加bootargs参数。可以通过/sys/kernel/kexec_crash_loaded判断是否加载成功，1为已加载。在smp系统上，kdump需要cpu支持热插拔否则无法加载捕获内核。

## 触发kdump

```
echo c > /proc/sysrq-trigger
```

以上命令可触发内核崩溃，然后启动捕获内核。捕获内核启动后在/proc/vmcore即为kdump文件，将其从设备中拷贝出来。

编译crash

https://github.com/crash-utility/crash

源码

```
make target=ARM
```

编译完成后使用最开始保留的vmlinux文件以及从设备拷贝出来的vmcore文件进行分析

```
./crash vmlinux vmcore
```

然后就可以分析崩溃时系统的各种信息。