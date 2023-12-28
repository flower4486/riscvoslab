## lab4

2113297 杨昕怡   2112075 李晓彤   2112100吴维翰
November 2023

### 1 练习 0

已成功填写 lab1、lab2、lab3 的代码。

### 2 练习 1

​     alloc_proc 函数（位于 kern/process/proc.c 中）负责分配并返回一个新的 struct proc_struct 结构，用于存储新建立的内核线程的管理信息。ucore需要对这个结构进行最基本的初始化。在 alloc_proc 函数的实现中，需要初始化的 proc_struct 结构中的成员变量至少包括：**state/pid/runs/kstack/need_resched/parent/mm/context/tf/cr3/flags/name。**

#### 2.1 设计实现

​    设计实现过程：实现内核线程首先需要给线程创建一个进程，于是我们需要给进程控制块指针（struct proc_struct* proc）初始化分配内存空间，而进程控制块指针中包含如下变量

**•state**：进程状态，proc.h 中定义了四种状态：创建（UNINIT）、睡眠（SLEEPING）、就绪（RUNNABLE）、退出（ZOMBIE，等待父进程回收其资源）

**• pid**：进程 ID，调用本函数时尚未指定，默认值设为-1

**• need_resched**：标志位，表示该进程是否需要重新参与调度以释放CPU，初值 0（false，表示不需要）

**• need_resched**：标志位，表示该进程是否需要重新参与调度以释放CPU，初值 0（false，表示不需要）

**• parent**：父进程控制块指针，初值 NULL

**• mm**：用户进程虚拟内存管理单元指针，由于系统进程没有虚存，其值为 NULL

**• context**：进程上下文，默认值全零

**• tf**：中断帧指针，默认值 NULL

**• cr3**：该进程页目录表的基址寄存器，初值为 ucore 启动时建立好的内核虚拟空间的页目录表首地址 boot_cr3（在 kern/mm/pmm.c 的pmm_init 函数中初始化）

**• flags**：进程标志位，默认值 0

**• name**：进程名数组

