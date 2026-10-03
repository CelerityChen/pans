#include <iostream>
#include <thread>
#include <vector>

#include <pans/singleton.hpp>

// class A
// {
// public:
//     void log(const std::string &msg)
//     {
//         std::cout << "[LOG]: " << msg << std::endl;
//     }

// private:
//     A() = default;
//     ~A() = default;
//     A(const A &) = delete;
//     A(A &&) = delete;
//     A &operator=(const A &) = delete;
//     A &operator=(A &&) = delete;
//     friend class pans::Singleton<A>;
// };

// using SA = pans::Singleton<A>;

// int main()
// {
//     SA::Instance().log("Hello, World!");
//     return 0;
// }

class A : public pans::Singleton<A>
{
    friend class pans::Singleton<A>;
    public:
        void log(const std::string& msg)
        {
            std::cout << "[LOG]: " << msg << " -- data = " << m_data << std::endl;
        }

    private:
        A()
        {
            std::puts("A::A() -- only once");
            m_data++;
        }
        ~A()
        {
            std::puts("A::~A() -- only once");
        }

        int m_data{0};
};

// g++ -S test_singleton.cpp -o test_singleton.s -I../pans/include
int main()
{
    std::vector<std::thread> threads;
    threads.reserve(10);
    for(int i =0; i < 10; ++i)
    {
        threads.emplace_back([](){
            A::Instance().log("Hello, Meyers Singleton!");
        });
    }

    for(auto& thread : threads)
    {
        thread.join();
    }

    return 0;
}