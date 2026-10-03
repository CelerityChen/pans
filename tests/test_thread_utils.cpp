#include <string>
#include <thread>
#include <iostream>
#include <unistd.h>

#if defined(__linux__) || defined(__APPLE__)
#include <pthread.h>
#endif

#include <utils/thread_utils.h>

int main()
{
    std::cout << "1: main thread id: " << pans::GetThreadId() << std::endl;
    std::cout << "2: main thread name: " << pans::GetThreadName() << std::endl;
    sleep(30);  // top -H -p <pid>
    pans::SetThreadName("main_thread");
    std::cout << "3: main thread name: " << pans::GetThreadName() << std::endl;
    sleep(10);
    pans::SetThreadName("Hello Thread");
    std::cout << "4: Hello Thread name: " << pans::GetThreadName() << std::endl;
    sleep(10);
    pans::SetThreadName("Test Thread");
    std::cout << "5: Test Thread name: " << pans::GetThreadName() << std::endl;
    sleep(10);
    std::thread worker([](){
        std::cout << "6: son thread id: " << pans::GetThreadId() << std::endl;
        std::cout << "7: son thread name: " << pans::GetThreadName() << std::endl;
        sleep(10);
        pans::SetThreadName("son-thread-long-name-for-testing-longlonglonglonglonglonglonglonglonglong");
        std::cout << "8: son thread name: " << pans::GetThreadName() << std::endl;
        sleep(10);
        pans::SetThreadName("");
        std::cout << "9: son thread name: " << pans::GetThreadName() << std::endl;
    });
    worker.join();

    return 0;
}