#include "water_visibility.h"

#include "d3d_util.h"
#include "log.h"

WaterVisibility::~WaterVisibility()
{
    Release();
}

void WaterVisibility::Release()
{
    for (Count& count : m_counts)
    {
        SafeRelease(count.query);
        count = {};
    }
    m_next = 0;
    m_open = -1;
    m_created = false;
    m_hidden = false;
}

void WaterVisibility::Forget()
{
    ++m_generation;
    m_hidden = false;
}

bool WaterVisibility::Prepare(IDirect3DDevice9* dev)
{
    if (m_created)
        return true;
    if (m_unsupported || !dev)
        return false;
    for (Count& count : m_counts)
    {
        const HRESULT hr = dev->CreateQuery(D3DQUERYTYPE_OCCLUSION, &count.query);
        if (FAILED(hr))
        {
            Release();
            m_unsupported = hr == D3DERR_NOTAVAILABLE;
            if (m_unsupported)
                VF_LOG_INFO("water: occlusion queries unavailable; hidden water is shaded like visible water");
            return false;
        }
    }
    m_created = true;
    return true;
}

void WaterVisibility::CollectFinished()
{
    for (int i = 0; i < kCounts; ++i)
    {
        Count& count = m_counts[(m_next + i) % kCounts];
        if (!count.pending)
            continue;
        DWORD pixels = 0;
        const HRESULT hr = count.query->GetData(&pixels, sizeof(pixels), 0);
        if (hr == S_FALSE)
            return;
        count.pending = false;
        if (hr == S_OK && !count.stale && count.generation == m_generation)
            m_hidden = pixels == 0;
    }
}

bool WaterVisibility::Hidden()
{
    if (m_created)
        CollectFinished();
    return m_hidden;
}

void WaterVisibility::BeginCount(IDirect3DDevice9* dev)
{
    m_open = -1;
    if (!Prepare(dev))
        return;
    CollectFinished();
    Count& count = m_counts[m_next];
    if (count.pending || FAILED(count.query->Issue(D3DISSUE_BEGIN)))
        return;
    count.generation = m_generation;
    count.stale = false;
    m_open = m_next;
    m_next = (m_next + 1) % kCounts;
}

void WaterVisibility::EndCount(bool counted)
{
    if (m_open < 0)
        return;
    Count& count = m_counts[m_open];
    m_open = -1;
    if (FAILED(count.query->Issue(D3DISSUE_END)))
        return;
    count.pending = true;
    count.stale = !counted;
}
