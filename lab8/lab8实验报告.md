lab8实验报告

2113297 杨昕怡 2112075 李晓彤 2112100 吴维翰

###  练习0：填写已有实验

#### lab6

首先修改初始化函数

```
static struct proc_struct * alloc_proc(void) {
    struct proc_struct *proc = kmalloc(sizeof(struct proc_struct));
    if (proc != NULL) {
        ...
        // Lab6 code
        proc->rq = NULL; //初始化运行队列为空
        list_init(&(proc->run_link)); 
        proc->time_slice = 0; //初始化时间片
        proc->lab6_run_pool.left = proc->lab6_run_pool.right = proc->lab6_run_pool.parent = NULL;//初始化指针为空
        proc->lab6_stride = 0;    //设置步长为0
        proc->lab6_priority = 0;  //设置优先级为0
    }
    return proc;
}
```

lab6是关于ucore的系统调度器框架，需要基于此实现一个Stride Scheduling调度算法。具体修改主要是：将进程加入就绪队列、将进程从就绪队列中移除、选择进程调度，详细代码如下所示

```
#define BIG_STRIDE  (1 << 30) 

static void stride_init(struct run_queue *rq) {
     list_init(&(rq->run_list));
     rq->lab6_run_pool = NULL;
     rq->proc_num = 0;
}

static void
stride_enqueue(struct run_queue *rq, struct proc_struct *proc) {
    //将进程加入就绪队列
     rq->lab6_run_pool = skew_heap_insert(rq->lab6_run_pool, &(proc->lab6_run_pool), proc_stride_comp_f);
     if (proc->time_slice == 0 || proc->time_slice > rq->max_time_slice) {
        proc->time_slice = rq->max_time_slice;
     }
     proc->rq = rq;
     rq->proc_num++;
}

static void
stride_dequeue(struct run_queue *rq, struct proc_struct *proc) {
     assert(proc->rq == rq && rq->proc_num > 0);
     rq->lab6_run_pool = skew_heap_remove(rq->lab6_run_pool, &(proc->lab6_run_pool), proc_stride_comp_f);
     rq->proc_num--;
}

static struct proc_struct* stride_pick_next(struct run_queue *rq) {
     if (rq->lab6_run_pool == NULL) {
        return NULL;
     }
     struct proc_struct* proc = le2proc(rq->lab6_run_pool, lab6_run_pool);
   
     if (proc->lab6_priority != 0) {//优先级不为0
        //步长设置为优先级的倒数
        proc->lab6_stride += BIG_STRIDE / proc->lab6_priority;
     } 
     else {
        //步长设置为最大值
        proc->lab6_stride += BIG_STRIDE;
     }
     return proc;
}

static void
stride_proc_tick(struct run_queue *rq, struct proc_struct *proc) {
     if (proc->time_slice > 0) {//到达时间片
        proc->time_slice--;//执行进程的时间片减一
     }
     if (proc->time_slice == 0) {//时间片为0
        //设置此进程成员变量need_resched标识为1,进程需要调度
        proc->need_resched = 1;
     }
}
```

#### lab7

lab7是实现ucore中同步互斥机制

首先是基于信号量实现完成条件变量实现，cond_signal用于等待某一个条件，cond_wait提醒某一个条件已经达成。

```
void cond_signal (condvar_t *cvp) {
   cprintf("cond_signal begin: cvp %x, cvp->count %d, cvp->owner->next_count %d\n", cvp, cvp->count, cvp->owner->next_count);
     if(cvp->count>0) { //当前存在执行cond_wait而睡眠的进程 
        cvp->owner->next_count ++; //睡眠的进程总个数加一  
        up(&(cvp->sem)); //唤醒等待在cv.sem上睡眠的进程 
        down(&(cvp->owner->next)); //自己需要睡眠
        cvp->owner->next_count --; //睡醒后等待此条件的睡眠进程个数减一
      }
   cprintf("cond_signal end: cvp %x, cvp->count %d, cvp->owner->next_count %d\n", cvp, cvp->count, cvp->owner->next_count);
}

void cond_wait (condvar_t *cvp) {
    cprintf("cond_wait begin:  cvp %x, cvp->count %d, cvp->owner->next_count %d\n", cvp, cvp->count, cvp->owner->next_count);
      cvp->count++; //需要睡眠的进程个数加一
      if(cvp->owner->next_count > 0) 
         up(&(cvp->owner->next)); //唤醒进程链表中的下一个进程
      else
         up(&(cvp->owner->mutex)); //唤醒睡在monitor.mutex上的进程 
      down(&(cvp->sem));  //将此进程等待  
      cvp->count --;  //睡醒后等待此条件的睡眠进程个数减一
    cprintf("cond_wait end:  cvp %x, cvp->count %d, cvp->owner->next_count %d\n", cvp, cvp->count, cvp->owner->next_count);
}
```

