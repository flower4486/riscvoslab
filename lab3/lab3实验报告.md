#### 练习1：理解基于FIFO的页面替换算法（思考题）
1. trap.c内`exception_handler()`对应缺页异常，进入vmm.c的`dop_default(struct mm_struct *mm, uint_t error_code, uintptr_t addr)`函数,将通过它，进入页表项建立、页swap_out以及新页的swap_in；
2. 首先将 `find_vma(mm, addr)`，找到对应的vma段，得到他的读写属性；之后拼接页表项内容，需要将通过`get_pte(pde_t *pgdir, uintptr_t la, bool create)`，得到pte的地址，从而将拼接的结果写入地址内。(get_pte中，create为1，需要在该虚拟地址指向的某一级页表不存在时候创建，此时`alloc_page`对pd、pt表分配页)
3. 当get_pte发现该页表项是0  说明没有数据写入过
   1. 在get_pte得到pte地址后，需要为其分配`pgdir_alloc_page(pde_t *pgdir, uintptr_t la, uint32_t perm) `对应页，函数需要实现的功能是根据访问的虚拟地址，为他分配一物理页，若当前不存在则换入换出；因此，这个函数内也需要通过`alloc_page()`分配。
   2. `define alloc_page`  是对`alloc_pages(nsize = 1)`的封装，在alloc_pages中，调用`pmm_manager`的分配页面算法，此时页面没空余，则调用sm的置换方法`swap_out(struct mm_struct *mm, int n, int in_tick)`先将内存中的某个页置换出去，再把内存的这一页置换进。
   3. `swap_out()`选择换出的页，使用sm内实现的`swap_out_victim(struct mm_struct *mm, struct Page **ptr_page, int in_tick)`选择一个，通过`swapfs_write(swap_entry_t entry, struct Page *page)`写入内容，`swapfs_write`是对我们模拟的硬盘数组读写`ide_write_secs()`的封装，ide_write指定某一块盘，从哪个扇区开始写多大数据，swapfs_write则将这些内容封装写入后，将la页表项目本来指向内存某一页的地址改为指向硬盘的地址。
4. 当get_pte发现该页表项是！0  ，说明有数据写入过，此时准备从磁盘换入。
   1. 首先`swap_in`将磁盘数据换入，换入时也根据get_pte得到对应页表项地址，若页表空闲，则顺利将读取的数据写入分配的空闲页；若不空，由3说明的方式进行swap_out页，再重复进行操作。
   2. `page_insert(pde_t *pgdir, struct Page *page, uintptr_t la, uint32_t perm)`将新的page写入页表项。

##### 具体代码列举如下：

  1、_fifo_init_mm：mm_struct中的页访问情况初始化

  2、_fifo_map_swappable：该函数的作用就是将访问过的页加入mm_struct结构的sm_priv链表中，以记录该页访问过，且可替换。

  3、_fifo_swap_out_victim：需要实现的是swap_manager_fifo对应的_fifo_swap_out_victim函数，选择出将要换出的页。按照先进先出的算   法，需要换出的页是最早被访问的页。

  4、_fifo_check_swap：检查具体交换了哪个页面，并输出。

  5、get_pte： 获取当前发生缺页的虚拟页对应的PTE 

  6、swap_in：访问的虚拟地址的物理页不存在的第二种情况是存在映射，但物理页被换出到硬盘。这时需要将硬盘上的页换入，这是通过swap_in()函数实现的。swap_in()函数通过传入的地址获取其页表项pte，根据页表项所提供的信息将硬盘上的信息读入，完成页的换入。

  7、swap_out：实现页的换出

  8、page_insert：建立虚拟地址与物理页的映射

  9、find_vma：给定内存控制块,在其中查找 addr 对应的 vma

  10、pgdir_alloc_page：给定线性地址和 page directory,为这个地址进行映射

#### 练习2：深入理解不同分页模式的工作原理（思考题）

