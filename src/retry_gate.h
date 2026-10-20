#pragma once

constexpr int kResourceRetryFrames = 120;

class RetryGate
{
public:
    bool Ready()
    {
        if (m_framesToWait == 0)
            return true;
        --m_framesToWait;
        return false;
    }

    void Failed() { m_framesToWait = kResourceRetryFrames - 1; }
    void Reset() { m_framesToWait = 0; }

private:
    int m_framesToWait = 0;
};
