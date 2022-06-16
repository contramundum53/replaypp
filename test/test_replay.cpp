#include <replay++/replay++.hpp>

#include <random>
#include <queue>
#include <thread>
#include <cstdio>
#include <cassert>

struct MockStorage{
    std::queue<std::uint32_t>* m_data;
    template<class T>
    void put(const T& t){
        static_assert(std::is_convertible_v<T, std::uint32_t>);
        m_data->push(std::uint32_t(t));
    }
    template<class T>
    std::optional<T> get(){
        static_assert(std::is_convertible_v<std::uint32_t, T>);
        if(m_data->empty()){
            return std::nullopt;
        }
        auto ret = m_data->front();
        m_data->pop();
        return T(ret);
    }
};




replaypp::replay<MockStorage> replay;

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
    replay = std::move(replaypp::replay{replaypp::record_mode{MockStorage{&m_data}}});
    std::printf("===== start =====\n");
    outputs.clear();
    start();

    auto outputs1 = outputs;

    replay = {replaypp::replay_mode{MockStorage{&m_data}, [&]{
        assert(false);
    }}};
    std::printf("===== start2 =====\n");
    outputs.clear();
    start();
    auto outputs2 = outputs;
    assert(outputs1 == outputs2);
    std::printf("Test success.\n");
}