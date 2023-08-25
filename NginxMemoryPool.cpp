//
// Created by a1767 on 2023/8/25.
//

#include "NginxMemoryPool.h"
#include <cstdlib>
#include <cstring>
#include <cstdio>
//-创建内存池并初始化，api以静态成员(工厂)的方式模拟C风格函数实现
//-capacity是buffer的容量，在初始化的时候确定，后续所有小块的buffer都是这个大小

NginxMemoryPool *NginxMemoryPool::createPool(NginxMemoryPool::smallBlockSize capacity)
{
    // 分配内存池管理+第一个小缓冲区+小缓冲区大小
    unsigned long long total=sizeof(NginxMemoryPool)+sizeof(smallBlock)+capacity;
    void *temp=malloc(total);
    memset(temp, 0, total);

    NginxMemoryPool*pool=(NginxMemoryPool*)temp;
    if(temp == nullptr)
    {
        printf("memory create error\n");
    }
    pool->bufferCapacity=capacity;
    pool->bigBlockStart= nullptr;
    pool->currentFindLocation=static_cast<sPointer>(pool->smallBlockStart);

    // pool+1 为下一块内存大小 sizeof(NginxMemoryPool)+sizeof(smallBlock)+capacity;
    sPointer sPointer1=(sPointer)(pool+1); // 就是小缓冲区标志头的内存空间
    sPointer1->avaliableBufferStart=(char*)(sPointer1+1); //指向可用缓冲区开始
    sPointer1->bufferEnd=sPointer1->avaliableBufferStart+capacity;
    sPointer1->nextSmallBlock= nullptr;
    sPointer1->missNum=0;

    return pool;
}

// 代替malloc
void *NginxMemoryPool::poolMalloc(NginxMemoryPool *pool, NginxMemoryPool::smallBlockSize size)
{
//-先判断要malloc的是大内存还是小内存
    if(size<pool->bufferCapacity)
    {//-如果是小内存
        //-从cur small block开始寻找
        sPointer temp = pool -> currentFindLocation;
        do
        {
            //-判断当前small block的buffer够不够分配
            //-如果够分配,直接返回
            if(temp->bufferEnd-temp->avaliableBufferStart>size)
            {
                char * res = temp->avaliableBufferStart;
                temp -> avaliableBufferStart = temp->avaliableBufferStart+size;
                return res;
            }
            temp = temp->nextSmallBlock;
        }while (temp!= nullptr);
        //-如果最后一个small block都不够分配，则创建新的small block;
        //-该small block在创建后,直接预先分配size大小的空间,所以返回即可.
        return createNewSmallBlock(pool,size);
    }
    //-分配大内存
    return createNewSmallBlock(pool,size);
}

char *NginxMemoryPool::createNewSmallBlock(NginxMemoryPool *pool, NginxMemoryPool::smallBlockSize size)
{
    //-先创建新的small block，注意还有buffer
    size_t malloc_size = sizeof(smallBlock)+pool->bufferCapacity;
    void * temp = malloc(malloc_size);
    if(temp== nullptr)
    {
        printf("small block create error\n");
        exit(-1);
    }
    memset(temp,0,malloc_size);

    //-初始化新的small block
    sPointer sbp = static_cast<sPointer>(temp);

    sbp -> avaliableBufferStart = (char*)(sbp+1);//-跨越一个small_block的步长
    sbp -> bufferEnd = (char*)temp+malloc_size;
    sbp -> nextSmallBlock = nullptr;
    sbp -> missNum = 0;
    //-预留size空间给新分配的内存
    char* res = sbp -> avaliableBufferStart;//-存个副本作为返回值
    sbp -> avaliableBufferStart = res+size;

    //-因为目前的所有small_block都没有足够的空间了。
    //-意味着可能需要更新寻找的起点
    sPointer p = pool -> currentFindLocation;
    while(p->nextSmallBlock!= nullptr)
    {
        if(p->missNum>4) // 未命中次数太多,说明内存太小不值得从这里查找
        {
            pool -> currentFindLocation = p->nextSmallBlock;//先预判下一次
        }
        ++(p->missNum); //增加未命中次数
        p = p->nextSmallBlock; // 判断下一次可不可行
    }

    //-此时p正好指向当前pool中最后一个small_block,将新节点接上去。
    p->nextSmallBlock = sbp;

    //-因为最后一个block有可能missNum>4导致currentFindLocation更新成nullptr
    //-所以还要判断一下
    if(pool -> currentFindLocation == nullptr)
    {
        pool -> currentFindLocation = sbp;
    }
    return res;//-返回新分配内存的首地址
}


