#ifndef allocator_H_
#define allocator_H_

#include <climits>
#include <cstddef>
#include <type_traits>
#include <new>
#include <cmath>
#include <cstring>
#include <cstdlib>


using namespace std;

namespace myAllocator
{
    template<typename T, size_t BlockSize =1048576>  

    class allocator
    {
    public:
        /* Member types */
        using _Not_user_specialized = void;

        using value_type = T;

        typedef T* pointer;
        typedef const T* const_pointer;

        typedef T &reference;
        typedef const T &const_reference;

        typedef size_t size_type;
        typedef ptrdiff_t difference_type; // 机器数,记录两个指针距离(差)

        typedef true_type propagate_on_container_move_assignment; // 变量模板
        typedef true_type is_always_equal;

        template<typename U>
        struct rebind
        {
            typedef myAllocator::allocator<U> other;
        };

        /* Member functions。*/

        allocator() noexcept;


        ~allocator() noexcept;


        allocator(allocator &inputMP) noexcept;
        template<class U>
        allocator(const allocator<U> &inputMP) noexcept;
        pointer address(const_reference value) const noexcept;

        pointer address(reference value) const noexcept;


        pointer allocate(size_type count);

        pointer allocate(size_type count, const void *hint);


        void deallocate(pointer ptr, size_type count);

        size_type max_size() const noexcept;

        template<class _Objty, class... Tpes>
        void construct(_Objty *ptr, Tpes &&... _Args);
        template<class _Uty>
        void destroy(_Uty *ptr);

    private:

        struct Block // 内存块
        {
            Block *next; // 指向下一个内存块的指针
        };
        typedef Block *BlockPtr;
        
        struct freeList
        {
            pointer startBlockPtr; // 空闲内存块的起始地址
            size_type blockNum;
            freeList *next;
        };
        typedef freeList *freeListPtr;

        pointer currentBlockPtr; // 当前内存块的起始地址
        pointer lastBlockPtr; // 当前内存块的结束地址
        BlockPtr currentBlock; // 当前内存块的指针
        freeListPtr freeBlockPtr; // 空闲内存块的指针
        void allocateBlock();
    };


    template<class T1, class T2>
    typename myAllocator::allocator<T1>::propagate_on_container_move_assignment
    operator==(const allocator<T1> &lhs, const allocator<T2> &rhs);

    template<class T1, class T2>
    typename myAllocator::allocator<T1>::propagate_on_container_move_assignment
    operator!=(const allocator<T1> &lhs, const allocator<T2> &rhs);


    template<typename T, size_t BlockSize>
    allocator<T, BlockSize>::allocator() noexcept
    {
        currentBlock = nullptr;
        currentBlockPtr = nullptr;
        lastBlockPtr = nullptr;
        freeBlockPtr = nullptr;
    }


    template<typename T, size_t BlockSize>
    allocator<T, BlockSize>::~allocator() noexcept
    {
        BlockPtr currP = currentBlock;

        if (currP != nullptr)
        {
            BlockPtr nextP = currP->next;
            while (currP != nullptr)
            {
                ::operator delete(reinterpret_cast<void *>(currP));
                currP = nextP;
                if (currP != nullptr)
                    nextP = currP->next;
            }
        }
        if (freeBlockPtr != nullptr)
        {
            freeListPtr nextFP = freeBlockPtr->next;
            while (freeBlockPtr != nullptr)
            {
                free(freeBlockPtr);
                freeBlockPtr = nextFP;
                if (freeBlockPtr != nullptr)
                    nextFP = freeBlockPtr->next;
            }
        }
    }


    template<typename T, size_t BlockSize>
    allocator<T, BlockSize>::allocator(allocator &inputMP) noexcept // 拷贝构造函数
    {
        currentBlock = inputMP.currentBlock;
        currentBlockPtr = inputMP.currentBlockPtr;
        lastBlockPtr = inputMP.lastBlockPtr;
        freeBlockPtr = inputMP.freeBlockPtr;
        inputMP.currentBlock = nullptr;
    }

