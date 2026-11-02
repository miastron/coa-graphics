#pragma once

#include <d3d9.h>

#include <cstdint>

class WaterVisibility
{
public:
    ~WaterVisibility();

    bool Hidden();
    void BeginCount(IDirect3DDevice9* dev);
    void EndCount(bool counted);
    void Forget();
    void Release();
    bool Counting() const { return m_open >= 0; }

private:
    struct Count
    {
        IDirect3DQuery9* query = nullptr;
        uint32_t generation = 0;
        bool pending = false;
        bool stale = false;
    };

    static constexpr int kCounts = 4;

    bool Prepare(IDirect3DDevice9* dev);
    void CollectFinished();

    Count m_counts[kCounts];
    int m_next = 0;
    int m_open = -1;
    uint32_t m_generation = 0;
    bool m_created = false;
    bool m_unsupported = false;
    bool m_hidden = false;
};