然后是用管程机制解决哲学家就餐问题

哲学家先尝试获取刀叉，如果刀叉没有获取到，则等待刀叉。

```
void phi_take_forks_condvar(int i) {
    down(&(mtp->mutex));  //通过P操作进入临界区
    state_condvar[i]=HUNGRY; //记录哲学家i是否饥饿
    phi_test_condvar(i);   //试图拿到叉子 
    if (state_condvar[i] != EATING) {
        cprintf("phi_take_forks_condvar: %d didn't get fork and will wait\n",i);
        cond_wait(&mtp->cv[i]); //得不到叉子就睡眠
    }
    if(mtp->next_count>0)  //如果存在睡眠的进程则那么将之唤醒
        up(&(mtp->next));
    else
        up(&(mtp->mutex));
}
```

当哲学家放下刀叉时，如果左右两边的哲学家都满足条件可以进餐，则设置对应的条件变量。

```
void phi_put_forks_condvar(int i) {
    down(&(mtp->mutex)); ;//通过P操作进入临界区
    state_condvar[i]=THINKING; //记录进餐结束的状态
    phi_test_condvar(LEFT); //看一下左边哲学家现在是否能进餐
    phi_test_condvar(RIGHT); //看一下右边哲学家现在是否能进餐
    if(mtp->next_count>0) //如果有哲学家睡眠就予以唤醒
        up(&(mtp->next));
    else
        up(&(mtp->mutex)); //离开临界区
}
```

###  练习1: 完成读文件操作的实现（需要编码）

####  读文件过程

用户进程调用下面的`read`函数，也就是进入通用文件访问接口层的处理流程，进一步调用如下用户态函数：`read->sys_read->syscall`，进入系统调用那些。

```
int
read(int fd, void *base, size_t len) {
    return sys_read(fd, base, len);
}

int
sys_read(int64_t fd, void *base, size_t len) {
    return syscall(SYS_read, fd, base, len);
}
```

文件系统抽象层内核函数sysfile_read主要可以看成三个步骤：

1. 测试当前待读取的文件是否存在读权限
2. 在内核中创建一块缓冲区。
3. 循环执行file_read函数读取数据至缓冲区中，并将该缓冲区中的数据复制至用户内存（即传入sysfile_read的base指针所指向的内存）

```
int
sysfile_read(int fd, void *base, size_t len) {
    struct mm_struct *mm = current->mm;
    if (len == 0) {//如果读取长度为0，直接返回
        return 0;
    }
    if (!file_testfd(fd, 1, 0)) {//是否可读
        return -E_INVAL;
    }
    void *buffer;
    if ((buffer = kmalloc(IOBUF_SIZE)) == NULL) {//分配4096的buffer
        return -E_NO_MEM;
    }

    int ret = 0;
    size_t copied = 0, alen;
    while (len != 0) {//循环读取
        if ((alen = IOBUF_SIZE) > len) {
            alen = len;
        }
        ret = file_read(fd, buffer, alen, &alen);//读取文件,alen为实际读取长度
        if (alen != 0) {
            lock_mm(mm);
            {
                if (copy_to_user(mm, base, buffer, alen)) {//将读取的内容拷贝到用户空间
                    assert(len >= alen);
                    base += alen, len -= alen, copied += alen;//更新base,len,copied
                }
                else if (ret == 0) {
                    ret = -E_INVAL;
                }
            }
            unlock_mm(mm);
        }
        if (ret != 0 || alen == 0) {
            goto out;
        }
    }

out:
    kfree(buffer);
    if (copied != 0) {
        return copied;
    }
    return ret;
}
```

