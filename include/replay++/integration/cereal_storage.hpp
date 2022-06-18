#pragma once

#if !__has_include(<cereal/cereal.hpp>)
#error "cereal_integration.hpp requires cereal/cereal.hpp"
#endif
#include <iostream>
#include <memory>
#include <optional>
#include <cereal/cereal.hpp>

namespace replaypp{

    template<class Archive>
    struct cereal_writable_storage{
        std::unique_ptr<Archive> archive;
        cereal_writable_storage(std::unique_ptr<Archive>&& archive): archive(std::move(archive)){}
        cereal_writable_storage(cereal_writable_storage&&) = default;
        cereal_writable_storage& operator=(cereal_writable_storage&&) = default;

        template<class T>
        void put(const T& t){
            (*archive)(t);
        }
    };

    template<class Archive>
    struct cereal_readable_storage{
        std::unique_ptr<Archive> archive;
        cereal_readable_storage(std::unique_ptr<Archive>&& archive): archive(std::move(archive)){}
        cereal_readable_storage(cereal_readable_storage&&) = default;
        cereal_readable_storage& operator=(cereal_readable_storage&&) = default;

        template<class T>
        std::optional<T> get(){
            try{
                T x;
                (*archive)(x);
                return x;
            } catch(cereal::Exception& e){
                return std::nullopt;
            }
        }
    };
}