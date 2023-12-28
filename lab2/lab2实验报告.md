---
author:
- 2113297 杨昕怡  2112075 李晓彤112100 吴维翰
date: October 2023
---

练习1
=====

相关知识点
----------

1.什么是first-fit 连续物理内存分配算法?

首次适应(first fit,
FF)算法，要求空闲分区链以地址递增的次序链接。在分配内存时，从链首开始顺序查找，直至找到一个大小能满足要求的空闲分区为止。然后再按照作业的大小，从该分区划出一块内存空间，分配给请求者，余下的空闲分区仍留在空闲链中。若从链首直至链尾都不能找到一个能满足要求的分区，则表明系统中已没有足够大的内存分配给该进程，内存分配失败，返回。

2.first fit算法的特点是什么?

该算法倾向于优先利用内存中低地址部分的空闲分区，从而保留了高址部分的大空闲区。这为以后到达的大作业分配大的内存空间创造了条件。其缺点是低址部分不断被划分，会留下许多难以利用的、很小的空闲分区，称为碎片。而每次查找又是从低址部分开始的，这无疑又会增加查找可用分区时的开销。

3.分区分配的操作是什么?

分配内存：

系统应利用某种分配算法，从空闲分区链(表)中找到所需大小的分区。设请求的分区大小为u.size，表中每个空闲分区的大小可表示为m.size，若m.size-u.size\<=size(size是事先规定的不再切割的剩余分区的大小)，说明多余部分太小，可不再切割，将整个分区分配给请求者。否则(即多余部分超过size)，便从该分区中按请求的大小划分出一块内存空间分配出去，余下的部分仍留在空闲分区链(表)中。然后，将分配区的首址返回给调用者。

回收内存

当进程运行完毕释放内存时，系统根据回收区的首址，从空闲区链(表)中找到相应的插入点，此时可能出现以下四种情况之一：

回收区与插入点的前一个空闲分区F1相邻接。此时应将回收区与插入点的前一分区合并，不必为回收分区分配新表项，而只需修改前一分区F1的大小。

回收分区与插入点的后一空闲分区F2相邻接。此时也可将两份区合并，形成新的空闲分区，但用回收区的首地址作为新空闲区的首址，大小为两者之和。

回收区同时与插入点的前、后两个分区邻接。此时将三个分区合并，使用F1的表项和F1的首址，取消F2的表项，大小为三者之和。

回收区既不与F1邻接，又不与F2邻接。这时应为回收区单独建立一个新表项，填写回收区的首址和大小，并根据其首地址插入到空闲链中的适当位置。

代码及注释
----------