/*如果size超过了预设的capacity，那就会走这个api。其同样也是一个链式查找的过程，只不过比查找small block
更快更粗暴。big block链没有类似currentFindLocation这样的节点，只要从头开始遍历，如果有空buffer
就返回该block，如果超过3个还没找到（同样是Nginx的经验值）就直接不找了，创建新的big block。*/
char *NginxMemoryPool::createBigBlock(NginxMemoryPool *pool, NginxMemoryPool::smallBlockSize size)
{
//-先分配size大小的空间
    void*temp = malloc(size);
    memset(temp, 0, size);

    //-从big_block_start开始寻找,注意big block是一个栈式链，插入新元素是插入到头结点的位置。
    bPointer bbp = pool->bigBlockStart;
    int i = 0;
    while(bbp)
    {
        if(bbp->bigBufferStart == nullptr) // 说明对应的buffer已经被回收了
        {
            bbp->bigBufferStart = (char*)temp;
            return bbp->bigBufferStart;
        }
        if(i>3){
            break;//-为了保证效率，如果找三轮还没找到有空buffer的big_block，就直接建立新的big_block
        }
        bbp = bbp->nextBigBlock;
        ++i;
    }

    //-创建新的big_block，这里比较难懂的点，就是Nginx觉得big_block的buffer虽然是一个随机地址的大内存
    //-但是big_block本身算一个小内存，那就不应该还是用随机地址，应该保存在内存池内部的空间。
    //-所以这里有个套娃的内存池malloc操作
    bPointer new_bbp = static_cast<bPointer>(poolMalloc(pool, sizeof(bigBlock)));
    //-初始化
    new_bbp -> bigBufferStart = (char*)temp;
    new_bbp -> nextBigBlock = pool->bigBlockStart; //链栈插入
    pool->bigBlockStart = new_bbp;

    //-返回分配内存的首地址
    return new_bbp->bigBufferStart;
}

void NginxMemoryPool::freeBigBlock(NginxMemoryPool *pool, char *buffer_ptr)
{
    bPointer bbp = pool->bigBlockStart;
    while(bbp)
    {
        if(bbp->bigBufferStart == buffer_ptr)
        {
            bbp->bigBufferStart = nullptr;
            return;
        }
        bbp = bbp->nextBigBlock;
    }
}

/*pool中有两条链分别指向大内存和小内存，那么分别沿着这两条链去free掉内存即可,
由于大内存的buffer和big block不是一起malloc的，所以只需要free掉buffer,
而big block是分配在小内存池中的，所以，之后free掉小内存的时候会顺带一起free掉。
比较值得注意的一点是，small链的free不是从第一个small block开始的，而是第二个small block。
第一个small block的空间是和pool一起malloc出来的，不需要free，只要最后的时候
free pool就会一起释放掉。
*/
void NginxMemoryPool::destroyPool(NginxMemoryPool *pool)
{
    //-先free掉big block的buffer
    bPointer bbp = pool->bigBlockStart;
    while(bbp)
    {
        if(bbp->bigBufferStart)
        {
            free(bbp->bigBufferStart);
        }
        bbp = bbp->nextBigBlock;
    }

    //-再free掉small block的buffer
    sPointer sbp = pool->smallBlockStart->nextSmallBlock;
    while(sbp)
    {
        free(sbp);
        sbp = sbp->nextSmallBlock;
    }

    //-最后free掉pool
    free(pool);
}

