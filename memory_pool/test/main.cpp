#include <thread>

#include "print.h"
#include "tnc_malloc.h"
#include "ScopeProfiler.h"

using namespace hnc::core::mem_pool;

void thread1() {
    for (int i = 0; i < 5; ++i) {
        tnc_malloc(6);
    }
}

void thread2() {
    for (int i = 0; i < 5; ++i) {
        tnc_malloc(7);
    }
}

void ThreadAllocTest() {
    std::thread t1(thread1);
    std::thread t2(thread2);
    t1.join();
    t2.join();
}

void OneThreadTest1 () {
    void* p1 = tnc_malloc(5);
    void* p2 = tnc_malloc(8);
    void* p3 = tnc_malloc(4);
    void* p4 = tnc_malloc(6);
    void* p5 = tnc_malloc(3);
    void* p6 = tnc_malloc(3);
    void* p7 = tnc_malloc(3);

    print(p1);
    print(p2);
    print(p3);
    print(p4);
    print(p5);
    print(p6);
    print(p7);


    tnc_free(p1);
    tnc_free(p2);
    tnc_free(p3);
    tnc_free(p4);
    tnc_free(p5);
    tnc_free(p6);
    tnc_free(p7);
}

void OneThreadTest2 () {
    for (int i = 0; i < 1024; ++i) {
        void *ptr = tnc_malloc(5);
        print(ptr);
    }

    void *ptr = tnc_malloc(3);
    print(ptr);
}
void MultiThreadAlloc1()
{
    std::vector<void*> v;
    for (size_t i = 0; i < 7; ++i) // 申请7次，正好单个线程能走到pc回收cc中span的那一步
    {
        void* ptr = tnc_malloc(6); // 申请的都是8B的块空间
        v.push_back(ptr);
    }

    for (const auto e : v)
    {
        tnc_free(e);
    }
}

void MultiThreadAlloc2()
{
    std::vector<void*> v;
    for (size_t i = 0; i < 7; ++i)
    {
        void* ptr = tnc_malloc(16); // 申请的都是16B的块空间
        v.push_back(ptr);
    }

    for (int i = 0; i < 7; ++i)
    {
        tnc_free(v[i]);
    }
}

#include "page_cache.h"
void TestMultiThread()
{
    std::thread t1(MultiThreadAlloc1);
    std::thread t2(MultiThreadAlloc2);

    t1.join();
    t2.join();
}

void BigAlloc() {
    void* p1 = tnc_malloc(257 * 1024);
    print(p1);
    tnc_free(p1);

    void* p2 = tnc_malloc(129 * 8 * 1024);
    print(p2);
    tnc_free(p2);

}


/*这里测试的是让多线程申请ntimes*rounds次，比较malloc和刚写完的ConcurrentAlloc的效率*/

/*比较的时候分两种情况，
一种是申请ntimes*rounds次同一个块大小的空间，
一种是申请ntimes*rounds次不同的块大小的空间*/

/*下面的代码稍微过一眼就好*/


// ntimes 一轮申请和释放内存的次数
// rounds 轮次
// nwors表示创建多少个线程
void BenchmarkMalloc(size_t ntimes, size_t nworks, size_t rounds)
{
	std::vector<std::thread> vthread(nworks);
	std::atomic<size_t> malloc_costtime = 0;
	std::atomic<size_t> free_costtime = 0;

	for (size_t k = 0; k < nworks; ++k)
	{
		vthread[k] = std::thread([&, k]() {
			std::vector<void*> v;
			v.reserve(ntimes);

			for (size_t j = 0; j < rounds; ++j)
			{
				size_t begin1 = clock();
				for (size_t i = 0; i < ntimes; i++)
				{
					//v.push_back(malloc(16)); // 每一次申请同一个桶中的块
					v.push_back(malloc((16 + i) % 8192 + 1));// 每一次申请不同桶中的块
				}
				size_t end1 = clock();

				size_t begin2 = clock();
				for (size_t i = 0; i < ntimes; i++)
				{
					free(v[i]);
				}
				size_t end2 = clock();
				v.clear();

				malloc_costtime += (end1 - begin1);
				free_costtime += (end2 - begin2);
			}
			});
	}

	for (auto& t : vthread)
	{
		t.join();
	}

	printf("%u个线程并发执行%u轮次，每轮次malloc %u次: 花费：%u ms\n",
		nworks, rounds, ntimes, malloc_costtime.load());

	printf("%u个线程并发执行%u轮次，每轮次free %u次: 花费：%u ms\n",
		nworks, rounds, ntimes, free_costtime.load());

	printf("%u个线程并发malloc&free %u次，总计花费：%u ms\n",
		nworks, nworks * rounds * ntimes, malloc_costtime.load() + free_costtime.load());
}


// 								单轮次申请释放次数 线程数 轮次
void BenchmarkConcurrentMalloc(size_t ntimes, size_t nworks, size_t rounds)
{
	std::vector<std::thread> vthread(nworks);
	std::atomic<size_t> malloc_costtime = 0;
	std::atomic<size_t> free_costtime = 0;

	for (size_t k = 0; k < nworks; ++k)
	{
		vthread[k] = std::thread([&]() {
			std::vector<void*> v;
			v.reserve(ntimes);

			for (size_t j = 0; j < rounds; ++j)
			{
				size_t begin1 = clock();
				for (size_t i = 0; i < ntimes; i++)
				{
					//v.push_back(ConcurrentAlloc(16));
					v.push_back(tnc_malloc((16 + i) % 8192 + 1));
				}
				size_t end1 = clock();

				size_t begin2 = clock();
				for (size_t i = 0; i < ntimes; i++)
				{
					tnc_free(v[i]);
				}
				size_t end2 = clock();
				v.clear();

				malloc_costtime += (end1 - begin1);
				free_costtime += (end2 - begin2);
			}
			});
	}

	for (auto& t : vthread)
	{
		t.join();
	}

	printf("%u个线程并发执行%u轮次，每轮次tnc_malloc %u次: 花费：%u ms\n",
		nworks, rounds, ntimes, malloc_costtime.load());

	printf("%u个线程并发执行%u轮次，每轮次tnc_free %u次: 花费：%u ms\n",
		nworks, rounds, ntimes, free_costtime.load());

	printf("%u个线程并发tnc_malloc&tnc_free %u次，总计花费：%u ms\n",
		nworks, nworks * rounds * ntimes, malloc_costtime.load() + free_costtime.load());
}

int main()
{
    using namespace std;
	size_t n = 1000;
	cout << "==========================================================" << endl;
	// 这里表示4个线程，每个线程申请10万次，总共申请40万次
	BenchmarkConcurrentMalloc(n, 4, 1);
	cout << endl << endl;

	// 这里表示4个线程，每个线程申请10万次，总共申请40万次
	BenchmarkMalloc(n, 4, 10);
	cout << "==========================================================" << endl;

    // ThreadAllocTest();
    // OneThreadTest1();
    // OneThreadTest2();
    // MultiThreadAlloc1();
    // MultiThreadAlloc2();
    // TestMultiThread();
    // BigAlloc();
	return 0;
}



