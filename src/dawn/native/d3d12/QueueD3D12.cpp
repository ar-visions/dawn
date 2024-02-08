// Copyright 2017 The Dawn & Tint Authors
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "dawn/native/d3d12/QueueD3D12.h"

#include <utility>

#include "dawn/common/Math.h"
#include "dawn/native/CommandValidation.h"
#include "dawn/native/Commands.h"
#include "dawn/native/DynamicUploader.h"
#include "dawn/native/d3d/D3DError.h"
#include "dawn/native/d3d12/CommandAllocatorManager.h"
#include "dawn/native/d3d12/CommandBufferD3D12.h"
#include "dawn/native/d3d12/DeviceD3D12.h"
#include "dawn/native/d3d12/SharedFenceD3D12.h"
#include "dawn/native/d3d12/UtilsD3D12.h"
#include "dawn/platform/DawnPlatform.h"
#include "dawn/platform/tracing/TraceEvent.h"

namespace dawn::native::d3d12 {

// static
ResultOrError<Ref<Queue>> Queue::Create(Device* device, const QueueDescriptor* descriptor) {
    Ref<Queue> queue = AcquireRef(new Queue(device, descriptor));
    DAWN_TRY(queue->Initialize());
    return queue;
}

Queue::~Queue() {}

MaybeError Queue::Initialize() {
    SetLabelImpl();

    ID3D12Device* d3d12Device = ToBackend(GetDevice())->GetD3D12Device();

    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    DAWN_TRY(CheckHRESULT(d3d12Device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&mCommandQueue)),
                          "D3D12 create command queue"));

    // If PIX is not attached, the QueryInterface fails. Hence, no need to check the return
    // value.
    mCommandQueue.As(&mD3d12SharingContract);

    DAWN_TRY(CheckHRESULT(d3d12Device->CreateFence(uint64_t(kBeginningOfGPUTime),
                                                   D3D12_FENCE_FLAG_SHARED, IID_PPV_ARGS(&mFence)),
                          "D3D12 create fence"));

    mFenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    DAWN_ASSERT(mFenceEvent != nullptr);

    DAWN_TRY_ASSIGN(mSharedFence, SharedFence::Create(ToBackend(GetDevice()),
                                                      "Internal shared DXGI fence", mFence));

    // TODO(dawn:1413): Consider folding the command allocator manager in this class.
    mCommandAllocatorManager = std::make_unique<CommandAllocatorManager>(this);
    return {};
}

void Queue::DestroyImpl() {
    // Immediately forget about all pending commands for the case where device is lost on its
    // own and WaitForIdleForDestruction isn't called.
    DAWN_ASSERT(!mPendingCommands.IsOpen());
    mPendingCommands.Release();

    if (mFenceEvent != nullptr) {
        ::CloseHandle(mFenceEvent);
        mFenceEvent = nullptr;
    }

    // Release the shared fence here to prevent a ref-cycle with the device, but do not destroy the
    // underlying native fence so that we can return a SharedFence on EndAccess after destruction.
    mSharedFence = nullptr;

    mCommandQueue.Reset();
}

ResultOrError<Ref<d3d::SharedFence>> Queue::GetOrCreateSharedFence() {
    if (mSharedFence == nullptr) {
        DAWN_ASSERT(!IsAlive());
        return SharedFence::Create(ToBackend(GetDevice()), "Internal shared DXGI fence", mFence);
    }
    return mSharedFence;
}

ID3D12CommandQueue* Queue::GetCommandQueue() const {
    return mCommandQueue.Get();
}

ID3D12SharingContract* Queue::GetSharingContract() const {
    return mD3d12SharingContract.Get();
}

MaybeError Queue::SubmitImpl(uint32_t commandCount, CommandBufferBase* const* commands) {
    CommandRecordingContext* commandContext;
    DAWN_TRY_ASSIGN(commandContext, GetPendingCommandContext());

    TRACE_EVENT_BEGIN1(GetDevice()->GetPlatform(), Recording, "CommandBufferD3D12::RecordCommands",
                       "serial", uint64_t(GetPendingCommandSerial()));
    for (uint32_t i = 0; i < commandCount; ++i) {
        DAWN_TRY(ToBackend(commands[i])->RecordCommands(commandContext));
    }
    TRACE_EVENT_END1(GetDevice()->GetPlatform(), Recording, "CommandBufferD3D12::RecordCommands",
                     "serial", uint64_t(GetPendingCommandSerial()));

    return SubmitPendingCommands();
}

