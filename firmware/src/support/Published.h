#pragma once

#include <cstdint>

#ifdef ARDUINO
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#else
#include <mutex>
#endif

// Ponte entre a tarefa de rede e a UI: a rede publica uma cópia, a UI lê quando desenha. Nenhum lado segura o outro
// por mais do que uma cópia de struct, e a UI só redesenha quando a versão mudou.
template <typename T>
class Published {
public:
    void publish(const T& value)
    {
        Lock lock(mutex);

        current = value;
        version++;
    }

    bool readIfChanged(T& out, uint32_t& seenVersion) const
    {
        Lock lock(mutex);

        if (version == seenVersion) {
            return false;
        }

        out = current;
        seenVersion = version;
        return true;
    }

    T read() const
    {
        Lock lock(mutex);

        return current;
    }

private:
#ifdef ARDUINO
    struct Mutex {
        SemaphoreHandle_t handle = xSemaphoreCreateMutex();
    };

    struct Lock {
        explicit Lock(const Mutex& mutex)
            : handle(mutex.handle)
        {
            xSemaphoreTake(handle, portMAX_DELAY);
        }

        ~Lock()
        {
            xSemaphoreGive(handle);
        }

        SemaphoreHandle_t handle;
    };
#else
    using Mutex = std::mutex;
    using Lock = std::lock_guard<std::mutex>;
#endif

    mutable Mutex mutex;
    T current{};
    uint32_t version = 0;
};