``` {.python language="Python" firstline="62" lastline="185"}
default_init(void) {
    list_init(&free_list);//free_list 可用的物理内存块
    nr_free = 0;//初始时为0
}

static void
default_init_memmap(struct Page *base, size_t n) {
    //这个函数用于初始化一个物理内存块。base起始页，n指定数量大小


    assert(n > 0);//确保n>0，否则报错
    struct Page *p = base;
    for (; p != base + n; p ++) {//在 for 循环中，它遍历从 base 到 base + n 范围内的页框
        assert(PageReserved(p));//确保已被保留
        p->flags = p->property = 0;// 清空当前页框的标志和属性信息，
        set_page_ref(p, 0);//页框的引用计数设置为0
    }
    base->property = n;//设置第一个页面的property为 n，表示整个物理内存块的大小设置当前页块的属性为释放的页块数
    SetPageProperty(base);//调用SetPageProperty设置该页面的属性，当前页块标记为已分配状态
    nr_free += n;//上面释放了n个，所以可用加n
    if (list_empty(&free_list)) {
        list_add(&free_list, &(base->page_link));//将该物理页结构体插入到 free_list 链表中，确保链表中的页按照地址顺序排列
    } else {
        list_entry_t* le = &free_list;
        while ((le = list_next(le)) != &free_list) {
            //使用 list_next 函数获取链表中的下一个元素，直到遍历到链表末尾（list_next(le) 返回 &free_list
            struct Page* page = le2page(le, page_link);
            //这是一个常见的宏定义，用于将链表节点（list_entry_t）转换为包含该节点的数据结构（struct Page）
            if (base < page) {
                //找到第一个大于base的页，将base插入到它前面，并退出循环
                list_add_before(le, &(base->page_link));
                break;
            } else if (list_next(le) == &free_list) {
                //若已经到达链表结尾，将base插入到链表尾部，base的地址比free_list中的所有元素都大
                list_add(le, &(base->page_link));
            }
        }
    }
}

static struct Page *
default_alloc_pages(size_t n) {
    //用于分配指定数量的物理内存页。


    assert(n > 0);
    if (n > nr_free) {
        return NULL;// 如果没有足够的空闲页面来满足请求，返回NULL
    }
    struct Page *page = NULL;
    list_entry_t *le = &free_list;
    //循环一次free_list找到第一块满足大小需求的free block，并返回
    while ((le = list_next(le)) != &free_list) {
        struct Page *p = le2page(le, page_link);
        if (p->property >= n) {
            page = p;
            break;
        }
    }
    //返回后，将free block的块取出后，进行划分，并将剩余空闲块重新加到free_list当中，对free_area中的参数更新
    // 如果找到了第一个满足请求的页面property >= n
    if (page != NULL) {
        list_entry_t* prev = list_prev(&(page->page_link));
        list_del(&(page->page_link));
        // 如果页面的属性大于请求的页面数量，进行分割
        if (page->property > n) {
            struct Page *p = page + n;
            p->property = page->property - n;
            SetPageProperty(p);
            list_add(prev, &(p->page_link));
        }
        nr_free -= n;// 更新可用的空闲页面数量
        ClearPageProperty(page);// 清除页面属性
    }
    //返回 target page的地址
    return page;
}

static void
default_free_pages(struct Page *base, size_t n) {
    //用于释放一块连续的物理内存页,并通过合并可用内存块来减少内存碎片，起始页和数量



    assert(n > 0);
    struct Page *p = base;
    //在确认page是可以被修改的前提下，将已分配的块重新初始化
    for (; p != base + n; p ++) {
        assert(!PageReserved(p) && !PageProperty(p));
        p->flags = 0;//设置可用
        set_page_ref(p, 0);//并将页框的引用计数设置为0。这意味着该页面不再被引用，通常是因为要释放该页面。
    }
    
    base->property = n;//设置第一个页面的property为 n，表示整个物理内存块的大小设置当前页块的属性为释放的页块数
    SetPageProperty(base);//调用SetPageProperty设置该页面的属性，当前页块标记为已分配状态
    nr_free += n;//上面释放了n个，所以可用加n
    //把这块空闲块加入free_list中，并保持链表按照物理地址升序排列
    if (list_empty(&free_list)) {
        list_add(&free_list, &(base->page_link));
    } else {
        list_entry_t* le = &free_list;
        while ((le = list_next(le)) != &free_list) {
            struct Page* page = le2page(le, page_link);
            if (base < page) {
                list_add_before(le, &(base->page_link));
                break;
            } else if (list_next(le) == &free_list) {
                list_add(le, &(base->page_link));
            }
        }
    }
    //检测空闲块是否可以和前后空闲块合并
   
    list_entry_t* le = list_prev(&(base->page_link));
    //获取了指向要释放的内存块 base 前一个页面的指针 le
    if (le != &free_list) {
        p = le2page(le, page_link);
        if (p + p->property == base) {
            //将前一个页面 p 的 property 属性增加为合并后的总大
            p->property += base->property;//将前一个页面 p 的 property 属性增加为合并后的总大
            ClearPageProperty(base);//清除属性
            list_del(&(base->page_link));//删除节点
            base = p;//更新 base 的指针，指合并后的页面
        }
```

代码解析
--------

首先阅读default\_pmm.c代码的开头部分，其大意为：

准备:熟悉list.h中的结构体list及其接口，并了解le2page等宏的作用；

default\_init:
你可以重复使用default\_init函数初始化free\_list和nr\_free。free\_list用来记录空闲的内存块。nr\_free是空闲内存块的总数。

default\_init\_memmap: 调用路径： kern\_init --\> pmm\_init --\>
page\_init --\> init\_memmap --\> pmm\_manager --\> init\_memmap.
该函数被用来初始化一个空闲内存块。