    template<typename T, size_t BlockSize>
    template<class U>
    allocator<T, BlockSize>::allocator(const allocator<U> &inputMP) noexcept: //默认拷贝构造函数
            allocator()
    {}

    template<typename T, size_t BlockSize>
    inline typename allocator<T, BlockSize>::pointer
    allocator<T, BlockSize>::address(const_reference value) const noexcept
    {
        return &value;
    }

    template<typename T, size_t BlockSize>
    inline
    typename allocator<T, BlockSize>::pointer
    allocator<T, BlockSize>::address(reference value) const noexcept
    {
        return &value;
    }

    template<typename T, size_t BlockSize>
    inline
    typename allocator<T, BlockSize>::pointer
    allocator<T, BlockSize>::allocate(size_type count) // 分配内存
    {
        if (count == 0)
            return nullptr;
        size_type needSize = count * sizeof(value_type);

        if (needSize > BlockSize) // 如果需要的内存大于一个内存块的大小，则直接分配
        {
            // 分配内存块,分配的内存块大小为所需内存大小+一个指向下一个内存块的指针+一个指向内存块起始地址的指针
            size_type requireSize = needSize + sizeof(BlockPtr) + sizeof(value_type);
            BlockPtr newBlock = reinterpret_cast<BlockPtr>(::operator new(requireSize));
            newBlock->next = currentBlock->next; // 将新分配的内存块插入到内存块链表中
            currentBlock->next = newBlock;     // 将新分配的内存块插入到内存块链表中

            // 分配内存块的起始地址
            BlockPtr body = reinterpret_cast<BlockPtr>(reinterpret_cast<size_type>(newBlock) + sizeof(BlockPtr));

            uintptr_t result = reinterpret_cast<uintptr_t>(body); // 将内存块的起始地址转换为无符号整型
            size_type bodyPadding = ((alignof(value_type) - result) % alignof(value_type)); // 计算内存块的起始地址需要对齐的字节数
            return reinterpret_cast<pointer>(newBlock + bodyPadding);
        }

        if (freeBlockPtr == nullptr)
        {
            if (lastBlockPtr - currentBlockPtr < count)
                allocateBlock();
            pointer returnP = reinterpret_cast<pointer>(currentBlockPtr);
            currentBlockPtr += count;
            return returnP;
        }
        else
        {
            freeListPtr curr = freeBlockPtr; // 遍历空闲内存块链表
            freeListPtr last = freeBlockPtr; // 遍历空闲内存块链表
            for (; curr != nullptr; last = curr, curr = curr->next) // 遍历空闲内存块链表
            {
                if (curr->blockNum > count) // 如果当前空闲内存块的大小大于所需内存大小，则将当前空闲内存块分割
                {
                    curr->blockNum -= count;
                    return curr->startBlockPtr + curr->blockNum; // 返回分配的内存块的起始地址
                }
                if (curr->blockNum == count) // 如果当前空闲内存块的大小等于所需内存大小，则将当前空闲内存块从空闲内存块链表中删除
                {
                    last->next = curr->next;
                    pointer currBlockPtr = curr->startBlockPtr;

                    if (curr == freeBlockPtr)
                        freeBlockPtr = curr->next;
                    free(curr);
                    return currBlockPtr;
                }
            }
            if (lastBlockPtr - currentBlockPtr < count)
                allocateBlock(); // 如果没有找到合适的空闲内存块，则分配一个新的内存块
            pointer returnP2 = reinterpret_cast<pointer>(currentBlockPtr); // 返回分配的内存块的起始地址
            currentBlockPtr += count;
            return returnP2;
        }
    }

    template<typename T, size_t BlockSize>
    inline
    typename allocator<T, BlockSize>::pointer
    allocator<T, BlockSize>::allocate(size_type count, const void *hint)
    {
        return allocate(count);
    }

