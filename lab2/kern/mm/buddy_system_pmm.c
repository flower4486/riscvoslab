#include <pmm.h>
#include <list.h>
#include <string.h>
#include <default_pmm.h>
#include <stdio.h>

#define LEFT_LEAF(index) ((index) * 2 + 1)
#define RIGHT_LEAF(index) ((index) * 2 + 2)
#define PARENT(index) ( ((index) + 1) / 2 - 1)
#define IS_POWER_OF_2(x) (!((x)&((x)-1)))
#define max(a, b) ((a) > (b) ? (a) : (b))
#define free_list (free_area.free_list)//ά�����п��е��ڴ�飬��һ��˫���������ʼʱ����prev��next��ָ������
#define nr_free (free_area.nr_free) //����ҳ����Ŀ
//����ҳ���Ĵ�С�����������ֵ��n
unsigned calculate_node_size(unsigned n) {
    unsigned i = 0;
    n--;
    while (n >= 1) {
        n >>= 1; 
        i++;
    }
    return 1 << i; 
}
free_area_t free_area;
unsigned *root;
int size;
struct Page *_base;

static void
buddy_system_init(void)
{
    list_init(&free_list);
    nr_free = 0;
}
//��ʼ���ڴ�ӳ���ϵ,��һ������������ҳ���ʼ��Ϊ���õĿ���ҳ��
static void
buddy_system_init_memmap(struct Page *base, size_t n)
{
    assert(n > 0);
    struct Page *p = base;
    for (; p != base + n; p++)
    {
        assert(PageReserved(p));
        p->flags = 0;
        p->property = 0;
        set_page_ref(p, 0);
        SetPageProperty(p);
    }
    base->property = n;
    SetPageProperty(base);
    nr_free += n;
    _base = base;
    size =calculate_node_size(n);
    unsigned node_size = 2*size;
    root = (unsigned *)(base + size);
    //��֤�����С��2���ݴη�
    for (int i = 0; i < 2 * size - 1; ++i)
    {
        if (IS_POWER_OF_2(i + 1))
            node_size /= 2;
        root[i] = node_size;
    }
}

static struct Page *
buddy_system_alloc_pages(size_t n)
{
    assert(n > 0);
    if (n > nr_free)
    {
        return NULL;
    }
    struct Page *page = NULL;
    unsigned index = 0;//��ǰҪ������ڴ���ڶ������еĽڵ�����
    unsigned node_size;//��ǰ�ڵ������ڴ��С
    unsigned offset = 0;//������ڴ���ƫ����
    if (n <= 0)
        n = 1;
    else if (!IS_POWER_OF_2(n))
    {
        n = calculate_node_size(n);
    }
    if (root[index] < n)
        offset = -1;
    for (node_size = size; node_size != n; node_size /= 2)
    {
        if (root[LEFT_LEAF(index) ] >= n)
            index =LEFT_LEAF(index) ;
        else
            index =RIGHT_LEAF(index) ;
    }
    root[index] = 0;
    offset = (index + 1) * node_size - size;
    while (index > 0)//�������Ƚڵ�� longest ֵ
    {
       index=PARENT(index);
        root[index]=max(root[LEFT_LEAF(index)],root[RIGHT_LEAF(index)]);
    }
    page = _base+ offset;
    unsigned size_ = calculate_node_size(n);
    nr_free -= size_;
    for (struct Page *p = page; p != page + size_; p++)
        ClearPageProperty(p);
    page->property = n;
    return page;
}
//�ͷ�ҳ��
static void buddy_system_free_pages(struct Page *base, size_t n)
{
    assert(n > 0);
    n=calculate_node_size(n);
    struct Page *p = base;
    for (; p != base + n; p++)
    {
        assert(!PageReserved(p) && !PageProperty(p));
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
 /*��longest�ָ���ԭ����״̬��ֵ���������ϻ��ݣ�����Ƿ���ںϲ��Ŀ飬���ݾ�����������longest��ֵ����Ƿ����ԭ���п���״̬�Ĵ�С������ܹ��ϲ����ͽ����ڵ�longest���Ϊ��ӵĺ�*/
    }
}

static size_t
buddy_system_nr_free_pages(void)
{
    return nr_free;
}//��ѯ���õĿ���ҳ��
static void
buddy_check(void)
{
    struct Page *p0, *A, *B, *C, *D;
    p0 = A = B = C = D = NULL;
    A = alloc_pages(70);
    B = alloc_pages(35);
    C = alloc_pages(257);
    D = alloc_pages(63);
    cprintf("A����70��B����35��C����257��D����63\n");
    cprintf("��ʱA %p\n",A);
    cprintf("��ʱB %p\n",B);
    cprintf("��ʱC %p\n",C);
    cprintf("��ʱD %p\n",D);
        
    free_pages(B, 35);
    cprintf("B�ͷ�35\n");
    free_pages(D, 63);
    cprintf("D�ͷ�63\n");
    cprintf("BDӦ�úϲ�\n");
    free_pages(A, 70);
    cprintf("A�ͷ�70\n");
    cprintf("��ʱǰ512���ѿգ������ٷ���511����A������\n");
    A = alloc_pages(511);
    cprintf("A����511\n");
    cprintf("��ʱA %p\n",A);
    free_pages(A, 512);
    cprintf("A�ͷ�512\n");

    A = alloc_pages(255);
    B = alloc_pages(255);
    cprintf("A����255��B����255\n");
    cprintf("��ʱA %p\n",A);
    cprintf("��ʱB %p\n",B);
    free_pages(C, 257);
    free_pages(A, 255);
    free_pages(A, 255);  
    cprintf("ȫ���ͷ�\n");
    cprintf("�����ɣ�û�д���\n");
}
// ����ṹ����
const struct pmm_manager buddy_pmm_manager = {
    .name = "buddy_pmm_manager",
    .init = buddy_system_init,
    .init_memmap = buddy_system_init_memmap,
    .alloc_pages = buddy_system_alloc_pages,
    .free_pages = buddy_system_free_pages,
    .nr_free_pages = buddy_system_nr_free_pages,
    .check = buddy_check,
};