# coredump自动压缩生成

普通开发工程师设置coredump

- setrlimit系统调用设置进程的core文件大小限制
- echo "1" > /proc/sys/kernel/core_uses_pid
  设置pid可以作为core文件名的扩展
- echo "/corefile/core-%p-%u-%g-%s-%t-%h-%e" > /proc/sys/kernel/core_pattern
  设置core文件生成路径以及命名，依次代表pid-uid-gid-产生core的信号-unix时间-主机名-命令名

第一步每个进程都需要设置，后两步整个系统设置一次就够了。

正常生成core文件大几百MB，在设备emmc里分了一个1GB的分区专门生成存放core文件。直到有一天万恶的产品经理想要把这个分区给刀了给其他功能用，把刀架在我的脖子上逼我就范。我用gzip压缩了一下core文件，几百MB可以压缩到几MB（coredump里有大量的内存数据，且很多内存虽然申请了但没有用到都是0，压缩率就很高了），确实可以把分区刀小一点，我从了。

第一步打开百度，第二步搜索coredump自动压缩，第三步打开csdn，第四步关闭浏览器一气呵成并没有解决问题，网上的帖子没找一个能用的，还是得靠自己，自己动手丰衣足食。

既然是proc文件夹下，那对应的core_pattern内核里肯定有相应的处理函数，直接在内核路径grep core_pattern，找到core_pattern的proc处理函数proc_dostring_coredump，继续看函数内部会把数据写到哪个变量里，

```
	{
		.procname	= "core_pattern",
		.data		= core_pattern,
		.maxlen		= CORENAME_MAX_SIZE,
		.mode		= 0644,
		.proc_handler	= proc_dostring_coredump,
	},
```

```
int proc_dostring(struct ctl_table *table, int write,
		  void __user *buffer, size_t *lenp, loff_t *ppos)
{
	if (write && *ppos && sysctl_writes_strict == SYSCTL_WRITES_WARN)
		warn_sysctl_write(table);

	return _proc_do_string((char *)(table->data), table->maxlen, write,
			       (char __user *)buffer, lenp, ppos);
}
```

看到这里就知道写到了table->data里，table->data就是结构体里的core_pattern。

那还是继续grep core_pattern，看一下内核哪里会使用这个变量。会看到format_corename里使用了，接着看什么地方调用了这个接口，发现do_coredump里调用了。

```
do_coredump省略部分代码
	ispipe = format_corename(&cn, &cprm);

	if (ispipe) {
		call_usermodehelper_setup
		call_usermodehelper_exec
	}else {
	
	}
```

看见call_usermodehelper_setup、call_usermodehelper_exec这两个接口都知道要走哪才能执行我们设置的脚本，即我们期望ispipe为真，那进入format_corename分析

```
static int format_corename(struct core_name *cn, struct coredump_params *cprm)
{
	const struct cred *cred = current_cred();
	const char *pat_ptr = core_pattern;
	int ispipe = (*pat_ptr == '|');
```

一进去多么的直白，core_pattern第一个字符要为‘|’，ispipe为真。

```
static int format_corename(struct core_name *cn, struct coredump_params *cprm)
{
	const struct cred *cred = current_cred();
	const char *pat_ptr = core_pattern;
	int ispipe = (*pat_ptr == '|');
	int pid_in_pattern = 0;
	int err = 0;

	cn->used = 0;
	cn->corename = NULL;
	if (expand_corename(cn, core_name_size))
		return -ENOMEM;
	cn->corename[0] = '\0';

	if (ispipe)
		++pat_ptr;//去掉第一个|字符，从第二个开始

	/* Repeat as long as we have more pattern to process and more output
	   space */
	while (*pat_ptr) {
		if (*pat_ptr != '%') {
			err = cn_printf(cn, "%c", *pat_ptr++);
		} else {
			switch (*++pat_ptr) {
			case 0:
			case '%'://将相应的参数替换为对应的数值，省略具体代码
			case 'p':
			case 'P':
			case 'i':
			case 'I':
			case 'u':
			case 'g':
			case 'd':
			case 's':
			case 't':
			case 'h':
			case 'e':
			case 'E':
			case 'c':
			}
			++pat_ptr;
		}

		if (err)
			return err;
	}

out:
	/* Backward compatibility with core_uses_pid:
	 *
	 * If core_pattern does not include a %p (as is the default)
	 * and core_uses_pid is set, then .%pid will be appended to
	 * the filename. Do not do this for piped commands. */
	if (!ispipe && !pid_in_pattern && core_uses_pid) {
		err = cn_printf(cn, ".%d", task_tgid_vnr(current));
		if (err)
			return err;
	}
	return ispipe;
}
```

通过以上接口，可以得知如果core_pattern的值为“|/usr/sbin/core_helper %s”,那cn->corename就为“/usr/sbin/core_helper 9”（9只是举例%s指信号值）

通过以上接口，将proc里的core_pattern格式化后的结果给了cn->corename，那我们继续分析do_coredump里如何使用cn->corename。

```
		helper_argv = argv_split(GFP_KERNEL, cn.corename, NULL);
		if (!helper_argv) {
			printk(KERN_WARNING "%s failed to allocate memory\n",
			       __func__);
			goto fail_dropcount;
		}

		retval = -ENOMEM;
		sub_info = call_usermodehelper_setup(helper_argv[0],
						helper_argv, NULL, GFP_KERNEL,
						umh_pipe_setup, NULL, &cprm);
		if (sub_info)
			retval = call_usermodehelper_exec(sub_info,
							  UMH_WAIT_EXEC);
```

通过以上代码，可以看到cn.corename被分割为helper_argv，argv_split函数的功能就是将字符串里的空格替换为‘\0’，然后把各个空格分开的首地址变成字符串指针数值传出来，好比通过命令行输入“./main 1 2 3”字符串执行程序，在程序里就是int main(int argc, char **argv),输入的字符串会被分割为argv。cn.corename被分割后通过call_usermodehelper_exec里调用queue_work会将命令送到内核去异步执行任务。

分析到这里我们就知道如何去设置proc下的core_pattern值以及，脚本如何写

```
echo "|/usr/sbin/core_helper %s %e %p %t" > /proc/sys/kernel/core_pattern
```

然后/usr/sbin/core_helper脚本是

```
#!/bin/sh
exec gzip - > /mnt/cdp/core-$1-$2-$3-$4.gz
```

几个注意的点

- 脚本需要有执行权限
- 设备里需要有gzip命令
- 如果需要执行自定义的脚本core_pattern首字符一定需要’|‘

通过dmesg可以查看脚本是否执行成功。