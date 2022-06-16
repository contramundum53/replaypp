#pragma once
#include <concepts>
#include <mutex>
#include <variant>
#include <cstdint>
#include <optional>
#include <functional>
#include <cstdlib>
#include <iostream>
#include <condition_variable>

namespace replaypp::detail{
    struct dummy_class{};
    template<class... F>
    struct overload: public F...{
        using F::operator()...;
    };
    template<class... F>
    overload(F...)->overload<F...>;

    // FNV-1a 32bit hashing algorithm.
    constexpr inline std::uint32_t fnv1a_32(char const* s, std::size_t count){
        if(count <= 4){
            std::uint32_t ret = 0;
            for(int i=0; i<count-1; i++){
                ret |= s[i] << (i*8);
            }
            return ret;
        }else{
            return ((count ? fnv1a_32(s, count - 1) : 2166136261u) ^ s[count]) * 16777619u;
        }
    }
}

namespace replaypp{
    template<class T>
    concept WritableStorage = std::move_constructible<T> && requires (T storage){
        {storage.put(detail::dummy_class{})} -> std::same_as<void>;
    };
    template<class T>
    concept ReadableStorage = std::move_constructible<T> && requires (T storage){
        {storage.template get<detail::dummy_class>()} -> std::same_as<std::optional<detail::dummy_class>>;
    };

    /**
     * @brief Recording mode.
     * Specify a `WritableStorage` object to the constructor.
     */
    template<WritableStorage S>
    class record_mode{
        S m_storage;
        std::mutex m_mutex{};
        inline static std::int32_t g_next_thread_id = 0;
        inline static thread_local std::int32_t g_thread_id = -1;
    public:
        record_mode(S&& storage) : m_storage(std::move(storage)){
        }

        record_mode(record_mode&& other): m_storage{std::move(other.m_storage)}{}
        record_mode& operator=(record_mode&& other){
            std::scoped_lock<std::mutex, std::mutex> lock{m_mutex, other.m_mutex};
            m_storage = std::move(other.m_storage);
            return *this;
        }

        template<class F>
        auto record(std::uint32_t func_id, F&& f){
            if constexpr (std::is_void_v<decltype(std::forward<F>(f)())>){
                std::forward<F>(f)();
                std::scoped_lock<std::mutex> lock(m_mutex);
                if(g_thread_id == -1){
                    g_thread_id = g_next_thread_id++;
                }
                m_storage.put(g_thread_id);
                m_storage.put(func_id);
            }else{
                auto ret = std::forward<F>(f)();
                std::scoped_lock<std::mutex> lock(m_mutex);
                if(g_thread_id == -1){
                    g_thread_id = g_next_thread_id++;
                }
                m_storage.put(g_thread_id);
                m_storage.put(func_id);
                m_storage.put(ret);
                return ret;
            }
        }
    };


    /**
     * @brief Replaying mode.
     * Specify a `ReadableStorage` object and a `on_replay_end` callback to the constructor.
     */
    template<ReadableStorage S>
    class replay_mode{
        S m_storage;
        std::mutex m_mutex{};
        std::condition_variable m_cv{};
        std::function<void()> m_on_replay_end = []{};

        std::int32_t m_next_thread_id = -1;
        std::uint32_t m_next_func_id = 0;
        std::uint32_t m_gen = 0;

        inline static thread_local std::int32_t g_thread_id = -1;
    public:
        replay_mode(S&& storage, std::function<void()> on_replay_end = []{}) 
            : m_storage(std::move(storage)), 
              m_on_replay_end{std::move(on_replay_end)}
        {
        }

        replay_mode(replay_mode&& other): m_storage{std::move(other.m_storage)},
                                           m_on_replay_end{std::move(other.m_on_replay_end)},
                                            m_next_thread_id{other.m_next_thread_id},
                                            m_next_func_id{other.m_next_func_id}
        {
        }
        replay_mode& operator=(replay_mode&& other) {
            std::scoped_lock<std::mutex, std::mutex> lock{m_mutex, other.m_mutex};
            m_storage = std::move(other.m_storage);
            m_on_replay_end = std::move(other.m_on_replay_end);
            m_next_thread_id = other.m_next_thread_id;
            m_next_func_id = other.m_next_func_id;
            return *this;
        }