default\_alloc\_pages
在空闲块的链表内搜索第一个空闲的内存块，并重新设置该块的尺寸。

default\_free\_pages
将内存页重新连接到空闲块的链表中，有可能会将几个小的空闲内存块合并为一个大的内存块。

1.default\_init:

在这之前，我们先根据要求，打开libs/list.h并认真查看。在这里我们需要做的是初始化free\_list，然后将nr\_free设为0。
可以看到，在该函数之前，曾定义了free\_area与两个相关的宏free\_list，nr\_free：

    free_area_t free_area;
    
    #define free_list (free_area.free_list)
    #define nr_free (free_area.nr_free)

其中，free\_area\_t的定义为

    //    File:    memlayout.h
    //    Line:    155-158
    
    /* free_area_t - maintains a doubly linked list to record free (unused) pages */
    typedef struct {
        list_entry_t free_list;         // the list header
        unsigned int nr_free;           // # of free pages in this free list
    } free_area_t;

显然，在default\_pmm.c中，宏free\_list和nr\_free即为变量free\_area的两个成员，这样写调用更为简便。

那么，如何对free\_list进行初始化呢？我们把目光转向list.h，发现有这样一段代码：

    //    File:    default_pmm.c
    
    /* *
     * list_init - initialize a new entry
     * @elm:        new entry to be initialized
     * */
    static inline void
    list_init(list_entry_t *elm) {
        elm->prev = elm->next = elm;
    }

显然，这就是用来初始化list的。最终代码如下：

    //    File:    default_pmm.c
    
    static void
    default_init(void) {
        list_init(&free_list);
        nr_free = 0;
    }

2.default\_init\_memmap

该函数的功能是初始化一个内存块。

为了初始化一个空闲的内存块，首先需要初始化该空闲的内存块中每一页。这个过程包括：

设置p-\>flags的PG\_property位，这意味着这一页是可用的。P.S.
在函数pmm\_init（在pmm.c中），PG\_reserved位已经被设置好。

如果该页是可用的而且不是一个空闲内存块的第一页，那么p-\>property应该被设置为0。
如果该页是可用的而且是一个空闲内存块的第一页，那么p-\>property应该被设置为该块的总页数。

p-\>ref应该被设为0,因为现在p是可用的而且不含有引用。

最后，我们应该更新空闲内存块的总数：nr\_free += n。

以上为注释的含义。那么，在此需要提起的是，在memlayout.h中定义了两个宏以及对其的一系列操作：

    //    File:    memlayout.h
    //    Line:    106-115
    
    /* Flags describing the status of a page frame */
    #define PG_reserved                 0       // if this bit=1: the Page is reserved for kernel, cannot be used in alloc/free_pages; otherwise, this bit=0 
    #define PG_property                 1       // if this bit=1: the Page is the head page of a free memory block(contains some continuous_addrress pages), and can be used in alloc_pages; if this bit=0: if the Page is the the head page of a free memory block, then this Page and the memory block is alloced. Or this Page isn't the head page.
    
    #define SetPageReserved(page)       set_bit(PG_reserved, &((page)->flags))
    #define ClearPageReserved(page)     clear_bit(PG_reserved, &((page)->flags))
    #define PageReserved(page)          test_bit(PG_reserved, &((page)->flags))
    #define SetPageProperty(page)       set_bit(PG_property, &((page)->flags))
    #define ClearPageProperty(page)     clear_bit(PG_property, &((page)->flags))
    #define PageProperty(page)          test_bit(PG_property, &((page)->flags))

这两个宏是内存块所在page的flag，其中，PG\_reserved是flag的第0位，当其值为1时表示该页是内核预留页，不能被alloc或free\_pages使用；PG\_property是flag的第一位，当其值为1时表示该页是一个空闲内存块的第一个页，可以被alloc\_pages使用，当其值为0时，如果此页是一个空闲内存块的第一个页，那么该页以及该内存块已经被分配。

