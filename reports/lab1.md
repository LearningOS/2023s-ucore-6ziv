## Chapter 3 实验报告

王哲威（[https://github.com/LearningOS/2023s-ucore-6ziv](https://github.com/LearningOS/2023s-ucore-6ziv)）



##### 实验内容

添加一个系统调用sys_task_info,获取当前进程的相关信息。



###### 具体实现：

首先，在syscall_ids.h中添加相关系统调用的定义：`#define SYS_task_info 410`，以及`#define MAX_SYSCALL_NUM 500`。并从`user/include/stddef.h`中复制`TaskStatus`和`TaskInfo`的定义到`proc.h`中。

此外，向`proc.h`中`struct proc`结构体的定义中添加如下两个字段：

* `uint64 time_scheduled`，用来表示程序开始被调度时的毫秒数

* `unsigned int syscall_counter[MAX_SYSCALL_NUM]` ，用来存放每种系统调用被调用的次数。

在`proc.c` 中的`allocproc`函数中添加相关初始化：将`syscall_counter`初始化为全0，并将`time_scheduled`初始化为`(uint64)-1`。在`scheduler` 函数中进行判断：如果进程的`time_scheduled`为`(uint64)-1`，则将它设置为`get_cycle()/(CPU_FREQ/1000)`。

此外，为了进行系统调用次数的统计，在`syscall()`函数靠前的部分进行相应的更新。

接下来，正式实现相关的系统调用：先在`syscall()`函数里的`switch`中添加`SYS_task_info`项目，并实现`sys_task_info`函数。

在`sys_task_info`函数中，建立`curr_proc()->status`到`TaskInfo::status`的映射：`RUNNING`状态对应`Running`，`SLEEPING`和`RUNNABLE`根据`time_scheduled`是否被初始化对应到`Uninit`或`Ready`，`ZOMBIE`对应`Exited`，其它状态则`panic`；利用记录的`time_scheduled`计算程序开始到现在经历的毫秒数；最后将`curr_proc()->syscall_counter`复制到`TaskInfo::syscall_times`，便完成了`TaskInfo`的填充。

在必要的地方添加头文件的引用，即完成了`sys_task_info`系统调用的添加。



###### 只记录被调度的时间

在代码中利用`ONLY_RUNNING_TIME`进行条件编译。当设置了这一宏时：`time_scheduled`中记录进程这次运行开始的毫秒数，在`scheduler()` 函数中进行更新。同时增加一个属性`uint64 total_used_time`用来记录这次运行之前，进程运行的毫秒数。

在程序`yield`或者`exit`时，将`total_used_time`更新为`total_used_time + get_cycle() / (CPU_FREQ/1000) - time_scheduled`。

这样，在`sys_task_info`中，我们可以利用这两个属性获得进程运行的时长：如果程序正在运行，则它等于`total_used_time + get_cycle() / (CPU_FREQ/1000) - time_scheduled`；否则，它等于`total_used_time`。



### 问答题：

1.

版本：

`[rustsbi] RustSBI version 0.3.0-alpha.2, adapting to RISC-V SBI v1.0.0`

`__ch2_bad_instruction`与`__ch2_bad_register`分别测试了使用特权指令`sret`与访问特权寄存器`sstatus`，输出如下：

`[ERROR 0]IllegalInstruction in application, epc = 0x0000000080400002, core dumped.`

也就是说抛出了`IllegalInstruction`异常。



2.在完整的实现中，`a0`指向当前进程的`trapframe`，记录着用来处理trap以及恢复现场的必要的寄存器等信息；而`a1`则指向进程的页表。

在这一阶段，`userret`函数只接受一个参数，`a0`仍然是`trapframe`，而`a1` 则没有定义。



3.因为`a0`作为函数的传入参数，此时表示着`trapframe`。如果在恢复过程中把`a0`也恢复了，那么后续恢复就不能正常完成。

需要恢复的`a0`此时装载在`112(a0)`的位置，并在前面(L92-L93)被放入了`sscratch`中。当`trapframe`中的其它内容都已经被恢复出来之后，L128从`sscratch`中恢复了`a0`。



4.

L132:`sret`

在此之前，`os/trap.c` L115完成了`sstatus`的设置。因此在执行`sret`时，会切换到`U`特权级并继续用户态的执行。



5.

`csrrw a0, sscratch, a0` 这条指令会交换`a0`与`sscratch`的值。

在它执行之前，`sscratch`指向`trapframe`的位置，而`a0`则是用户态代码正常执行的一部分，是需要保存的现场。

这一句执行之后，需要保存的原`a0`被存放在`sscratch`中，而`a0`则指向`trapframe`。



6.对照这一段汇编与`os/trap.h`中`struct trapframe`的定义可知，它从第六项开始依次进行保存，并且跳过了`a0`。

需要保存的“`a0`”此时已经被存放在`sscratch`中，并在L65被存进了`trapframe`中；`epc`则由`trap.c`的L107写入了`sepc`中，并在L68存入`trapframe`。剩下的几项在`trap.c`的L101-L105被填写完整。

其中，`a0`是因为被用作访问`trapframe`，所以不能直接从寄存器存入。而另外几项则是在`usertrapret`中已经被填写，在用户态程序运行过程中不会发生改变，因此不需要重新填入。



7.

并没有特定的指令：当cpu认为需要进入`trap`时，就会进入`S`特权级并跳转到`stvec`指向的位置。而`usertrapret()`中通过`set_usertrap`，将`stvec`指向`trampoline.S`中定义的`uservec`，因此可以完成跳转。

当程序主动进行系统调用触发`trap`时，会通过`sbi.c`中定义的`sbi_call`函数L19-L22的`ecall`触发`trap`完成特权级的切换。



8.

`16(a0)`处是`trapframe`中的`kernel_trap`条目。

在程序执行到这里（在`uservec`中）时，它指向`os/trap.c`中`usertrap()`函数的入口地址。因此，在下一行(L76)，程序就会跳转到`usertrap()`进行执行。