异：sv32是两级页表，适应32位系统，10+10+12，虚拟地址最大4G， sv39、sv48适应64位系统，9+9+9+12  9+9+9+9+12,sv39为三级页表，虚拟地址空间最大512G；sv48为四级页表，虚拟地址空间达到256TB。

同：三种格式的页目录页表项都包含权限位，且都是根据逐级根据页目录索引，索引方式相同，最后指向由os分配的一个物理地址。因此代码相像，对于不同的sv页表设计，每一次对于页目录表的查询(没有创建)的操作相同，而会根据页表设计的层数导致相像代码重复次数不同。

我们需要先判断是否需要创建新页表空间，再进行页表项填写分配；因此我们认为上一级页表的每一个entry指向的下一级页目录、页表的这一页一定是存在的，所以我们认为如此封装是好的，它确保虚拟地址查询逻辑的严密性。

#### 练习3：给未被映射的地址映射上物理页（需要编程）

补充完成do_pgfault（mm/vmm.c）函数，给未被映射的地址映射上物理页。设置访问权限 的时候需要参考页面所在 VMA 的权限，同时需要注意映射物理页时需要操作内存控制 结构所指定的页表，而不是内核的页表。
请在实验报告中简要说明你的设计实现过程。请回答如下问题：

 - 请描述页目录项（Page Directory Entry）和页表项（Page Table Entry）中组成部分对ucore实现页替换算法的潜在用处。

   A: page_fault 函数不知道哪些是“合法可用”的虚拟页，PTE_A和PTE_D分别代表了内存页是否被访问过和内存也是否被修改过，借助这两个标志位实现Enhanced Clock页替换算法。

 - 如果ucore的缺页服务例程在执行过程中访问内存，出现了页访问异常，请问硬件要做哪些事情？

   A： 硬件首先会将错误的相关信息保存在相应寄存器中，并且将执行流程转交给中断处理程序。

- 数据结构Page的全局变量（其实是一个数组）的每一项与页表中的页目录项和页表项有无对应关系？如果有，其对应关系是啥？

  A：对于页目录中的每个条目，全局变量数组中的相应项可以存储该条目指向的页目录项的地址。对于每个页目录项，全局变量数组中的相应项可以存储该页目录项指向的页表项的地址。对于每个页表项，全局变量数组中的相应项可以存储该页表项指向的物理内存页的地址。

  

#### 练习4：补充完成Clock页替换算法（需要编程）

通过之前的练习，相信大家对FIFO的页面替换算法有了更深入的了解，现在请在我们给出的框架上，填写代码，实现 Clock页替换算法（mm/swap_clock.c）。
请在实验报告中简要说明你的设计实现过程。请回答如下问题：

 - 比较Clock页替换算法和FIFO算法的不同。

1、将clock页替换算法所需数据结构进行初始化。

```
_clock_init_mm(struct mm_struct *mm)
{     
     /*LAB3 EXERCISE 4: YOUR CODE*/ 
     // 初始化pra_list_head为空链表
     // 初始化当前指针curr_ptr指向pra_list_head，表示当前页面替换位置为链表头
     // 将mm的私有成员指针指向pra_list_head，用于后续的页面替换算法操作
     //cprintf(" mm->sm_priv %x in fifo_init_mm\n",mm->sm_priv);

     list_init(&pra_list_head);
     curr_ptr = &pra_list_head;
     mm->sm_priv = &pra_list_head;
     //cprintf(" mm->sm_priv %x in fifo_init_mm\n",mm->sm_priv);
     return 0;
}
```

2、记录页访问情况相关属性，应该将最近到达的页面链接到 pra_list_head 队列的末尾；将页面的visited标志置为1，表示该页面已被访问。