MaybeError Queue::SubmitPendingCommands() {
    Device* device = ToBackend(GetDevice());
    DAWN_ASSERT(device->IsLockedByCurrentThreadIfNeeded());

    if (!mPendingCommands.IsOpen() || !mPendingCommands.NeedsSubmit()) {
        return {};
    }

    DAWN_TRY(mCommandAllocatorManager->Tick(GetCompletedCommandSerial()));
    DAWN_TRY(mPendingCommands.ExecuteCommandList(device, mCommandQueue.Get()));
    return NextSerial();
}

MaybeError Queue::NextSerial() {
    Device* device = ToBackend(GetDevice());
    DAWN_ASSERT(device->IsLockedByCurrentThreadIfNeeded());
    // NextSerial should not ever be called with an open command list since the underlying command
    // allocator could be recycled after the serial completes on the GPU.
    DAWN_ASSERT(!mPendingCommands.IsOpen());

    IncrementLastSubmittedCommandSerial();

    TRACE_EVENT1(device->GetPlatform(), General, "D3D12Device::SignalFence", "serial",
                 uint64_t(GetLastSubmittedCommandSerial()));

    return CheckHRESULT(
        mCommandQueue->Signal(mFence.Get(), uint64_t(GetLastSubmittedCommandSerial())),
        "D3D12 command queue signal fence");
}

MaybeError Queue::WaitForSerial(ExecutionSerial serial) {
    if (GetCompletedCommandSerial() >= serial) {
        return {};
    }
    DAWN_TRY(CheckHRESULT(mFence->SetEventOnCompletion(uint64_t(serial), mFenceEvent),
                          "D3D12 set event on completion"));
    WaitForSingleObject(mFenceEvent, INFINITE);
    DAWN_TRY(CheckPassedSerials());
    return {};
}

bool Queue::HasPendingCommands() const {
    return mPendingCommands.NeedsSubmit();
}

ResultOrError<ExecutionSerial> Queue::CheckAndUpdateCompletedSerials() {
    ExecutionSerial completedSerial = ExecutionSerial(mFence->GetCompletedValue());
    if (DAWN_UNLIKELY(completedSerial == ExecutionSerial(UINT64_MAX))) {
        // GetCompletedValue returns UINT64_MAX if the device was removed.
        // Try to query the failure reason.
        ID3D12Device* d3d12Device = ToBackend(GetDevice())->GetD3D12Device();
        DAWN_TRY(CheckHRESULT(d3d12Device->GetDeviceRemovedReason(),
                              "ID3D12Device::GetDeviceRemovedReason"));
        // Otherwise, return a generic device lost error.
        return DAWN_DEVICE_LOST_ERROR("Device lost");
    }

    if (completedSerial <= GetCompletedCommandSerial()) {
        return ExecutionSerial(0);
    }

    return completedSerial;
}

void Queue::ForceEventualFlushOfCommands() {
    if (mPendingCommands.IsOpen()) {
        mPendingCommands.SetNeedsSubmit();
    }
}

MaybeError Queue::WaitForIdleForDestruction() {
    // Immediately forget about all pending commands
    mPendingCommands.Release();

    DAWN_TRY(NextSerial());
    // Wait for all in-flight commands to finish executing
    DAWN_TRY(WaitForSerial(GetLastSubmittedCommandSerial()));

    return {};
}

ResultOrError<CommandRecordingContext*> Queue::GetPendingCommandContext(SubmitMode submitMode) {
    Device* device = ToBackend(GetDevice());
    ID3D12Device* d3d12Device = device->GetD3D12Device();

    // Callers of GetPendingCommandList do so to record commands. Only reserve a command
    // allocator when it is needed so we don't submit empty command lists
    if (!mPendingCommands.IsOpen()) {
        DAWN_TRY(mPendingCommands.Open(d3d12Device, mCommandAllocatorManager.get()));
    }
    if (submitMode == SubmitMode::Normal) {
        mPendingCommands.SetNeedsSubmit();
    }
    return &mPendingCommands;
}

void Queue::SetLabelImpl() {
    Device* device = ToBackend(GetDevice());
    // TODO(crbug.com/dawn/1344): When we start using multiple queues this needs to be adjusted
    // so it doesn't always change the default queue's label.
    SetDebugName(device, mCommandQueue.Get(), "Dawn_Queue", GetLabel());
}

void Queue::SetEventOnCompletion(ExecutionSerial serial, HANDLE event) {
    mFence->SetEventOnCompletion(static_cast<uint64_t>(serial), event);
}

}  // namespace dawn::native::d3d12