代码如下：

    //    File:    default_pmm.c
    //    Line:    107-120
    
    static void
    default_init_memmap(struct Page *base, size_t n) {
        assert(n > 0);
        struct Page *p = base;
        for (; p != base + n; p ++) {
            assert(PageReserved(p));
            p->flags = p->property = 0;
            set_page_ref(p, 0);
        }
        base->property = n;
        SetPageProperty(base);
        nr_free += n;
        list_add(&free_list, &(base->page_link));
    }

3.default\_alloc\_pages

在空间的内存链表中搜索第一个可用的内存块（块尺寸\>=n）并重新设置发现块的尺寸，然后该块作为被malloc调用得到的地址被返回。

所以应该在空闲的内存链表中像这样搜索：

在while循环中，获得结构体page并检查该块所含空闲页数（p-\>property）是否大于等于n

struct Page \*p = le2page(le, page\_link);

if (p-\>property \>= n) \...

如果我们找到这个p，这就意味着我们找到了一个尺寸大于等于n的空闲内存块，它的前n页可以被申请。该页的一些标志位应该被设置为：PG\_reserved
= 1，PG\_property = 0。然后，取消该页与free\_list的链接。

如果p-\>property大于n，我们应该重新计算该内存块的剩余页数。（例如：le2page(le,
page\_link)-\>property = p-\>property - n;）

重新计算nr\_free（剩余所有空闲内存页的数量）。

返回p。

如果我们没有找到尺寸大于等于n的空闲内存块，那么返回NULL。

据解释可知，该函数作用为分配内存，即在空闲块的链表内搜索第一个空闲的内存块，并重新设置该块的尺寸。

代码如下：

    //    File:    default_pmm.c
    //    Line:    122-148
    
    static struct Page *
    default_alloc_pages(size_t n) {
        assert(n > 0);
        if (n > nr_free) {
            return NULL;
        }
        struct Page *page = NULL;
        list_entry_t *le = &free_list;
        while ((le = list_next(le)) != &free_list) {
            struct Page *p = le2page(le, page_link);
            if (p->property >= n) {
                page = p;
                break;
            }
        }
        if (page != NULL) {
            if (page->property > n) {
                struct Page *p = page + n;
                p->property = page->property - n;
                SetPageProperty(p);
                list_add_after(&(page->page_link), &(p->page_link));
            }
            list_del(&(page->page_link));
            nr_free -= n;
            ClearPageProperty(page);
        }
        return page;
    }

4.default\_free\_pages

重新将页连接到空闲内存列表中，有可能将一些小的内存块合并为一个大的内存块。

通过独立内存块的base地址，搜索它正确的位置（从低到高），并插入该页。（可能使用list\_next，le2page，list\_add\_before）

重置这些页的参数，例如p-\>ref和p-\>flags（PageProperty）。

尝试合并低或高地址的内存块。注意：应该正确修改一些页的p-\>property。

当合并的时候，对于某一内存块中的第一页p，当p + p-\>property ==
base或者base + base-\>property == p时就该将这两块合并了。

        //    File:    default_pmm.c
    
    static void
    default_free_pages(struct Page *base, size_t n) {
        assert(n > 0);
        struct Page *p = base;
        for (; p != base + n; p ++) {
            assert(!PageReserved(p) && !PageProperty(p));
            p->flags = 0;
            set_page_ref(p, 0);
        }
        base->property = n;
        SetPageProperty(base);
        list_entry_t *le = list_next(&free_list);
        while (le != &free_list) {
            p = le2page(le, page_link);
            le = list_next(le);
            if (base + base->property == p) {
                base->property += p->property;
                ClearPageProperty(p);
                list_del(&(p->page_link));
            }
            else if (p + p->property == base) {
                p->property += base->property;
                ClearPageProperty(base);
                base = p;
                list_del(&(p->page_link));
            }
        }
        nr_free += n;
        list_add(&free_list, &(base->page_link));
    }

练习2
=====

具体实现，请参考gitee代码。

(1)best\_fit\_init(void)

进行best\_fit\_pmm\_manager的初始化，参考default\_init(void)函数即可。

(2)best\_fit\_init\_memmap(struct Page \*base, size\_t n)

进行free\_list的初始化，参考default\_init\_memmap(struct Page \*base,
size\_t n)函数即可。

(3)best\_fit\_alloc\_pages(size\_t n)

