1、Buddy System简介

在Buddy System中，我们去寻找大小合适的内存块（大于等于所需大小并且最接近2的幂，比如需要27，实际分配32）

1.如果找到了，分配给应用程序。
2.如果没找到，分出合适的内存块。

1.对半分离出高于所需大小的空闲内存块
2.如果分到最低限度，分配这个大小。
3.回溯到步骤1（寻找合适大小的块）
4.重复该步骤直到一个合适的块



释放某一内存的时候

1.寻找相邻的块，看其是否释放了。
2.如果相邻块也释放了，合并这两个块，重复上述步骤直到遇上未释放的相邻块，或者达到最高上限（即所有内存都释放了）。

分配器的整体思想是，通过一个数组形式的完全二叉树来监控管理内存，二叉树的节点用于标记相应内存块的使用状态，高层节点对应大的块，低层节点对应小的块，在分配和释放中我们就通过这些节点的标记属性来进行块的分离合并。

1、先设计计算传入size_t类型的数的最近的2的幂

```HTML
unsigned calculate_node_size(unsigned n) {
    unsigned i = 0;
    n--;
    while (n &gt;= 1) {
        n &gt;&gt;= 1; 
        i++;
    }
    return 1 &lt;&lt; i; 
}
```

2、内存分配时，向下寻找大小合适的节点，入参是分配器指针和需要分配的大小，返回值是内存块索引。首先将size调整到2的幂大小，并检查是否超过最大限度。然后进行适配搜索，深度优先遍历，将其longest标记为0，即分离适配的块出来，并转换为内存块索引offset返回，依据二叉树排列序号。

```HTML
<code>buddy_system_alloc_pages(size_t n)
{
    assert(n &gt; 0);
    if (n &gt; nr_free)
    {
        return NULL;
    }
    struct Page *page = NULL;
    unsigned index = 0;//当前要分配的内存块在二叉树中的节点索引
    unsigned node_size;//当前节点管理的内存大小
    unsigned offset = 0;//分配的内存块的偏移量
    if (n &lt;= 0)
        n = 1;
    else if (!IS_POWER_OF_2(n))
    {
        n = calculate_node_size(n);
    }
    if (root[index] &lt; n)
        offset = -1;
    for (node_size = size; node_size != n; node_size /= 2)
    {
        if (root[LEFT_LEAF(index) ] &gt;= n)
            index =LEFT_LEAF(index) ;
        else
            index =RIGHT_LEAF(index) ;
    }
    root[index] = 0;
    offset = (index + 1) * node_size - size;
    while (index &gt; 0)//更新祖先节点的 longest 值
    {
       index=PARENT(index);
        root[index]=max(root[LEFT_LEAF(index)],root[RIGHT_LEAF(index)]);
    }
    page = _base+ offset;
    unsigned size_ = calculate_node_size(n);
    nr_free -= size_;
    for (struct Page *p = page; p != page + size_; p++)
        ClearPageProperty(p);
    page-&gt;property = n;
    return page;
}</code>
```

3、内存释放时将longest恢复到原来满状态的值。继续向上回溯，检查是否存在合并的块，依据就是左右子树longest的值相加是否等于原空闲块满状态的大小，如果能够合并，就将父节点longest标记为相加的和。

```HTML
static void buddy_system_free_pages(struct Page *base, size_t n)
{
    assert(n &gt; 0);
    n=calculate_node_size(n);
    struct Page *p = base;
    for (; p != base + n; p++)
    {
        assert(!PageReserved(p) &amp;&amp; !PageProperty(p));
        set_page_ref(p, 0);
    }
    nr_free += n;
    unsigned offset = base - _base;
    unsigned node_size = 1;
    unsigned index = size + offset - 1;
    while(root[index]){
        node_size *= 2;
        if (index == 0)
            return;
        index=PARENT(index);
    }
    root[index] = node_size;
    while (index)
    {
        index=PARENT(index);
        node_size *= 2;
        if (root[LEFT_LEAF(index)] + root[RIGHT_LEAF(index)] == node_size)
            root[index] = node_size;
        else
            root[index]=max(root[LEFT_LEAF(index)],root[RIGHT_LEAF(index)]);
    }
}
```

4、检查。

```HTML
<p>static void buddy_check(void)
{
    struct Page *p0, *A, *B, *C, *D;
    p0 = A = B = C = D = NULL;
    A = alloc_pages(70);
    B = alloc_pages(35);
    C = alloc_pages(257);
    D = alloc_pages(63);
    cprintf(&quot;A分配70，B分配35，C分配257，D分配63\n&quot;);
    cprintf(&quot;此时A %p\n&quot;,A);
    cprintf(&quot;此时B %p\n&quot;,B);
    cprintf(&quot;此时C %p\n&quot;,C);
    cprintf(&quot;此时D %p\n&quot;,D);</p>
    free_pages(B, 35);
    cprintf(&quot;B释放35\n&quot;);
    free_pages(D, 63);
    cprintf(&quot;D释放63\n&quot;);
    cprintf(&quot;BD应该合并\n&quot;);
    free_pages(A, 70);
    cprintf(&quot;A释放70\n&quot;);
    cprintf(&quot;此时前512个已空，我们再分配511个的A来测试\n&quot;);
    A = alloc_pages(511);
    cprintf(&quot;A分配511\n&quot;);
    cprintf(&quot;此时A %p\n&quot;,A);
    free_pages(A, 512);
    cprintf(&quot;A释放512\n&quot;);

    A = alloc_pages(255);
    B = alloc_pages(255);
    cprintf(&quot;A分配255，B分配255\n&quot;);
    cprintf(&quot;此时A %p\n&quot;,A);
    cprintf(&quot;此时B %p\n&quot;,B);
    free_pages(C, 257);
    free_pages(A, 255);
    free_pages(A, 255);  
    cprintf(&quot;全部释放\n&quot;);
    cprintf(&quot;检查完成，没有错误\n&quot;);
}</p>

```

结果为

![image-20231016173857587](C:\Users\86199\Desktop\buddy System报告\image-20231016173857587.png)