file_read函数会先初始化一个IO缓冲区，并执行vop_read函数将数据读取至缓冲区中。

`sfs_io_nolock`函数主要实现的就是对设备上基础块数据的读取与写入。从参数角度来看就是从偏移位置offset到offset+长度length读取/写入文件内容，磁盘块<-->缓冲区（内存中）的功能。

```
static int
sfs_io_nolock(struct sfs_fs *sfs, struct sfs_inode *sin, void *buf, off_t offset, size_t *alenp, bool write) {
    struct sfs_disk_inode *din = sin->din;
    assert(din->type != SFS_TYPE_DIR);
    // 计算缓冲区读取/写入的终止位置
    off_t endpos = offset + *alenp, blkoff;
    *alenp = 0;
    //处理一些特殊情况
    if (offset < 0 || offset >= SFS_MAX_FILE_SIZE || offset > endpos) {
        return -E_INVAL;
    }
    if (offset == endpos) {
        return 0;
    }
    if (endpos > SFS_MAX_FILE_SIZE) {
        endpos = SFS_MAX_FILE_SIZE;
    }
    if (!write) {
        if (offset >= din->size) {
            return 0;
        }
        if (endpos > din->size) {
            endpos = din->size;
        }
    }
    // 根据不同的执行函数，设置对应的函数指针
    int (*sfs_buf_op)(struct sfs_fs *sfs, void *buf, size_t len, uint32_t blkno, off_t offset);
    int (*sfs_block_op)(struct sfs_fs *sfs, void *buf, uint32_t blkno, uint32_t nblks);
    if (write) {
        sfs_buf_op = sfs_wbuf, sfs_block_op = sfs_wblock;
    }
    else {
        sfs_buf_op = sfs_rbuf, sfs_block_op = sfs_rblock;
    }    
    int ret = 0;
    size_t size, alen = 0;//alen表示实际读取/写入的长度
    uint32_t ino;
    uint32_t blkno = offset / SFS_BLKSIZE;          // 块的编号
    uint32_t nblks = endpos / SFS_BLKSIZE - blkno;  // 需要读取/写入的块数
    if ((blkoff = offset % SFS_BLKSIZE) != 0) {
        /*
        计算了offset相对于SFS_BLKSIZE的余数，并将结果赋值给blkoff变量。
        SFS_BLKSIZE是一个常量，表示文件系统中的块大小。
        如果offset不是SFS_BLKSIZE的倍数，即存在一个不完整的块，那么blkoff将不为零。
        */
        size = (nblks != 0) ? (SFS_BLKSIZE - blkoff) : (endpos - offset);//读取/写入的大小
        // 获取第一个基础块所对应的block的编号`ino`
        if ((ret = sfs_bmap_load_nolock(sfs, sin, blkno, &ino)) != 0) {
            goto out;
        }
        // 通过上一步取出的`ino`，读取/写入一部分第一个基础块的末尾数据
        if ((ret = sfs_buf_op(sfs, buf, size, ino, blkoff)) != 0) {
            goto out;
        }

        alen += size;
        buf += size;

        if (nblks == 0) {
            goto out;
        }

        blkno++;
        nblks--;
    }
    if (nblks > 0) {
    //获取inode对应的基础块编号
        if ((ret = sfs_bmap_load_nolock(sfs, sin, blkno, &ino)) != 0) {
            goto out;
        }
        // 单次读取/写入一基础块的数据
        if ((ret = sfs_block_op(sfs, buf, ino, nblks)) != 0) {
            goto out;
        }

        alen += nblks * SFS_BLKSIZE;
        buf += nblks * SFS_BLKSIZE;
        blkno += nblks;
        nblks -= nblks;
    }
    // 3. 如果结束位置不与最后一个块对齐，则最后一个块的开始读取/写入一些内容，直到最后一个块的 (endpos % SFS_BLKSIZE) 处
    if ((size = endpos % SFS_BLKSIZE) != 0) {
        if ((ret = sfs_bmap_load_nolock(sfs, sin, blkno, &ino)) != 0) {
            goto out;
        }
        if ((ret = sfs_buf_op(sfs, buf, size, ino, 0)) != 0) {
            goto out;
        }
        alen += size;
    }
out:
    *alenp = alen;
    if (offset + alen > sin->din->size) {
        sin->din->size = offset + alen;
        sin->dirty = 1;
    }
    return ret;
}
```