    template<typename T, size_t BlockSize>
    inline void
    allocator<T, BlockSize>::deallocate(pointer ptr, size_type count)
    {
        if (count == 0 || ptr == nullptr) return;
        freeListPtr tmp = new freeList;
        tmp->startBlockPtr = ptr;
        tmp->blockNum = count;
        tmp->next = nullptr;
        if (freeBlockPtr == nullptr) freeBlockPtr = tmp;
        else if (freeBlockPtr->next == nullptr)
        {
            if (freeBlockPtr->startBlockPtr > tmp->startBlockPtr)
            {
                tmp->next = freeBlockPtr;
                freeBlockPtr = tmp;
            } else freeBlockPtr->next = tmp;
        }
        else
        {
            if (tmp->startBlockPtr < freeBlockPtr->startBlockPtr)
            {
                tmp->next = freeBlockPtr;
                freeBlockPtr = tmp;
                return;
            }
            freeListPtr curr = freeBlockPtr->next;
            freeListPtr last = freeBlockPtr;
            for (; curr != nullptr; last = curr, curr = curr->next)
            {
                if (curr->startBlockPtr > tmp->startBlockPtr && last->startBlockPtr < tmp->startBlockPtr)
                {
                    if (last->startBlockPtr + last->blockNum == tmp->startBlockPtr)
                    {
                        last->blockNum += tmp->blockNum;
                        if (tmp->startBlockPtr + tmp->blockNum == curr->startBlockPtr)
                        {
                            last->blockNum += curr->blockNum;
                            last->next = curr->next;
                            free(curr);
                        }
                        free(tmp);
                    }
                    else if (tmp->startBlockPtr + tmp->blockNum == curr->startBlockPtr)
                    {
                        tmp->blockNum += curr->blockNum;
                        last->next = tmp;
                        tmp->next = curr->next;
                        free(curr);
                    }
                    else
                    {
                        last->next = tmp;
                        tmp->next = curr;
                    }
                    return;
                }
            }
            last->next = tmp;
            return;
        }
    }


    template<typename T, size_t BlockSize>
    inline
    typename allocator<T, BlockSize>::size_type
    allocator<T, BlockSize>::max_size() const noexcept
    {
        size_type maxBlocks = -1 / BlockSize;
        return (BlockSize - sizeof(BlockPtr)) / sizeof(value_type) *maxBlocks;
    }
    template<typename T, size_t BlockSize>
    template<class _Objty, class... Tpes>
    inline void
    allocator<T, BlockSize>::construct(_Objty *ptr, Tpes &&... _Args)
    {
        new(ptr) _Objty(forward<Tpes>(_Args)...);
    }
    template<typename T, size_t BlockSize>
    template<class _Uty>
    inline
    void allocator<T, BlockSize>::destroy(_Uty *ptr)
    {
        ptr->~_Uty();
    }

    template<typename T, size_t BlockSize>
    inline void
    allocator<T, BlockSize>::allocateBlock()
    {
        BlockPtr newBlock = reinterpret_cast<BlockPtr>(operator new(BlockSize));
        newBlock->next = currentBlock;
        currentBlock = newBlock;
        BlockPtr body = reinterpret_cast<BlockPtr>(reinterpret_cast<size_type>(newBlock) + sizeof(BlockPtr));
        uintptr_t result = reinterpret_cast<uintptr_t>(body);
        size_type bodyPadding = ((alignof(value_type) - result) % alignof(value_type));
        currentBlockPtr = reinterpret_cast<pointer>(body + bodyPadding);  
        lastBlockPtr = reinterpret_cast<pointer>(newBlock + BlockSize - sizeof(value_type) + 1);  
    }

 
    template<class T1, class T2>
    inline
    typename myAllocator::allocator<T1>::propagate_on_container_move_assignment
    operator==(const allocator<T1> &lhs, const allocator<T2> &rhs)
    {
        return true;  
    }

    template<class T1, class T2>
    inline
    typename myAllocator::allocator<T1>::propagate_on_container_move_assignment
    operator!=(const allocator<T1> &lhs, const allocator<T2> &rhs)
    {
        return false;  
    }
};

#endif