```
_clock_map_swappable(struct mm_struct *mm, uintptr_t addr, struct Page *page, int swap_in)
{
    list_entry_t *head=(list_entry_t*) mm->sm_priv;
    list_entry_t *entry=&(page->pra_page_link);
 
    assert(entry != NULL && curr_ptr != NULL);
    //record the page access situlation
    /*LAB3 EXERCISE 4: YOUR CODE*/ 
    // link the most recent arrival page at the back of the pra_list_head qeueue.
    // 将页面page插入到页面链表pra_list_head的末尾
    // 将页面的visited标志置为1，表示该页面已被访问
    list_add(head, entry);
    page->visited=1;
    cprintf("curr_ptr %p\n", curr_ptr);
    return 0;
}
```

3、挑选需要换出的页。首先获取当前页面对应的Page结构指针，根据物理页的visited位来进行判断，如果是1就置0，如果是0，删去并把指针赋值给ptr_page作为换出页面，一直去循环这个过程，直到找到一个合适的换出页面。

```
_clock_swap_out_victim(struct mm_struct *mm, struct Page ** ptr_page, int in_tick)
{
     list_entry_t *head=(list_entry_t*) mm->sm_priv;
         assert(head != NULL);
     assert(in_tick==0);
     /* Select the victim */
     //(1)  unlink the  earliest arrival page in front of pra_list_head qeueue
     //(2)  set the addr of addr of this page to ptr_page
     curr_ptr = head;
    while ((curr_ptr = list_prev(curr_ptr))) {
        struct Page *p = le2page(curr_ptr, pra_page_link);
        if (p->visited==0) {
        list_del(curr_ptr);
        *ptr_page = le2page(curr_ptr, pra_page_link);
        break;
        }
        else {
            p->visited=0;
        }
    }
```

比较Clock页替换算法和FIFO算法的不同：
   FIFO算法：这是一种最早的页面替换算法，它选择最早进入内存的页面进行替换。FIFO的主要优点是实现简单，但它的缺点也很明显。由于它只考虑了页面的进入时间，而没有考虑到页面的使用情况，所以可能会替换掉后面仍然需要使用的页面，导致更多的页面缺失。

   时钟页面替换算法：这是一种改进的页面替换算法，它用一个环形数据结构（像一个时钟）来记录页面的使用情况。算法将当前页面和它之前的页面看作是“近”的，而将当前页面之后的页面看作是“远”的。当需要替换页面时，时钟页面替换算法选择最近最少使用的“远”页面进行替换。这种算法不仅考虑了页面的进入时间，还考虑了页面的使用情况，所以比FIFO算法更加有效。

#### 练习5：阅读代码和实现手册，理解页表映射方式相关知识（思考题）

如果我们采用”一个大页“ 的页表映射方式，相比分级页表，有什么好处、优势，有什么坏处、风险？

**优势**：

1. TLB未命中的情况下，访存次数的比多级页表少：大页表可以提高 TLB的命中率，因为更多的虚拟地址范围映射到相同的物理页帧上。
2. 减少了页表级数：使用大页面可以直接映射到物理内存，不需要经过多级页表，减少了查找页表的时间，提高了内存访问速度。
3. 提高了内存性能：使用大页面可以减少内存管理的复杂性和开销，减轻了CPU的负载，提高了内存的性能。

**坏处**：

1. 内存浪费：使用大页表时，每个页面都会较大，因此可能会浪费一定的内存空间，尤其是对于小型数据结构或小程序来说。
2. 对于一些需要精细内存管理的操作系统，比如需要实现复杂的内存保护机制的操作系统，使用大页面可能会对内存管理造成一定的困难。
3. 在多任务环境下，由于不同任务使用的虚拟地址空间可能存在重叠，因此可能会发生页面冲突，导致程序运行异常

#### 扩展练习 Challenge：实现不考虑实现开销和效率的LRU页替换算法（需要编程）

challenge部分不是必做部分，不过在正确最后会酌情加分。需写出有详细的设计、分析和测试的实验报告。完成出色的可获得适当加分。







