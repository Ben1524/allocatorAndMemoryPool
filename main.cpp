/*
#include <iostream>
#include <random>
#include <vector>
// include header of your allocator here
#include "allocator.h"


template<class T>
using MyAllocator = myAllocator::allocator<T>;



using Point2D = std::pair<int, int>;
const int TestSize = 10000;
const int PickSize = 1000;
int main()
{
std::random_device rd;
std::mt19937 gen(rd());
std::uniform_int_distribution<> dis(1, TestSize);
// vector creation
using IntVec = std::vector<int, MyAllocator<int>>;
std::vector<IntVec, MyAllocator<IntVec>> vecints(TestSize);
for (int i = 0; i < TestSize; i++)
vecints[i].resize(dis(gen));

using PointVec = std::vector<Point2D, MyAllocator<Point2D>>;
std::vector<PointVec, MyAllocator<PointVec>> vecpts(TestSize);
for (int i = 0; i < TestSize; i++)
    vecpts[i].resize(dis(gen));

// vector resize
for (int i = 0; i < PickSize; i++)
{
    int idx = dis(gen) - 1;
    int size = dis(gen);
    vecints[idx].resize(size);
    vecpts[idx].resize(size);
}
// vector element assignment
{
    int val = 10;
    int idx1 = dis(gen) - 1;
    int idx2 = vecints[idx1].size() / 2;
    vecints[idx1][idx2] = val;
    if (vecints[idx1][idx2] == val)
    std::cout << "correct assignment in vecints: " << idx1 << std::endl;
    else
    std::cout << "incorrect assignment in vecints: " << idx1 << std::endl;
}
{
    Point2D val(11, 15);
    int idx1 = dis(gen) - 1;
    int idx2 = vecpts[idx1].size() / 2;
    vecpts[idx1][idx2] = val;
    if (vecpts[idx1][idx2] == val)
    std::cout << "correct assignment in vecpts: " << idx1 << std::endl;
    else
    std::cout << "incorrect assignment in vecpts: " << idx1 << std::endl;
}
    return 0;
}*/
#include "NginxMemoryPool.h"
#include <iostream>
#include <vector>
#include <chrono>

using namespace std::chrono;
using namespace std;

NginxMemoryPool *pool = NginxMemoryPool::createPool(8192);

class test
{
private:
    int arr[10];
public:
    test()
    {
    }
    ~test()
    {
    }
    void * operator new(size_t size)
    {
        return NginxMemoryPool::poolMalloc(pool,size);
    }
    void operator delete(void *p)
    {
        NginxMemoryPool::freeBigBlock(pool,(char*)p);
    }
};

int main()
{
    vector<test*> vec;
    auto start = system_clock::now();
    for(int i=0;i<10000000;i++)
    {
        vec.push_back(new test());
    }
    auto end = system_clock::now();
    auto duration = duration_cast<microseconds>(end - start);
    cout <<  "use time: " << double(duration.count()) * microseconds::period::num / microseconds::period::den << "s" << endl;
    for(int i=0;i<10000000;i++)
    {
        delete vec[i];
    }
    NginxMemoryPool::destroyPool(pool);
}