#ifndef UTILS_H
#define UTILS_H

#include <vector>
#include <algorithm>
#include <random>
#include <iostream>
#include <thread>
#include <functional>
#include <glm/glm.hpp>

template <typename T>
class InfiniteRandomIterator
{
    using VecType = std::vector<T>;
public:

    InfiniteRandomIterator(const VecType &v,size_t batch_size = 1) : v(v), b(batch_size), engine(42) {
        shuffleV();
    }

    void shuffleV(){
        std::shuffle(std::begin(v), std::end(v), engine);
        i = 0;
    }

    T next(){
        T ret = v[i++];
        if (i >= v.size()) shuffleV();
        return ret;
    }

    //std::tuple<size_t, size_t> next_batch() {
    //    T ret = v[i++];
    //    if (i >= v.size()) shuffleV();
    //    return ret;
    //}
private:
    VecType v;
    size_t i;
    size_t b;
    std::default_random_engine engine;
};

template <typename IndexType, typename FuncType>
void parallel_for(IndexType begin, IndexType end, FuncType func) {
    size_t range = end - begin;
    if (range <= 0) return;
    size_t numThreads = (std::min)(static_cast<size_t>(std::thread::hardware_concurrency()), range);
    size_t chunkSize = (range + numThreads - 1) / numThreads;
    std::vector<std::thread> threads;

    for (unsigned int i = 0; i < numThreads; i++) {
        IndexType chunkBegin = begin + i * chunkSize;
        IndexType chunkEnd = (std::min)(chunkBegin + chunkSize, end);

        threads.emplace_back([chunkBegin, chunkEnd, &func]() {
            for (IndexType item = chunkBegin; item < chunkEnd; item++) {
                func(*item);
            }
        });
    }

    for (std::thread& t : threads) {
        t.join();
    }
}

float fov2focal(float fov, float pixels);
float focal2fov(float focal, float pixels);
glm::mat4 getProjectionMatrix(float znear, float zfar, float fovX, float fovY);

#endif