####  文件写过程

流程和读过程类似

#### 文件打开过程

打开文件的过程就是:给当前进程分配一个新的文件,将这个文件与参数中 path 对应的 inode 关联起来.

用户进程调用open函数时，open()->sys_open->sys_call通过系统中断进入内核态，系统调用sys_open()->sysfile_open()函数。 在sysfile_open()函数里面需要把位于用户空间的字符串__path拷贝到内核空间中的字符串path中，然后调用了file_open（path, open_flags)，file_open里面调用了vfs_open, 使用了VFS的接口。

在file_open函数中，程序主要做了以下几个操作：

- 在当前进程的文件管理结构filesp中，分配一个空闲的file对象。
- 调用vfs_open函数，并存储该函数返回的inode结构
- 根据上一步返回的inode，设置file对象的属性。如果打开方式是append，则还会设置file的pos成员为当前文件的大小。
- 最后返回file->fd

```
// open file
int
file_open(char *path, uint32_t open_flags) {
    bool readable = 0, writable = 0;
    switch (open_flags & O_ACCMODE) {
    case O_RDONLY: readable = 1; break;
    case O_WRONLY: writable = 1; break;
    case O_RDWR:
        readable = writable = 1;
        break;
    default:
        return -E_INVAL;
    }
    int ret;
    struct file *file;
    if ((ret = fd_array_alloc(NO_FD, &file)) != 0) { //在当前进程分配file descriptor
        return ret;
    }
    struct inode *node;
    if ((ret = vfs_open(path, open_flags, &node)) != 0) {//打开文件的工作在vfs_open完成
        fd_array_free(file);//打开失败，释放file descriptor
        return ret;
    }
    file->pos = 0;
    if (open_flags & O_APPEND) {
        struct stat __stat, *stat = &__stat;
        if ((ret = vop_fstat(node, stat)) != 0) {
            vfs_close(node);
            fd_array_free(file);
            return ret;
        }
        file->pos = stat->st_size;//追加写模式，设置当前位置为文件尾
    }
    file->node = node;//current->fs_struct->filemap[fd]
    file->readable = readable;
    file->writable = writable;
    fd_array_open(file);//设置该文件的状态为“打开”
    return file->fd;
}
```

然后用vfs_open函数来找到path指出的文件所对应的基于inode数据结构的VFS索引节点node。

###  练习2: 完成基于文件系统的执行程序机制的实现（需要编码）

需要初始化进程控制结构中的fs，添加对`proc->filesp`的初始化。

```
static proc_struct* alloc_proc(void) {
    struct proc_struct *proc = kmalloc(sizeof(struct proc_struct));
    if (proc != NULL) {
        // ...
        //LAB8:EXERCISE2 YOUR CODE HINT:need add some code to init fs in proc_struct, ...
        proc->filesp = NULL;
    }
    return proc;
}
```

`load_icode`函数通过ELF文件的文件描述符调用`load_icode_read`函数来进行解析程序

