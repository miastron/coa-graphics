#pragma once

namespace resource_retry
{
constexpr unsigned kShaders = 1;
constexpr unsigned kTargets = 2;
constexpr unsigned kNoise = 4;
constexpr UINT kNarrowerViewportWidth = 960;
constexpr float kNoiseAmount = 0.5f;

void CheckRetryGate()
{
    RetryGate gate;
    Check(gate.Ready(), "a new retry gate lets the first attempt through");
    gate.Failed();
    int blocked = 0;
    while (!gate.Ready() && blocked < 2 * kResourceRetryFrames)
        ++blocked;
    Check(blocked == kResourceRetryFrames - 1, "a retry gate blocks the frames after a failure and then retries");
    gate.Failed();
    gate.Reset();
    Check(gate.Ready(), "resetting a retry gate allows an immediate retry");
}

bool RenderFrame(Harness& h, Vec3 eye, Vec3 at, const float* proj, const D3DVIEWPORT9& viewport)
{
    float view[16];
    CameraRelativeLookAt(eye, at, view);
    const FrameInputs in = MakeInputs(view, proj, eye, at, viewport);
    h.BeginFrame();
    h.DrawScene(eye, view, proj, viewport);
    const char* skip = "";
    const bool rendered = vf_test_render(&in, &skip) != 0;
    h.dev->EndScene();
    h.dev->Present(nullptr, nullptr, nullptr, nullptr);
    return rendered;
}

struct Retries
{
    unsigned attemptsAfterFirstFrame = 0;
    unsigned attemptsBeforeRetry = 0;
    unsigned attemptsAfterRetry = 0;
    int rendersWhileFailing = 0;
    bool renderedOnRetry = false;
};

void Print(const char* name, const Retries& r)
{
    std::printf("     %s: %u attempt(s) after the first frame, %u after %d frames (%d rendered), %u on the next frame "
                "(rendered %d)\n",
                name, r.attemptsAfterFirstFrame, r.attemptsBeforeRetry, kResourceRetryFrames, r.rendersWhileFailing,
                r.attemptsAfterRetry, r.renderedOnRetry ? 1 : 0);
}

Retries FailThenRecover(Harness& h, Vec3 eye, Vec3 at, const float* proj, const D3DVIEWPORT9& viewport, unsigned kind)
{
    Retries retries;
    const unsigned started = vf_test_fog_resource_attempts(kind);
    vf_test_fail_fog_resources(kind);
    retries.rendersWhileFailing += RenderFrame(h, eye, at, proj, viewport) ? 1 : 0;
    retries.attemptsAfterFirstFrame = vf_test_fog_resource_attempts(kind) - started;
    for (int frame = 1; frame < kResourceRetryFrames; ++frame)
    {
        if (frame == kResourceRetryFrames - 1)
            vf_test_fail_fog_resources(0);
        retries.rendersWhileFailing += RenderFrame(h, eye, at, proj, viewport) ? 1 : 0;
    }
    retries.attemptsBeforeRetry = vf_test_fog_resource_attempts(kind) - started;
    retries.renderedOnRetry = RenderFrame(h, eye, at, proj, viewport);
    retries.attemptsAfterRetry = vf_test_fog_resource_attempts(kind) - started;
    Print(kind == kShaders ? "shaders" : (kind == kTargets ? "targets" : "noise"), retries);
    return retries;
}

bool FreshDevice(Harness& h, DWORD behaviour, ULONG& releasedReferences)
{
    h.ReleaseEngineObjects();
    releasedReferences += h.dev->Release();
    h.dev = nullptr;
    if (FAILED(h.d3d->CreateDevice(0, D3DDEVTYPE_HAL, h.window, behaviour, &h.pp, &h.dev)) || !h.dev)
        return false;
    h.CreateEngineObjects();
    return true;
}

void CheckFogResourceRetries(Harness& h, DWORD behaviour, Vec3 eye, Vec3 at, const float* proj,
                             const D3DVIEWPORT9& viewport, ULONG& releasedReferences)
{
    Retries shaders = FailThenRecover(h, eye, at, proj, viewport, kShaders);
    Check(shaders.attemptsAfterFirstFrame == 1 && shaders.attemptsBeforeRetry == 1 &&
              shaders.rendersWhileFailing == 0 && shaders.attemptsAfterRetry == 2 && shaders.renderedOnRetry,
          "a failed shader creation is retried after 120 frames, not every frame, and the fog draws again once "
          "it succeeds");

    D3DVIEWPORT9 narrower = viewport;
    narrower.Width = kNarrowerViewportWidth;
    Retries targets = FailThenRecover(h, eye, at, proj, narrower, kTargets);
    Check(targets.attemptsAfterFirstFrame == 1 && targets.attemptsBeforeRetry == 1 &&
              targets.rendersWhileFailing == 0 && targets.attemptsAfterRetry == 2 && targets.renderedOnRetry,
          "a failed render target creation is retried after 120 frames, not every frame, and the fog draws again "
          "once it succeeds");

    Config saved;
    vf_test_get_config(&saved);
    Config noisy = saved;
    noisy.noiseAmount = kNoiseAmount;
    vf_test_set_config(&noisy);
    const bool fresh = FreshDevice(h, behaviour, releasedReferences);
    Retries noise = FailThenRecover(h, eye, at, proj, viewport, kNoise);
    vf_test_set_config(&saved);
    Check(fresh && noise.attemptsAfterFirstFrame >= 1 && noise.attemptsBeforeRetry == noise.attemptsAfterFirstFrame &&
              noise.attemptsAfterRetry > noise.attemptsBeforeRetry,
          "a failed noise or local-light texture creation is retried after 120 frames, not every frame");
    vf_test_fail_fog_resources(0);
}
}
