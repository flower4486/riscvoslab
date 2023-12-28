## lab5实验报告

2113297 杨昕怡 2112075 李晓彤 2112100 吴维翰
November 2023

####  练习1.请简要描述这个用户态进程被ucore选择占用CPU执行（RUNNING态）到具体执行应用程序第一条指令的整个经过。

在调用 usermain 创建一个用户态进程，之后通过 loadicode() 加载应用程序后，调度器调用 schedule() 函数实现用户进程的调度，然后设置 current指针是当前执行的 PCB，并且通过 procr un() 函数加载该进程的内核栈和页目录表，其中再调用 switchto() 函数，context 中 eip 设置为 forket，从
而函数返回时会跳转到 forkret 入口点，forket 内部会将栈设置为当前进程的中断帧 trapframe，然后跳转到 t rapret，此时 t rapret 会根据当前进程的中断帧恢复上下文。最后，退出中断的 iret 从系统调用的函数调用路径，返回到用户进程 hello 第一句语句 start 处开始执行。 

#### 练习2: 父进程复制自己的内存空间给子进程（需要编码）

创建子进程的函数`do_fork`在执行中将拷贝当前进程（即父进程）的用户内存地址空间中的合法内容到新进程中（子进程），完成内存资源的复制。具体是通过`copy_range`函数（位于kern/mm/pmm.c中）实现的，请补充`copy_range`的实现，确保能够正确执行。

    int copy_range(pde_t *to, pde_t *from, uintptr_t start, uintptr_t end,
                   bool share) {
                //获父进程页面的内核虚拟地址作为memcpy函数的源地址
                void* src_kvaddr = page2kva(page);
                //获取子进程页面的内核虚拟地址作为memcpy函数的目的地址
                void* dst_kvaddr = page2kva(npage); 
                //复制父进程的页面内容到子进程的页面中
                memcpy(dst_kvaddr, src_kvaddr, PGSIZE);
                //建立子进程页面虚拟地址到物理地址的映射关系
                ret = page_insert(to, npage, start, perm); 
                assert(ret == 0);
        }
        start += PGSIZE;
    } while (start != 0 && start < end);
由上可见，copy_range函数做的就是以页为单位调用memcpy函数，将父进程的用户内存地址空间中的内容逐页复制到子进程中，从而完成内存资源的复制。

***如何设计实现Copy on Write机制？给出概要设计，鼓励给出详细设计。***

Copy-on-write 指如果有多个使用者对一个资源A（比如内存块）进行读操作，则每个使用者只需获得一个指向同一个资源A的指针，就可以获得该资源了。若某使用者需要对这个资源A进行写操作，系统会对该资源进行拷贝操作，从而使得该“写操作”使用者获得一个该资源A的“私有”拷贝—资源B，可对资源B进行写操作。该“写操作”使用者对资源B的改变对于其他的使用者而言是不可见的，因为其他使用者看到的还是资源A。

当COW机制启用时，我们可以将父进程用户内存空间中的页都设置为只读，即将这些页的PTE_W置0，可以通过调用`page_insert`来重新设置这些页的权限位，然后再调用`page_insert`将父进程的页插入到子进程的页目录表中，这样就达到了内存共享的目的。当COW机制禁用时，执行原来的内存拷贝操作。

因为设置了父进程用户内存空间中的所有页为只读，所以在之后的内存操作中，如果有进程对此空间进行写操作，就会触发缺页异常，操作系统就会在相应的中断处理函数`do_pgfault`中复制该页面。

***具体实现***

根据这个思路首先先检查start和end地址是否对齐，并且判断是否再用户地址空间。

```
assert(start % PGSIZE == 0 && end % PGSIZE == 0);
assert(USER_ACCESS(start, end));
```

随后获取页表项，通过get_pte()函数获取源进程中指定的虚拟地址start的页表项PTE，并且复制页面内容，如果PTE已经存在，表明页面已经映射到了源进程，否则分配一个新的页表，即获取源页面结构体指针，并且为目标进程分配一个新的页面，通过memcpy函数复制源页面内容，再通过page_insert函数建立目标进程的页表映射。