```
static int
load_icode(int fd, int argc, char **kargv) {
    assert(argc >= 0 && argc <= EXEC_MAX_ARG_NUM);
    //(1)建立内存管理器
    if (current->mm != NULL) {    //要求当前内存管理器为空
        panic("load_icode: current->mm must be empty.\n");
    }

    int ret = -E_NO_MEM;    // E_NO_MEM代表因为存储设备产生的请求错误
    struct mm_struct *mm;  //建立内存管理器
    if ((mm = mm_create()) == NULL) {
        goto bad_mm;
    }

    //(2)建立页目录
    if (setup_pgdir(mm) != 0) {
        goto bad_pgdir_cleanup_mm;
    }
    struct Page *page;//建立页表

    //(3)从文件加载数据到内存
    //Lab8 这里要从文件中读取ELF文件头，而不是原先的内存
    struct elfhdr __elf, *elf = &__elf;
    if ((ret = load_icode_read(fd, elf, sizeof(struct elfhdr), 0)) != 0) {//读取elf文件头
        goto bad_elf_cleanup_pgdir;           
    }
    // 判断读取入的elf header是否正确
    if (elf->e_magic != ELF_MAGIC) {
        ret = -E_INVAL_ELF;
        goto bad_elf_cleanup_pgdir;
    }
    // 根据每一段的大小和基地址来分配不同的内存空间
    struct proghdr __ph, *ph = &__ph;
    uint32_t vm_flags, perm, phnum;
    for (phnum = 0; phnum < elf->e_phnum; phnum ++) {  //e_phnum代表程序段入口地址数目，即多少个段
        //Lab8 计算elf程序每个段头部的偏移
        off_t phoff = elf->e_phoff + sizeof(struct proghdr) * phnum;     
        //Lab8 从每个段头部的偏移处循环读取每个段的详细信息（包括大小、基地址等）到ph中
        if ((ret = load_icode_read(fd, ph, sizeof(struct proghdr), phoff)) != 0) {
            goto bad_cleanup_mmap;
        }
        if (ph->p_type != ELF_PT_LOAD) {
            continue ;
        }
        if (ph->p_filesz > ph->p_memsz) {
            ret = -E_INVAL_ELF;
            goto bad_cleanup_mmap;
        }
        if (ph->p_filesz == 0) {
            continue ;
        }
        vm_flags = 0, perm = PTE_U;
        if (ph->p_flags & ELF_PF_X) vm_flags |= VM_EXEC;
        if (ph->p_flags & ELF_PF_W) vm_flags |= VM_WRITE;
        if (ph->p_flags & ELF_PF_R) vm_flags |= VM_READ;
        if (vm_flags & VM_WRITE) perm |= PTE_W;
        //为当前段分配内存空间（设置vma）
        if ((ret = mm_map(mm, ph->p_va, ph->p_memsz, vm_flags, NULL)) != 0) {
            goto bad_cleanup_mmap;
        }
        off_t offset = ph->p_offset;
        size_t off, size;
        uintptr_t start = ph->p_va, end, la = ROUNDDOWN(start, PGSIZE);

        ret = -E_NO_MEM;

        //复制数据段和代码段
        end = ph->p_va + ph->p_filesz;      //计算数据段和代码段终止地址
        while (start < end) {       
            // 为la分配内存页并设置该内存页所对应的页表项        
            if ((page = pgdir_alloc_page(mm->pgdir, la, perm)) == NULL) {
                ret = -E_NO_MEM;
                goto bad_cleanup_mmap;
            }
            off = start - la, size = PGSIZE - off, la += PGSIZE;
            if (end < la) {
                size -= la - end;
            }
            //Lab8 读取elf对应段内的数据到该内存页中，每次读取size大小的块，直至全部读完
            if ((ret = load_icode_read(fd, page2kva(page) + off, size, offset)) != 0) {       
                goto bad_cleanup_mmap;
            }
            start += size, offset += size;
        }
        //建立BSS段
        end = ph->p_va + ph->p_memsz;   //同样计算终止地址
        // 对于段中当前页中剩余的空间（复制elf数据后剩下的空间），将其置为0
        if (start < la) {     
            if (start == end) {   
                continue ;
            }
            off = start + PGSIZE - la, size = PGSIZE - off;
            if (end < la) {
                size -= la - end;
            }
            memset(page2kva(page) + off, 0, size);
            start += size;
            assert((end < la && start == end) || (end >= la && start == la));
        }
        // 对于段中剩余页中的空间（复制elf数据后的多余页面），将其置为0
        while (start < end) {
            if ((page = pgdir_alloc_page(mm->pgdir, la, perm)) == NULL) {
                ret = -E_NO_MEM;
                goto bad_cleanup_mmap;
            }
            off = start - la, size = PGSIZE - off, la += PGSIZE;
            if (end < la) {
                size -= la - end;
            }
            //每次操作size大小的块
            memset(page2kva(page) + off, 0, size);
            start += size;
        }
    }
    sysfile_close(fd);//关闭文件，加载程序结束

    //(4)建立相应的虚拟内存映射表
    vm_flags = VM_READ | VM_WRITE | VM_STACK;
    if ((ret = mm_map(mm, USTACKTOP - USTACKSIZE, USTACKSIZE, vm_flags, NULL)) != 0) {
        goto bad_cleanup_mmap;
    }
    assert(pgdir_alloc_page(mm->pgdir, USTACKTOP-PGSIZE , PTE_USER) != NULL);
    assert(pgdir_alloc_page(mm->pgdir, USTACKTOP-2*PGSIZE , PTE_USER) != NULL);
    assert(pgdir_alloc_page(mm->pgdir, USTACKTOP-3*PGSIZE , PTE_USER) != NULL);
    assert(pgdir_alloc_page(mm->pgdir, USTACKTOP-4*PGSIZE , PTE_USER) != NULL);
    //(5)设置当前进程的mm、sr3，并设置CR3寄存器等于页目录表的物理地址
    current->mm = mm;
    current->cr3 = PADDR(mm->pgdir);
    lcr3(PADDR(mm->pgdir));

    //(6)Lab8 设置用户栈，处理用户栈中传入的参数，其中argc为参数个数，kargv存储命令行参数，uargv存储用户参数
    uint32_t argv_size=0, i;
    for (i = 0; i < argc; i ++) {//计算所有参数的总长度
        argv_size += strnlen(kargv[i],EXEC_MAX_ARG_LEN + 1)+1;
    }

    uintptr_t stacktop = USTACKTOP - (argv_size/sizeof(long)+1)*sizeof(long);//开辟用户栈空间
    // 计算uargv在用户栈的位置
    char** uargv=(char **)(stacktop  - argc * sizeof(char *));

    argv_size = 0;
    for (i = 0; i < argc; i ++) {//将所有参数取出来放置uargv
        uargv[i] = strcpy((char *)(stacktop + argv_size ), kargv[i]);
        argv_size +=  strnlen(kargv[i],EXEC_MAX_ARG_LEN + 1)+1;
    }

    stacktop = (uintptr_t)uargv - sizeof(int);//在用户栈中开辟一个整数的空间存储argc
    *(int *)stacktop = argc;//将argc存储在栈顶位置
    //(7)设置进程的中断帧   
    struct trapframe *tf = current->tf;     
    memset(tf, 0, sizeof(struct trapframe));//清空进程的中断帧
    tf->tf_cs = USER_CS;      
    tf->tf_ds = tf->tf_es = tf->tf_ss = USER_DS;
    tf->tf_esp = stacktop;
    tf->tf_eip = elf->e_entry;
    tf->tf_eflags = FL_IF;
    ret = 0;
    //(8)错误处理部分
out:
    return ret;           //返回
bad_cleanup_mmap:
    exit_mmap(mm);
bad_elf_cleanup_pgdir:
    put_pgdir(mm);
bad_pgdir_cleanup_mm:
    mm_destroy(mm);
bad_mm:
    goto out;
}
```

