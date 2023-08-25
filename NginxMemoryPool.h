//
// Created by a1767 on 2023/8/25.
//

#ifndef ALLOCATE_NGINXMEMORYPOOL_H
#define ALLOCATE_NGINXMEMORYPOOL_H
//一个small block我们可以看做是一个小缓冲区，而整条链的小缓冲区串起来组成一个大缓冲区，就是内存池了。

/*对于small block，我们看它的格局要更上层一点，达到看山不是山，看水不是水的境界,
因为它不仅仅是单独的block对象，还代表了后面跟着的buffer，当链中所有小缓冲区都不够位
置分配新的空间时，就会创建新的small block，而创建的时候，会一次性创建
small_block+buffer_capacity大小的空间。目的是为了让它俩连续存放
我们不需要在small_block的数据结构中存buffer的首地址指针，因为很自然的是，
 我们拿到small_block的指针后自然也就拿到了buffer的首地址指针即
 buffer_head_ptr = (char*)small_block + sizeof(small_block);
*/


struct smallBlock // 内存块的标志头,管理缓冲区
{
    char* avaliableBufferStart; // 可用缓冲区开始处
    char * bufferEnd; // 缓冲区末尾
    smallBlock*nextSmallBlock; // 下一块缓冲区
    unsigned int missNum; // 没有命中该缓冲区的次数
};

struct bigBlock //大缓冲区标志头 与实际缓冲区分开
{
    char *bigBufferStart;
    bigBlock*nextBigBlock;
};

class NginxMemoryPool
{
private:
    using smallBlockSize=unsigned long long;
    typedef smallBlock * sPointer;
    typedef bigBlock* bPointer;
    smallBlockSize bufferCapacity; // 小缓冲区大小
    sPointer currentFindLocation; // 当前查找位置,每次查找从这里开始
    bPointer bigBlockStart; // 大缓冲区 就是多个小缓冲区链

    /*我们希望在memory_pool保留一个指针锚点来指向第一个small_block而不
    是通过刚刚的指针加法来找到small_block的首地址，那便使用一个0长度的数组作为这个锚点.*/
    smallBlock smallBlockStart[0]; //小缓冲区开始
    
//-----------------------------上面是成员，下面是api--------------------------------------------
public:
    static NginxMemoryPool * createPool(smallBlockSize capacity);
    static void destroyPool(NginxMemoryPool * pool);
    static char* createNewSmallBlock(NginxMemoryPool * pool,smallBlockSize size);
    static char* createBigBlock(NginxMemoryPool * pool,smallBlockSize size);
    static void* poolMalloc(NginxMemoryPool * pool,smallBlockSize size);
    static void freeBigBlock(NginxMemoryPool * pool, char *buffer_ptr);
};



#endif //ALLOCATE_NGINXMEMORYPOOL_H