```
//完整拷贝内存
void* src_kvaddr = page2kva(page);
void* dst_kvaddr = page2kva(npage); 
memcpy(dst_kvaddr, src_kvaddr, PGSIZE);
ret = page_insert(to, npage, start, perm);
```

再通过循环处理从start到end的虚拟地址范围，逐页进行复制或者共享处理。最后如果操作完成，返回0，否则返回错误代码。

####  练习3: 阅读分析源代码，理解进程执行 fork/exec/wait/exit 的实现，以及系统调用的实现（不需要编码）

请在实验报告中简要说明你对 fork/exec/wait/exit函数的分析。并回答如下问题：

- ***请分析fork/exec/wait/exit的执行流程。重点关注哪些操作是在用户态完成，哪些是在内核态完成？内核态与用户态程序是如何交错执行的？内核态执行结果是如何返回给用户程序的？***

fork：在用户态调用fork，经过一系列流程，会进入内核态执行`sys_fork`函数，`sys_fork`任将tf和sp准备好，调用`do_fork`函数。启用了`copy_mm`的，如果启用`CLONE_VM`，则是`share`模式，相当于浅拷贝，直接将当前的mm指针赋值给新创建的mm指针；但是sys_fork这里传入的参数都是0，也就是`duplicate`模式，相当于深拷贝。

exec：清除了当前进程的内存布局，再通过调用load_icode，读取ELF映像中的内存布局并且填写，保持进程状态不变，

wait： 令当前进程等待某个特定的或任意一个子进程，并在得到子进程的退出消息后，彻底回收子进程所占用的资源，包括子进程的内核栈和进程控制块。只有在`do_wait`函数执行完成之后，子进程的所有资源才被完全释放。

- 查找特定/所有子进程中是否存在某个等待父进程回收的僵尸态（PROC_ZOMBIE）子进程。
  - 如果有，则回收该进程并函数返回。
  - 如果没有，则设置当前进程状态为休眠态（PROC_SLEEPING）并执行`schedule`函数调度其他进程运行。当该进程的某个子进程结束运行后，当前进程会被唤醒，并在`do_wait`函数中回收子进程的PCB内存资源。

exit： 清除当前进程几乎所有资源(PCB和内核栈不清除), 将所有子进程(如果有的话)设置为 init 进程(内核), 将当前进程状态设置为 ZOMBIE; 若有父进程在等待当前进程。

fork,exec,wait和exit中封装了类似于sys_fork这样的系统调用函数，这四个函数操作都是在用户态完成的，而当用户态程序触发ebreak或者ecall中断时，会触发trap从而进入内核态，从而发起syscall系统调用，然后会对系统调用的函数进行分发，然后调用sys_fork等内核态运行的操作。

具体来说，用户态调用fork等函数，并触发trap进入内核态，根据寄存器参数分发相应的函数指针，触发系统调用的相关函数，类似于sys_fork等，调用完成后内核态通过sret返回用户态，实现交错执行。

在这些系统调用的执行流程中，用户态和内核态之间的切换是关键的。当用户程序执行系统调用时，会触发从用户态切换到内核态，让操作系统执行相关的内核代码。在系统调用完成后，操作系统会将控制权切回到用户态，让用户程序继续执行。

- ***请给出 ucore 中一个用户态进程的执行状态生命周期图（包执行状态，执行状态之间的变换关系，以及 产生变换的事件或函数调用）。***

```
alloc_page()--> PROC_UNINIT --wakeup_proc()--> RUNNABLE --> exit() --> PROC_ZOMBIE
                                                   |  |                  ↑ ↑
                                                   ↓  ↓                  | |
                                                do_wait()                | |
                                                   |  |                  ↑ ↑
                                                   ↓  ↓                  | |
                                            PROC_SLEEPING --> exit() --> | |
```