load_icode_read函数从文件描述符fd指向的文件中读取数据到buf中。首先调用`sysfile_seek`函数将文件指针移动到指定的偏移量处。如果`sysfile_seek`返回错误码，则`load_icode_read`直接返回该错误码。接着，函数调用`sysfile_read`函数从文件中读取指定长度的数据到buf中。如果读取的数据长度不等于要求的长度len，则`load_icode_read`返回错误码-1；如果读取过程中发生了错误，则`load_icode_read`返回`sysfile_read`函数的错误码。最后，如果函数执行成功，则load_icode_read返回0。

```
//kern/fs/sysfile.c
int
sysfile_seek(int fd, off_t pos, int whence) {
    return file_seek(fd, pos, whence);
}

//kern/fs/file.c
int
file_seek(int fd, off_t pos, int whence) {
    struct stat __stat, *stat = &__stat;
    int ret;
    struct file *file;
    if ((ret = fd2file(fd, &file)) != 0) {
        return ret;
    }
    fd_array_acquire(file);

    switch (whence) {
    case LSEEK_SET: break;
    case LSEEK_CUR: pos += file->pos; break;
    case LSEEK_END:
        if ((ret = vop_fstat(file->node, stat)) == 0) {
            pos += stat->st_size;
        }
        break;
    default: ret = -E_INVAL;
    }

    if (ret == 0) {
        if ((ret = vop_tryseek(file->node, pos)) == 0) {
            file->pos = pos;
        }
//    cprintf("file_seek, pos=%d, whence=%d, ret=%d\n", pos, whence, ret);
    }
    fd_array_release(file);
    return ret;
}
```

