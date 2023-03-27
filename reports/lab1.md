## Chapter 3 实验报告

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