        template<class T>
        T replay(std::uint32_t func_id){
            std::unique_lock<std::mutex> lock(m_mutex);
            while(true){
                if(m_next_thread_id == -1){
                    std::optional<std::int32_t> next_thread_id_opt = m_storage.template get<std::int32_t>();
                    if(!next_thread_id_opt.has_value()){
                        m_on_replay_end();
                        std::exit(0);
                    }
                    m_next_thread_id = *next_thread_id_opt;
                    m_next_func_id = *m_storage.template get<std::uint32_t>();
                    // std::printf("[%d] next: %d %.4s\n", g_thread_id, m_next_thread_id, &m_next_func_id);
                }
                if(m_next_func_id == func_id){
                    if(g_thread_id == -1){
                        g_thread_id = m_next_thread_id;
                    }
                    if(m_next_thread_id == g_thread_id){
                        if constexpr(std::is_void_v<T>){
                            m_next_thread_id = -1;
                            m_next_func_id = 0;
                            m_cv.notify_all();
                            return;
                        }else{
                            T ret = *m_storage.template get<T>();
                            m_next_thread_id = -1;
                            m_next_func_id = std::uint32_t(-1);
                            m_cv.notify_all();
                            return ret;
                        }
                    }
                }

                if(g_thread_id == m_next_thread_id){
                    throw std::runtime_error("replay error: function id mismatch");
                }
                // std::printf("[%d] wait: %d %.4s\n", g_thread_id, m_next_thread_id, &m_next_func_id);
                m_cv.wait(lock, [&]{
                    return m_next_thread_id == -1 || ((g_thread_id == -1 || m_next_thread_id == g_thread_id) && m_next_func_id == func_id);
                });
                // std::printf("[%d] wait end: %d %.4s\n", g_thread_id, m_next_thread_id, &m_next_func_id);
            }
        }

    };


    /**
     * @brief A `replay` object switches its behavior depending on its mode.
     * In "no-op" mode (default constructor), the `wrap` function simply calls its argument.
     * In "record" mode (constructor with `record_mode` argument), the `wrap` function records the call and returns the result of the argument.
     * In "replay" mode (constructor with `replay_mode` argument), the `wrap` function returns the result of the recorded call.
     */
    template<WritableStorage RecordStorage, ReadableStorage ReplayStorage = RecordStorage>
    class replay{
        std::variant<std::monostate, record_mode<RecordStorage>, replay_mode<ReplayStorage>> m_mode;

        template<class F>
        auto wrap(std::uint32_t func_id, F&& f) -> std::decay_t<decltype(f())> {
            using T = std::decay_t<decltype(f())>;
            return std::visit(detail::overload{[&](std::monostate){
                return f();
            }, [&](record_mode<RecordStorage>& rec_mode){
                return rec_mode.record(func_id, std::forward<F>(f));
            }, [&](replay_mode<ReplayStorage>& rep_mode){
                return rep_mode.replay<T>(func_id);
            }}, m_mode);
        }
        
    public:
        constexpr replay() = default;
        replay(record_mode<RecordStorage>&& rec_mode) : m_mode(std::move(rec_mode)){
        }
        replay(replay_mode<ReplayStorage>&& rep_mode) : m_mode(std::move(rep_mode)){
        }
        constexpr replay(replay&& other) = default;
        constexpr replay& operator=(replay&& other) = default;

        
        /**
         * @brief Returns the result of the function call.
         * @param func_name The name of the function (for distinguishing between different calls). This must be a unique string.
         * @param f The function to be recorded.
         * @return The result of function call.
         * @note Be careful, any side-effects in the function is not recorded.
         */
        template<class F, std::size_t N>
        auto wrap(const char (&func_name)[N], F&& f) -> std::decay_t<decltype(f())> {
            const std::int32_t hash = detail::fnv1a_32(func_name, N-1);
            return wrap(hash, std::forward<F>(f));
        }

    };
}