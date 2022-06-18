#include <replay++/replay++.hpp>
#include <replay++/integration/cereal_storage.hpp>

#include <random>
#include <queue>
#include <thread>
#include <cstdio>
#include <cassert>

#include <cereal/cereal.hpp>
#include <cereal/archives/binary.hpp>
#include <sstream>

std::stringstream ss;

replaypp::replay<replaypp::cereal_writable_storage<cereal::BinaryOutputArchive>, 
                replaypp::cereal_readable_storage<cereal::BinaryInputArchive>> replay;

replaypp::recording_mutex<replay> mtx;
std::vector<std::int32_t> outputs;

std::int32_t func1(){
    return replay.wrap("fnc1", [&]{
        static std::mt19937 rng(1);
        std::this_thread::sleep_for(std::chrono::milliseconds(std::uniform_int_distribution{1, 100}(rng)));
        return std::uniform_int_distribution{1000, 1999}(rng);
    });
}

std::int32_t func2(){
    return replay.wrap("fnc2", [&]{
        static std::mt19937 rng(2);
        std::this_thread::sleep_for(std::chrono::milliseconds(std::uniform_int_distribution{1, 100}(rng)));
        return std::uniform_int_distribution{2000, 2999}(rng);
    });
}

std::int32_t func3(){
    return replay.wrap("fnc3", [&]{
        static std::mt19937 rng(3);
        std::this_thread::sleep_for(std::chrono::milliseconds(std::uniform_int_distribution{1, 100}(rng)));
        return std::uniform_int_distribution{3000, 3999}(rng);
    });
}

void start(){
    std::thread thread1{
        []{
            for(int i=0; i<20; i++){
                std::int32_t output = func1();
                std::scoped_lock lock{mtx};
                outputs.push_back(output);
                std::printf("[thread1] %d: %d\n", i, output);
            }
        }
    };

    std::thread thread2{
        []{
            for(int i=0; i<20; i++){
                std::int32_t output = func2();
                std::scoped_lock lock{mtx};
                outputs.push_back(output);
                std::printf("[thread2] %d: %d\n", i, output);
            }
        }
    };

    std::thread thread3{
        []{
            for(int i=0; i<20; i++){
                std::int32_t output = func3();
                std::scoped_lock lock{mtx};
                outputs.push_back(output);
                std::printf("[thread3] %d: %d\n", i, output);
            }
        }
    };
    thread1.join();
    thread2.join();
    thread3.join();
}

int main(){

    std::queue<std::uint32_t> m_data;
    replay = {replaypp::record_mode{
        replaypp::cereal_writable_storage{
            std::make_unique<cereal::BinaryOutputArchive>(ss)}
    }};
    std::printf("===== start =====\n");
    outputs.clear();
    start();

    auto outputs1 = outputs;

    replay = {replaypp::replay_mode{
        replaypp::cereal_readable_storage{
            std::make_unique<cereal::BinaryInputArchive>(ss)}
    }};
    std::printf("===== start2 =====\n");
    outputs.clear();
    start();
    auto outputs2 = outputs;
    assert(outputs1 == outputs2);
    std::printf("Test success.\n");
}