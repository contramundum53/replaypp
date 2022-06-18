# replay++

A very simple header-only library for recording and replaying the results of function calls.

## Preparation

This is a header-only library. Simply add `include` directory in your include path.

Alternatively, if you use `cmake`, you can `add_subdirectory` this directory and use `replay++` target.

```cmake
add_subdirectory(path/to/this/directory)
target_link_library(your_target replay++)
```

## Example

In this example, we use `cereal` for serialization. Please install `cereal` to run this code.

```cpp

#include <replay++/replay++.hpp>
#include <replay++/integration/cereal_storage.hpp>
#include <cereal/archives/binary.hpp>

replaypp::replay replay;

// Say we want to record the results of a function returning timestamp.
#include <chrono>
#include <cstdint>

std::uint64_t timestamp(){
    return replay.wrap([]{
        return std::chrono::high_resolution_clock::now().time_since_epoch().count();
    });
}

// The code using the recorded function
void start(){
    std::uint64_t t1 = timestamp();
    // ...
    std::uint64_t t2 = timestamp();
    std::printf("%llu %llu\n", t1, t2);
}

// Record the function calls.
replay = {replaypp::record_mode{
        replaypp::cereal_writable_storage{
            std::make_unique<cereal::BinaryOutputArchive>(ss)}
    }};

start();

// Now, replay the function calls. The results should be exactly the same.
replay = {replaypp::replay_mode{
        replaypp::cereal_readable_storage{
            std::make_unique<cereal::BinaryInputArchive>(ss)}
    }};

start();

```
