#include <iostream>
#include <vector>
#include <cassert>

#include "tnc_malloc.h"

using namespace hnc::core::mem_pool;
using namespace hnc::core::logger;

void test_class_new_delete() {
    std::cout << "\n[Test] class new/delete\n";
    class Test : public TncMemObj{
    public:
        int x;
        Test() : x(42) { std::cout << "Test Constructor\n"; }
        ~Test() { std::cout << "Test Destructor\n"; }
    };

    Test* obj = new Test();
    assert(obj);
    delete obj;
}

void test_global_malloc_dealloc() {
    std::cout << "\n[Test] global malloc/dealloc\n";

    // 小块内存
    void* ptr1 = tnc_malloc(126);
    std::cout << "small block = " << ptr1 << '\n';
    assert(ptr1 != nullptr);
    tnc_free(ptr1);

    // 直接向pc申请的内存
    void* ptr2 = tnc_malloc(256 * 1024 + 1);
    std::cout << "pc block = " << ptr2 << '\n';
    assert(ptr2 != nullptr);
    tnc_free(ptr2);

    // 直接向OS申请的 mmap内存
    void* ptr3 = tnc_malloc(128 * 1024 * 4 + 1);
    std::cout << "mmap block = " << ptr3 << '\n';
    assert(ptr3 != nullptr);
    tnc_free(ptr3);
}

void test_stl_allocator() {
    std::cout << "\n[Test] STL Allocator\n";

    std::vector<int, TncAllocator<int>> vec;
    vec.push_back(10);
    vec.push_back(20);

    assert(vec.size() == 2);
    assert(vec[0] == 10 && vec[1] == 20);
}

void test_pmr_stl_malloc_dealloc() {
    std::cout << "\n[Test] PMR Memory Resource\n";

    TncMemRe myResource;
    std::pmr::vector<int> pmr_vec(&myResource);

    pmr_vec.push_back(100);
    pmr_vec.push_back(200);

    assert(pmr_vec.size() == 2);
    assert(pmr_vec[0] == 100 && pmr_vec[1] == 200);
}



int main() {
    change_log_file_name("mem_pool/test_log");

    test_class_new_delete();
    test_global_malloc_dealloc();
    test_stl_allocator();
    test_pmr_stl_malloc_dealloc();

    std::cout << "\n[All Tests Passed]\n";
    return 0;
}