从空闲链表中(free\_list)中按照best
fit方法取下n个页，返回一个包含n个页的Page指针。工作流程如下：

首先，assert确保n\>0,否则出现报错信息。

然后定义Page类型的指针page初始化为NULL，定义指向free\_list的指针le，定义min\_size变量初始化为nr\_free+1;

遍历空闲页面链表，每当找到一个空闲页p，满足n \<= p-\>property \<=
min\_size时，将p赋值给page，并更新min\_size为p-\>property；

最后要将被分配的页其从链表中删除，并返回其对应的指针，具体操作为：将page从free\_list中删除；

如果p-\>property大于n，则定义一个Page类型的指针p，使其指向page+n页，并将p-\>property设置为page-\>property-n，这意味着页面p变成了一片连续空闲页的head；

将p添加到空闲链表中；

nr\_free减去n；

利用ClearPageProperty函数将page-\>flags的property位设置为0，表示被分配；

返回page。

(4)best\_fit\_free\_pages(struct Page \*base, size\_t n)

参考default\_free\_pages(struct Page \*base, size\_t n)函数即可。

你的 Best-Fit 算法是否有进一步的改进空间？

Challenage 3
============

操作系统可以通过
BIOS（基本输入输出系统）或UEFI（统一固件接口）等固件来获取硬件可用物理内存范围的信息。在系统启动时，操作系统会与固件进行通信，并从中获取有关硬件的详细信息。而固件中则包含了有关硬件设备和系统配置的信息，并提供了一套ACPI表和方法，供操作系统在启动时访问和使用。通过ACPI表，操作系统可以获取硬件可用的物理内存范围。以下是一些步骤：

1.访问ACPI表：ACPI BIOS包含着一些ACPI表，包括RSDP、RSDT和其他一些表。操作系统会根据RSDP找到RSDT表。

2.解析ACPI表：操作系统会解析RSDT表，从中获取其他ACPI表的地址，如FADT（Fixed ACPI Description Table），这个表包含了计算机硬件的详细信息。

3.获取硬件信息：通过解析FADT表，操作系统可以获取一系列硬件信息，包括可用的物理内存范围、处理器信息、IO端口、中断控制器、电源管理等。

4.ACPI方法：ACPI还定义了一些方法，也可以通过这些方法获取硬件信息。操作系统可以发送ACPI方法调用请求，并接收系统固件返回的结果来获取硬件信息。

知识点
======

本实验中重要的知识点，以及与对应的
OS原理中的知识点，并简要说明你对二者的含义，关系，差异等方面的理解（也可能出现实验中的知识点没有对应的原理知识点）

1.内存分段：将物理内存划分为不同的逻辑段，每个段用于存放特定类型的数据或程序。这样可以更好地组织和管理内存空间。
2.内存分页：将物理内存和逻辑内存都划分为固定大小的页面（通常为4KB），并按需将页面加载到物理内存中。通过分页，操作系统可以实现虚拟内存，提供比物理内存更大的地址空间。
3.虚拟内存：使用虚拟内存技术，操作系统可以将部分程序或数据从物理内存交换到硬盘上，以释放内存空间或满足程序的需要。这使得多个程序可以同时运行，并且每个程序都感觉自己独占整个内存。
4.页面置换算法：当物理内存不足时，操作系统需要选择要置换出去的页面。常见的页面置换算法有最先进先出（FIFO）、最近最久未使用（LRU）和最优页面置换（OPT）等。
5.内存分配与回收：操作系统负责分配和回收内存资源。常见的内存分配方式有连续分配和非连续分配。连续分配包括固定分区分配、动态分区分配和可变分区分配。非连续分配则使用页表和段表来管理虚拟内存和物理内存之间的映射关系。
6.内存保护：为了确保程序之间的隔离和安全性，操作系统提供内存保护机制。这包括将不同程序的内存空间隔离开来，防止彼此干扰，以及使用访问权限位来控制对内存的读写操作。
7.垃圾回收：在一些高级编程语言中，垃圾回收是自动管理内存的过程。操作系统或运行时环境负责检测和回收不再使用的内存块，以避免内存泄漏和资源浪费。
