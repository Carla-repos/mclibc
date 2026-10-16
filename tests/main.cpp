#include <atomic>
#include <cstdio>
#include <thread>

extern "C" {
#include "nashj.h"
#include "development.h"
}

#define mangled_n(id) nashj_##id
#define mangled_d(id) development_##id

int main() {
    auto nashj = mangled_n(start)(1024);
    auto first = mangled_n(alloc)(&nashj, 64);
    auto mem = mangled_n(alloc)(&nashj, 32);
    const char msg[] = "I love", second[] = "Samar";
    std::sprintf(static_cast<char*>(mem->ptr), "%s %s\n", msg, second);
    mangled_n(dump)(&nashj, first);

    void* before = mem->ptr;
    std::atomic<bool> go{false}, done{false};
    std::thread t([&] {
        while (!go.load()) std::this_thread::yield();

        void* ultimo = before;
        long leituras = 0;
        while(! done.load() ) {
            void* atual = *static_cast<void* volatile*>(&mem->ptr);
            ++leituras;
            if( atual != ultimo ) {
                std::printf("[thread] ptr mudou: %p -> %p (leitura %ld)\n", ultimo, atual, leituras);
                ultimo = atual;
            }
        }
        std::printf("[thread] fim: before=%p after=%p\n", before, mem->ptr);
        mangled_d(debug)(mem);
    });

    go = true;
    mangled_n(coalesce)(&nashj);
    std::printf("[main] reorganizado!\n");

    done = true;
    t.join();
    return 0;
}
