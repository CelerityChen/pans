#ifndef PANS_INCLUDE_PANS_SINGLETON_HPP
#define PANS_INCLUDE_PANS_SINGLETON_HPP

#include <string>
#include <iostream>

namespace pans
{
    // class Singleton
    // {
    // public:
    //     Singleton(const Singleton &) = delete;
    //     Singleton(Singleton &&) = delete;
    //     Singleton &operator=(const Singleton &) = delete;
    //     Singleton &operator=(Singleton &&) = delete;

    //     static Singleton &Instance()
    //     {
    //         static Singleton instance;
    //         return instance;
    //     }

    //     void log(const std::string &msg)
    //     {
    //         std::cout << "[LOG]: " << msg << std::endl;
    //     }

    // private:
    //     Singleton() = default;
    //     ~Singleton() = default;
    // };

    template <typename T>
    class Singleton
    {
    public:
        /**
         * @brief 获取 T 的全局唯一实例。
         * @return T 的进程内唯一实例引用。
         */
        static inline T &Instance()
        {
            static T instance;
            return instance;
        }

    protected:
        Singleton() = default;
        ~Singleton() = default;

    private:
        Singleton(const Singleton &) = delete;
        Singleton(Singleton &&) = delete;
        Singleton &operator=(const Singleton &) = delete;
        Singleton &operator=(Singleton &&) = delete;
    };
}
#endif