代码具体实现为：

    alloc_proc(void) {
        struct proc_struct *proc = kmalloc(sizeof(struct proc_struct));
        if (proc != NULL) {
            proc->state = PROC_UNINIT;
        	proc->pid = -1;
        	proc->runs = 0;
        	proc->kstack = NULL;
        	proc->need_resched = 0;
        	proc->parent = NULL;
        	proc->mm = NULL;
        	memset(&(proc->context),0,sizeof(struct context));
        	proc->tf = NULL;
        	proc->cr3 = boot_cr3;
        	proc->flags = 0;
        	memset(proc->name,0,PROC_NAME_LEN + 1);
    }
    return proc;



#### 2.2 补充问题

​    请说明 proc_struct 中 struct context context 和 struct trapframe *tf 成员变量含义和在本实验中的作用是啥？

**问题回答：**

  context 中保存了进程执行的上下文，其中包含了 ra，sp，s0 s11 共 1个寄存器，也就是几个关键寄存器的值。这些寄存器的值用于在进程切换中还原之前进程的运行状态（进程切换的详细过程在后面会介绍）。

  tf 是中断帧的指针，总是指向内核栈的某个位置：当进程从用户态转移到内核态时，中断帧 tf 记录了进程在被中断前的状态, 比如部分寄存器的值。当内核需要跳回用户态时，需要调整中断帧以恢复让进程继续执行的各寄存器值。tf 里保存了进程的中断帧。当进程从用户空间跳进内核空间的时候，进程的执行状态被保存在了中断帧中（注意这里需要保存的执行状态数量不同于上下文切换）。系统调用可能会改变用户寄存器的值，我们可以通过调整中断帧来使得系统调用返回特定的值。

### 3 练习 2

#### 3.1 设计实现

​     创建一个内核线程需要分配和设置好很多资源。kernel_thread 函数通过调用 do_fork 函数完成具体内核线程的创建工作。do_kernel 函数会调用alloc_proc 函数来分配并初始化一个进程控制块，但alloc_proc 只是找到了一小块内存用以记录进程的必要信息，并没有实际分配这些资源。ucore一般通过 do_fork 实际创建新的内核线程。do_fork 的作用是，创建当前内核线程的一个副本，它们的执行上下文、代码、数据都一样，但是存储位置不同。因此，我们实际需要”fork” 的东西就是 stack 和 trapframe。在这个过程中，需要给新内核线程分配资源，并且复制原进程的状态。你需要完成在 kern/process/proc.c 中的 do_fork 函数中的处理过程。

它的大致执行步骤包括：

​    调用 alloc_proc，首先获得一块用户信息块。

​    为进程分配一个内核栈。

​    返回新进程号- 复制原进程的内存管理信息到新进程（但内核线程不必做此事）

​     复制原进程上下文到新进程

​     将新进程添加到进程列表

​     唤醒新进程

```
int`
`do_fork(uint32_t clone_flags, uintptr_t stack, struct trapframe *tf) {`
    `int ret = -E_NO_FREE_PROC;`
    `struct proc_struct *proc;`
    `if (nr_process >= MAX_PROCESS) {`
        `goto fork_out;`
    `}`
    `ret = -E_NO_MEM;`
    `//LAB4:EXERCISE2` 
    `proc = alloc_proc();`
    `if (proc == NULL)`
    	`goto fork_out;`
    `//    2. call setup_kstack to allocate a kernel stack for child process`
    `ret = setup_kstack(proc);`
    `if (ret != 0)`
    	`goto fork_out;`
    `//    3. call copy_mm to dup OR share mm according clone_flag`
    `ret = copy_mm(clone_flags, proc);`
    `if (ret != 0)`
    	`goto fork_out;`
    `//    4. call copy_thread to setup tf & context in proc_struct`
    `copy_thread(proc, stack, tf);`
    `proc->pid = get_pid();`
    `//    5. insert proc_struct into hash_list && proc_list`
    `hash_proc(proc);`
    `list_add(&proc_list,&(proc->list_link));`
    `//    6. call wakeup_proc to make the new child process RUNNABLE`
    `wakeup_proc(proc);`
    `//    7. set ret vaule using child proc's pid`
    `ret = proc->pid;`

`fork_out:`
    `return ret;`

`bad_fork_cleanup_kstack:`
    `put_kstack(proc);`
`bad_fork_cleanup_proc:`
    `kfree(proc);`
    `goto fork_out;`
`}
```



#### 3.2 补充问题

​    请说明 ucore 是否做到给每个新 fork 的线程一个唯一的 id？请说明你的分析和理由。

**问题回答：**

​    线程的pid由get_pid()函数分配，函数中包含了静态变量last_pid和next_safe,last_pid保存了上次分配的pid，而next_safe和last_pid一起表示了一段可用的pid范围，函数中首先初始化这两个变量是MAX_PID，每次调用get_pid()函数时，除了确定一个可以分配的pid，还需要确认next_safe来实现均摊，并且确认pid后会检查所有进程的pid来确保pid没有出现重复，保证唯一性。

### 4 练习 3

#### 4.1 设计实现

proc_run 用于将指定的进程切换到 CPU 上运行。它的大致执行步骤包括：

​    检查要切换的进程是否与当前正在运行的进程相同，如果相同则不需要切换。

​    禁用中断。你可以使用/kern/sync/sync.h 中定义好的宏 local_intr_save(x)和 local_intr_restore(x) 来实现关、开中断。切换当前进程为要运行的进程。

​    切换页表，以便使用新进程的地址空间。/libs/riscv.h 中提供了 lcr3(unsignedint cr3) 函数，可实现修改 CR3 寄存器值的功能。

​     实现上下文切换。/kern/process 中已经预先编写好了 switch.S，其中定义了 switch_to() 函数。可实现两个进程的 context 切换。允许中断。

```
 void proc_run ( struct proc_struct *proc) {
 if (proc != current ) {
   bool intr_flag ;
   struct proc_struct *prev = current , *next = proc;
   local_intr_save ( intr_flag );{
      // 实 现 切 换 进 程
     current = proc;
     lcr3(next ->cr3);
     switch_to (&( prev -> context ), &(next -> context ));
     }
    local_intr_restore ( intr_flag );
    }
 }
```



#### 4.2 补充问题

在本实验的执行过程中，创建且运行了几个内核线程？

**问题回答：**

 2 个；

  idleproc 第一个内核进程，完成内核中各个子系统的初始化，之后立即调度，执行其他进程

  initproc：用于完成实验的功能而调度的内核进程。

### 5 挑战 1

说明语句 local_intr_save(intr_flag);....local_intr_restore(intr_flag);是如何实现开关中断的？

**问题回答：**

sstatus 寄存器 (Supervisor Status Register) 里面有一个二进制位 SIE(supervisor interrupt enable，在 RISCV 标准里是是2^1 对应的二进制位)，数值为0的时候，如果当程序在S态运行，将禁用全部中断。所以实现禁止中断就是使得SIE为0，恢复就是使得SIE为1。

 intr_disable函数实现的就是SIE置0（即禁止中断），intr_enable实现的就是SIE置1（即恢复中断）。

```
/* intr_enable - enable irq interrupt */
void intr_enable(void) { set_csr(sstatus, SSTATUS_SIE); }

/* intr_disable - disable irq interrupt */
void intr_disable(void) { clear_csr(sstatus, SSTATUS_SIE); }
```

​    __intr_save实现的就是我们这里的保存中断，先判断我们的SSTATUS_SIE是否为1，如果是我们就调用intr_disable禁止中断，并返回1，最终保存在bool变量intr_flag里，这个标志就是判断我们是否在这里真正的把中断禁止了，如果没有调用intr_disable函数即本身就是禁止中断的，我们就返回0。        __intr_restore函数就是恢复中断，他会根据intr_flag来判断是否恢复中断，如果原来调用了 intr_disable()函数我们就用intr_enable()恢复，如果原来就是禁止中断的，我们就不必恢复。

这里的参数的作用就是来恢复到我们原来的中断，如果是本身是禁止中断的，这边什么都不需要操作，一直保持这种状态就很好，否则我们就要在需要原子操作的时候暂时性的禁止中断。

```
static inline bool __intr_save(void) {//保存中断
    if (read_csr(sstatus) & SSTATUS_SIE) {//如果中断使能
        intr_disable();//禁止中断
        return 1;
    }
    return 0;
}

static inline void __intr_restore(bool flag) {
    if (flag) {
        intr_enable();//恢复中断
    }
}

#define local_intr_save(x) \
    do {                   \
        x = __intr_save(); \
    } while (0)
#define local_intr_restore(x) __intr_restore(x);
```

简而言之，这里主要两个功能，一个是保存 sstatus寄存器中的中断使能位(SIE)信息并屏蔽中断的功能，另一个是根据保存的中断使能位信息来使能中断的功能。