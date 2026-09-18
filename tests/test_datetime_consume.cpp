#include <iostream>
#include <chrono>
#include <ctime>

// 探索性微基准：比较逐次转换和按秒缓存，不代表完整日志吞吐量。
// 输出尚未消费，空循环和部分计算可能被优化；倍率需用可靠基准复核。
void test_time(int times)
{
    const std::string& format = "%Y-%m-%d %H:%M:%S";
    static thread_local time_t last_second = 0;
    static thread_local char cached_date_time[20] = {'\0'};
    auto t1 = std::chrono::steady_clock::now();
    for(int i = 0; i < times; ++i)
    {
    }
    // 原始路径：每次取时后转换本地时间并格式化。
    auto t2 = std::chrono::steady_clock::now();
    for(int i=0; i < times;++i)
    {
        struct tm tm;
        time_t now = time(0);
        localtime_r(&now, &tm);
        char buf[64];
        strftime(buf, sizeof(buf), format.c_str(), &tm);
    }
    // 缓存路径：每次取时，仅秒数变化时更新日期字符串。
    auto t3 = std::chrono::steady_clock::now();
    for(int i = 0; i < times; ++i)
    {
        auto now = std::chrono::system_clock::now();
        auto duration = now.time_since_epoch();
        time_t current_second = std::chrono::duration_cast<std::chrono::seconds>(duration).count();
        long micros = std::chrono::duration_cast<std::chrono::microseconds>(duration % std::chrono::seconds(1)).count();

        /**
         * 转换与格式化的相对耗时依赖平台、构建选项和负载，不能固定为某个倍率。
         */
        if(current_second != last_second)
        {
            struct tm buf;
            localtime_r(&current_second, &buf);
            strftime(cached_date_time, sizeof(cached_date_time), format.c_str(), &buf);
            last_second = current_second;
        }
    }
    auto t4 = std::chrono::steady_clock::now();
    // 空循环可能被消除，扣除 base 不保证得到准确的单次调用成本。
    auto base = t2 - t1;
    auto diff1 = t3 - t2 - base;
    auto diff2 = t4 - t3 - base;

    auto base_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(base).count();
    auto diff1_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(diff1).count();
    auto diff2_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(diff2).count();

    std::cout << "base: " << base_ns << " ns" << std::endl;
    std::cout << "diff1(original): " << diff1_ns << " ns" << std::endl;
    std::cout << "diff2(optimized): " << diff2_ns << " ns" << std::endl;
    std::cout << "ratio(original/optimized): " << static_cast<double>(diff1_ns) / diff2_ns << std::endl;
}

int main(int argc, char* argv[])
{
    if(argc < 2)
    {
        std::cout << "Usage: " << argv[0] << " <times>" << std::endl;
        return 0;
    }

    int times = std::stoi(argv[1]);
    test_time(times);
    return 0;
}
