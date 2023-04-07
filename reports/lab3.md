# chapter4 练习

王哲威（[https://github.com/LearningOS/2023s-ucore-6ziv](https://github.com/LearningOS/2023s-ucore-6ziv)）

#### 编程作业1:spawn

我们在`proc.c`中声明函数`int spawn(char *name)`。

首先，获取需要执行的程序：

```c
int id = get_id_by_name(name);
if (id < 0) {
    return -1;
}
```

然后，初始化要创建的进程：

```c
struct proc *np;
struct proc *p = curr_proc();
// Allocate process.
if ((np = allocproc()) == 0) {
	return -1;
}
np->parent = p;
np->state = RUNNABLE;
np->max_page = 0;
loader(id, np);
add_task(np);
return np->pid;
```

最后，声明系统调用`sys_spawn`：

```c
uint64 sys_spawn(uint64 va)
{
	struct proc *p = curr_proc();
	char name[200];
	copyinstr(p->pagetable, name, va, 200);
	return spawn(name);
}
```



#### 编程作业2：stride调度算法

在`proc.h`中，`struct proc`的定义中添加相关字段：`uint64 stride`和`uint64 pass`，并定义`#define BIG_STRIDE (65536)`。

我们不再需要用队列进行调度。因此，将`proc.c`中的`struct queue task_queue;`，以及`proc_init()`、`fetch_task()`、`add_task()`中的相关部分注释掉。

我们重新实现`fetch_task()`：每次遍历`pool`中的所有状态为`RUNNABLE`的进程，取出其中`stride`最小的那一个。

根据思考题可以知道，进程`stride`之间的差不超过`BIG_STRIDE/2`，更是远远小于`UINT64_MAX / 2`。于是，对于两个进程`p1`,`p2`，`(p1->stride) < (p2->stride)`等价于：`(uint64)(p1->stride - p2->stride) > UINT64_MAX / 2`。

在选出需要调度的程序后，在它的`stride`上加上它的`pass`即可。

重新实现的`fetch_task()`如下：

```c
struct proc *fetch_task()
{
	struct proc *ret = NULL;
	
	struct proc *p;
	for (p = pool; p < &pool[NPROC]; p++) {
		if (p->state != RUNNABLE)
			continue;

		if (NULL == ret || (uint64)(p->stride - ret->stride) > UINT64_MAX / 2) {
			ret = p;
		}
	}
	if (ret == NULL) {
		debugf("No task to fetch\n");
		return NULL;
	}
	ret->stride += ret->pass;
	return ret;
}
```

而`add_task()`则变成了空函数。



最后，我们实现`sys_set_priority()`。只需要检查`prio`在合法范围内，之后利用公式计算`pass = BIG_STRIDE / prio`即可。

```c
uint64 sys_set_priority(long long prio){
    // TODO: your job is to complete the sys call
	if (prio >= 2 && prio <= INT_MAX) {
		struct proc *p = curr_proc();
		p->pass = BIG_STRIDE / prio;
		return prio;
	}
    return -1;
}
```



#### 问答题：

> stride 算法原理非常简单，但是有一个比较大的问题。例如两个 pass = 10 的进程，使用 8bit 无符号整形储存 stride， p1.stride = 255, p2.stride = 250，在 p2 执行一个时间片后，理论上下一次应该 p1 执行。
> 
> 实际情况是轮到 p1 执行吗？为什么？

 不是，在执行之后`p2.stride`变成了$5$ 。在没有特殊处理的情况下，依然是`p2.stride`更小，下一次还是`p2`执行。



> 我们之前要求进程优先级 >= 2 其实就是为了解决这个问题。可以证明，**在不考虑溢出的情况下**, 在进程优先级全部 >= 2 的情况下，如果严格按照算法执行，那么 STRIDE_MAX – STRIDE_MIN <= BigStride / 2。
> 
> 为什么？尝试简单说明（传达思想即可，不要求严格证明）。

在不考虑溢出的情况下，如果按照算法执行：

那么：

* 对于此时的任意一个进程的stride，都有$STRIDE\_MIN \leqslant stride \leqslant STRIDE\_MAX$。记此时stride最小的进程的pass为$P\leqslant BigStride / 2$，那么在这个进程被调度之后，新的$STRIDE\_MIN' \geqslant STRIDE\_MIN$，$STRIDE\_MAX' = \max(STRIDE\_MAX,STRIDE\_MIN + P）\leqslant  STRIDE\_MIN + BigStride / 2$
  
  也就是说：$STRIDE\_MAX' - STRIDE\_MIN' \leqslant BigStride / 2$
  
  那么根据归纳公理可以知道：如果要证明的公式成立，那么它在运行过程中就一直成立。
  
  如果严格按照算法执行，那么要证明的公式初始就是成立的。所以公式一直成立。
  
  

* 另一方面，如果中间出现了其它情况，导致公式不成立（比如新进程）。
  
  那么在公式不成立期间，每次调度都会使得stride等于调度前的STRIDE_MIN的进程减少。因为总的进程数有限，所以一定会在有限步调度之后，所有进程的stride都大于STRIDE_MIN。
  
  也就是说，一定会在有限步调度之后，使得stride的最小值增加。同时，因为待证明的公式不成立，所以stride的最大值不会增加。也就是说进程的stride的极差一定会在有限步内减少。
  
  因为极差是个有限整数值，所以它一定会在有限步内减少到$BigStride/2$以内。换言之，就算发生了异常，公式也会在有限步调度内重新成立。
  
  

> 已知以上结论，**在考虑溢出的情况下**，假设我们通过逐个比较得到 Stride 最小的进程，请设计一个合适的比较函数，用来正确比较两个 Stride 的真正大小：

考虑溢出就等价于在模$M$的剩余类里讨论问题。

现在假设我们有两个stride，分别记为$s_1,s_2$，那么$|s_1-s_2|\leqslant BigStride/2$。

同时，直接进行带溢出的减法，可以得到：$s1-s2\equiv a (mod\ M)$，其中，$a\in \left[0,m\right)$

也就是说，$s1-s2=a$或者$s1-s2 = a-M$

现在我们想要无歧义，就需要$M>BigStride$。于是，$|a|$和$|a-M|$中，至多只有一个不超过$BigStride/2$。于是我们就可以判断出来$s1-s2$到底是哪一个，也就是知道了$s1-s2$的符号，自然就能够比较$s1$和$s2$的大小。



为了方便起见，我们不使用$BigStride/2$，而是用$M/2$作为标准：同样地，$|a|$和$|a-M|$至多只有一个大于它，而且在$|s_1-s_2|\leqslant BigStride/2$时恰好是一个。

所以，我们只要比较，当$a > M/2$时，$s1<s2$；否则，$s1\geqslant s2$。



更特别地：因为我们每次都按照同样的顺序遍历进程列表，所以即使有添加新的进程导致STRIDE_MAX – STRIDE_MIN <= BigStride / 2不成立，调度器也能够以某个相对固定的方式“切开圆环”，将进程的stride重新归到“环”的同一侧。
