// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/gpu/wayland_buffer_manager_gpu.h"
#include "ui/ozone/platform/wayland/host/wayland_buffer_manager_host.h"

#include <drm_fourcc.h>
#include <memory>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/test/mock_callback.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/ozone/platform/wayland/gpu/wayland_surface_gpu.h"
#include "ui/ozone/platform/wayland/test/mock_surface.h"
#include "ui/ozone/platform/wayland/test/mock_zwp_linux_buffer_params.h"
#include "ui/ozone/platform/wayland/test/mock_zwp_linux_dmabuf.h"
#include "ui/ozone/platform/wayland/test/wayland_test.h"

using testing::_;

namespace ui {

namespace {

using MockTerminateGpuCallback =
    base::MockCallback<base::OnceCallback<void(std::string)>>;

constexpr gfx::Size kDefaultSize(1024, 768);

struct InputData {
  bool has_file = false;
  gfx::Size size;
  uint32_t planes_count = 0;
  std::vector<uint32_t> strides;
  std::vector<uint32_t> offsets;
  std::vector<uint64_t> modifiers;
  uint32_t format = 0;
  uint32_t buffer_id = 0;
};

class MockSurfaceGpu : public WaylandSurfaceGpu {
 public:
  MockSurfaceGpu() = default;
  ~MockSurfaceGpu() override = default;

  MOCK_METHOD2(OnSubmission,
               void(uint32_t buffer_id, const gfx::SwapResult& swap_result));
  MOCK_METHOD2(OnPresentation,
               void(uint32_t buffer_id,
                    const gfx::PresentationFeedback& feedback));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockSurfaceGpu);
};

}  // namespace

class WaylandBufferManagerTest : public WaylandTest {
 public:
  WaylandBufferManagerTest() = default;
  ~WaylandBufferManagerTest() override = default;

  void SetUp() override {
    WaylandTest::SetUp();

    manager_host_ = connection_->buffer_manager_host();
    EXPECT_TRUE(manager_host_);

    // Use the helper methods below, which automatically set the termination
    // callback and bind the interface again if the manager failed.
    manager_host_->SetTerminateGpuCallback(callback_.Get());
    auto interface_ptr = manager_host_->BindInterface();
    buffer_manager_gpu_->SetWaylandBufferManagerHost(std::move(interface_ptr));
  }

 protected:
  base::ScopedFD MakeFD() {
    base::FilePath temp_path;
    EXPECT_TRUE(base::CreateTemporaryFile(&temp_path));
    auto file =
        base::File(temp_path, base::File::FLAG_READ | base::File::FLAG_WRITE |
                                  base::File::FLAG_CREATE_ALWAYS);
    return base::ScopedFD(file.TakePlatformFile());
  }

  // Sets the terminate gpu callback expectation, calls OnChannelDestroyed,
  // sets the same callback again and re-establishes mojo connection again
  // for convenience.
  void SetTerminateCallbackExpectationAndDestroyChannel(
      MockTerminateGpuCallback* callback,
      bool fail) {
    if (!fail) {
      // To avoid warning messages as "Expected to be never called, but has 0
      // WillOnce()s", split the expecations based on the expected call times.
      EXPECT_CALL(*callback, Run(_)).Times(0);
    } else {
      EXPECT_CALL(*callback, Run(_))
          .Times(1)
          .WillRepeatedly(::testing::Invoke([this, callback](std::string) {
            manager_host_->OnChannelDestroyed();
            manager_host_->SetTerminateGpuCallback(callback->Get());

            auto interface_ptr = manager_host_->BindInterface();
            buffer_manager_gpu_->SetWaylandBufferManagerHost(
                std::move(interface_ptr));
          }));
    }
  }

  void CreateDmabufBasedBufferAndSetTerminateExpecation(
      bool fail,
      gfx::AcceleratedWidget widget,
      uint32_t buffer_id,
      base::ScopedFD fd = base::ScopedFD(),
      const gfx::Size& size = kDefaultSize,
      const std::vector<uint32_t>& strides = {1},
      const std::vector<uint32_t>& offsets = {2},
      const std::vector<uint64_t>& modifiers = {3},
      uint32_t format = DRM_FORMAT_R8,
      uint32_t planes_count = 1) {
    if (!fd.is_valid())
      fd = MakeFD();

    SetTerminateCallbackExpectationAndDestroyChannel(&callback_, fail);
    buffer_manager_gpu_->CreateDmabufBasedBuffer(
        widget, std::move(fd), kDefaultSize, strides, offsets, modifiers,
        format, planes_count, buffer_id);

    Sync();
  }

  void CreateShmBasedBufferAndSetTerminateExpecation(
      bool fail,
      gfx::AcceleratedWidget widget,
      uint32_t buffer_id,
      const gfx::Size& size = kDefaultSize,
      size_t length = 0) {
    SetTerminateCallbackExpectationAndDestroyChannel(&callback_, fail);

    if (!length)
      length = size.width() * size.height() * 4;
    buffer_manager_gpu_->CreateShmBasedBuffer(widget, MakeFD(), length, size,
                                              buffer_id);

    Sync();
  }

  void DestroyBufferAndSetTerminateExpectation(gfx::AcceleratedWidget widget,
                                               uint32_t buffer_id,
                                               bool fail) {
    SetTerminateCallbackExpectationAndDestroyChannel(&callback_, fail);

    buffer_manager_gpu_->DestroyBuffer(widget, buffer_id);

    Sync();
  }

  void ProcessCreatedBufferResourcesWithExpectation(size_t expected_size,
                                                    bool fail) {
    auto params_vector = server_.zwp_linux_dmabuf_v1()->buffer_params();
    // To ensure, no other buffers are created, test the size of the vector.
    EXPECT_EQ(params_vector.size(), expected_size);

    for (auto* mock_params : params_vector) {
      if (!fail) {
        zwp_linux_buffer_params_v1_send_created(mock_params->resource(),
                                                mock_params->buffer_resource());
      } else {
        zwp_linux_buffer_params_v1_send_failed(mock_params->resource());
      }
    }
  }

  MockTerminateGpuCallback callback_;
  WaylandBufferManagerHost* manager_host_;

 private:
  DISALLOW_COPY_AND_ASSIGN(WaylandBufferManagerTest);
};

TEST_P(WaylandBufferManagerTest, CreateDmabufBasedBuffers) {
  constexpr uint32_t kDmabufBufferId = 1;

  EXPECT_CALL(*server_.zwp_linux_dmabuf_v1(), CreateParams(_, _, _)).Times(1);
  const gfx::AcceleratedWidget widget = window_->GetWidget();

  CreateDmabufBasedBufferAndSetTerminateExpecation(false /*fail*/, widget,
                                                   kDmabufBufferId);
  DestroyBufferAndSetTerminateExpectation(widget, kDmabufBufferId,
                                          false /*fail*/);
}

TEST_P(WaylandBufferManagerTest, CreateShmBasedBuffers) {
  constexpr uint32_t kShmBufferId = 1;

  const gfx::AcceleratedWidget widget = window_->GetWidget();

  CreateShmBasedBufferAndSetTerminateExpecation(false /*fail*/, widget,
                                                kShmBufferId);
  // The state is reset now and there are no buffers to destroy.
  DestroyBufferAndSetTerminateExpectation(widget, kShmBufferId, false /*fail*/);
}

TEST_P(WaylandBufferManagerTest, ValidateDataFromGpu) {
  const InputData kBadInputs[] = {
      // All zeros.
      {},
      // Valid file but zeros everywhereelse.
      {true},
      // Valid file, invalid size, zeros elsewhere.
      {true, {kDefaultSize.width(), 0}},
      {true, {0, kDefaultSize.height()}},
      // Valid file and size but zeros in other fields.
      {true, kDefaultSize},
      // Vectors have different lengths.
      {true, kDefaultSize, 1, {1}, {2, 3}, {4, 5, 6}},
      // Vectors have same lengths but strides have a zero.
      {true, kDefaultSize, 1, {0}, {2}, {6}},
      // Vectors are valid but buffer format is not.
      {true, kDefaultSize, 1, {1}, {2}, {6}},
      // Everything is correct but the buffer ID is zero.
      {true, kDefaultSize, 1, {1}, {2}, {6}, DRM_FORMAT_R8},
  };

  const gfx::AcceleratedWidget widget = window_->GetWidget();
  for (const auto& bad : kBadInputs) {
    EXPECT_CALL(*server_.zwp_linux_dmabuf_v1(), CreateParams(_, _, _)).Times(0);
    base::ScopedFD dummy;
    CreateDmabufBasedBufferAndSetTerminateExpecation(
        true /*fail*/, widget, bad.buffer_id,
        bad.has_file ? MakeFD() : std::move(dummy), bad.size, bad.strides,
        bad.offsets, bad.modifiers, bad.format, bad.planes_count);
  }

  constexpr uint32_t kBufferId = 1;

  // Create a buffer so it gets registered with the given ID.
  // This must be the only buffer that is asked to be created.
  EXPECT_CALL(*server_.zwp_linux_dmabuf_v1(), CreateParams(_, _, _)).Times(1);
  CreateDmabufBasedBufferAndSetTerminateExpecation(false /*fail*/, widget,
                                                   kBufferId);

  // It must be impossible to create a buffer with the same id.
  EXPECT_CALL(*server_.zwp_linux_dmabuf_v1(), CreateParams(_, _, _)).Times(0);
  CreateDmabufBasedBufferAndSetTerminateExpecation(true /*fail*/, widget,
                                                   kBufferId);

  // Create the buffer again and try to destroy it.
  EXPECT_CALL(*server_.zwp_linux_dmabuf_v1(), CreateParams(_, _, _)).Times(1);
  CreateDmabufBasedBufferAndSetTerminateExpecation(false /*fail*/, widget,
                                                   kBufferId);

  // The destruction of the previously created buffer must be ok.
  DestroyBufferAndSetTerminateExpectation(widget, kBufferId, false /*fail*/);

  // Destroying non-existing buffer triggers the termination callback.
  DestroyBufferAndSetTerminateExpectation(widget, kBufferId, true /*fail*/);
}

TEST_P(WaylandBufferManagerTest, CreateAndDestroyBuffer) {
  const uint32_t kBufferId1 = 1;
  const uint32_t kBufferId2 = 2;

  const gfx::AcceleratedWidget widget = window_->GetWidget();

  // This section tests that it is impossible to create buffers with the same
  // id.
  {
    EXPECT_CALL(*server_.zwp_linux_dmabuf_v1(), CreateParams(_, _, _)).Times(2);
    CreateDmabufBasedBufferAndSetTerminateExpecation(false /*fail*/, widget,
                                                     kBufferId1);
    CreateDmabufBasedBufferAndSetTerminateExpecation(false /*fail*/, widget,
                                                     kBufferId2);

    // Can't create buffer with existing id.
    CreateDmabufBasedBufferAndSetTerminateExpecation(true /*fail*/, widget,
                                                     kBufferId2);
    // Can't destroy buffer with non-existing id (the manager cleared the state
    // after the previous failure).
    DestroyBufferAndSetTerminateExpectation(widget, kBufferId2, true /*fail*/);
  }

  // This section tests that it is impossible to destroy buffers with
  // non-existing ids (for example, if the have already been destroyed).
  {
    EXPECT_CALL(*server_.zwp_linux_dmabuf_v1(), CreateParams(_, _, _)).Times(1);
    CreateDmabufBasedBufferAndSetTerminateExpecation(false /*fail*/, widget,
                                                     kBufferId2);
    DestroyBufferAndSetTerminateExpectation(widget, kBufferId2, false /*fail*/);
    // Can't destroy the same buffer twice (non-existing id).
    DestroyBufferAndSetTerminateExpectation(widget, kBufferId2, true /*fail*/);
  }
}

TEST_P(WaylandBufferManagerTest, EnsureCorrectOrderOfCallbacks) {
  constexpr uint32_t kBufferId1 = 1;
  constexpr uint32_t kBufferId2 = 2;

  const gfx::AcceleratedWidget widget = window_->GetWidget();
  const gfx::Rect bounds = gfx::Rect({0, 0}, kDefaultSize);
  window_->SetBounds(bounds);

  MockSurfaceGpu mock_surface_gpu;
  buffer_manager_gpu_->RegisterSurface(widget, &mock_surface_gpu);

  auto* linux_dmabuf = server_.zwp_linux_dmabuf_v1();
  EXPECT_CALL(*linux_dmabuf, CreateParams(_, _, _)).Times(2);
  CreateDmabufBasedBufferAndSetTerminateExpecation(false /*fail*/, widget,
                                                   kBufferId1);
  CreateDmabufBasedBufferAndSetTerminateExpecation(false /*fail*/, widget,
                                                   kBufferId2);

  Sync();

  ProcessCreatedBufferResourcesWithExpectation(2u /* expected size */,
                                               false /* fail */);

  auto* mock_surface = server_.GetObject<wl::MockSurface>(widget);

  constexpr uint32_t kNumberOfCommits = 3;
  EXPECT_CALL(*mock_surface, Attach(_, _, _)).Times(kNumberOfCommits);
  EXPECT_CALL(*mock_surface, Frame(_)).Times(kNumberOfCommits);
  EXPECT_CALL(*mock_surface, Commit()).Times(kNumberOfCommits);

  // All the other expectations must come in order.
  ::testing::InSequence sequence;
  EXPECT_CALL(mock_surface_gpu,
              OnSubmission(kBufferId1, gfx::SwapResult::SWAP_ACK))
      .Times(1);
  // wp_presentation must not exist now. This means that the buffer
  // manager must send synthetized presentation feedbacks.
  ASSERT_TRUE(!connection_->presentation());
  EXPECT_CALL(mock_surface_gpu, OnPresentation(kBufferId1, _)).Times(1);

  buffer_manager_gpu_->CommitBuffer(widget, kBufferId1, bounds);

  Sync();

  // As long as there hasn't any previous buffer attached (nothing to release
  // yet), it must be enough to just send a frame callback back.
  mock_surface->SendFrameCallback();

  Sync();

  // Commit second buffer now.
  buffer_manager_gpu_->CommitBuffer(widget, kBufferId2, bounds);

  Sync();

  EXPECT_CALL(mock_surface_gpu,
              OnSubmission(kBufferId2, gfx::SwapResult::SWAP_ACK))
      .Times(1);
  EXPECT_CALL(mock_surface_gpu, OnPresentation(kBufferId2, _)).Times(1);

  mock_surface->ReleasePrevAttachedBuffer();
  mock_surface->SendFrameCallback();

  Sync();

  // wp_presentation is available now.
  auto* mock_wp_presentation = server_.EnsureWpPresentation();
  ASSERT_TRUE(mock_wp_presentation);

  Sync();

  // Now, the wp_presentation object exists and there must be a real feedback
  // sent. Ensure the order now.
  ASSERT_TRUE(connection_->presentation());

  EXPECT_CALL(*mock_wp_presentation,
              Feedback(_, _, mock_surface->resource(), _))
      .Times(1);

  // Commit second buffer now.
  buffer_manager_gpu_->CommitBuffer(widget, kBufferId1, bounds);

  Sync();

  // Even though, the server send the presentation feeedback, the host manager
  // must make sure the order of the submission and presentation callbacks is
  // correct. Thus, no callbacks must be received by the MockSurfaceGpu.
  EXPECT_CALL(mock_surface_gpu, OnSubmission(_, _)).Times(0);
  EXPECT_CALL(mock_surface_gpu, OnPresentation(_, _)).Times(0);

  mock_wp_presentation->SendPresentationCallback();

  Sync();

  EXPECT_CALL(mock_surface_gpu,
              OnSubmission(kBufferId1, gfx::SwapResult::SWAP_ACK))
      .Times(1);
  EXPECT_CALL(mock_surface_gpu, OnPresentation(kBufferId1, _)).Times(1);

  // Now, send the release callback. The host manager must send the submission
  // and presentation callbacks in correct order.
  mock_surface->ReleasePrevAttachedBuffer();

  Sync();
}

TEST_P(WaylandBufferManagerTest, TestCommitBufferConditions) {
  constexpr uint32_t kDmabufBufferId = 1;
  constexpr uint32_t kDmabufBufferId2 = 2;

  const gfx::AcceleratedWidget widget = window_->GetWidget();
  auto* mock_surface = server_.GetObject<wl::MockSurface>(widget);

  auto* linux_dmabuf = server_.zwp_linux_dmabuf_v1();
  EXPECT_CALL(*linux_dmabuf, CreateParams(_, _, _)).Times(1);

  CreateDmabufBasedBufferAndSetTerminateExpecation(false /*fail*/, widget,
                                                   kDmabufBufferId);

  // Part 1: the surface mustn't have a buffer attached until
  // zwp_linux_buffer_params_v1_send_created is called. Instead, the buffer must
  // be set as pending buffer.

  EXPECT_CALL(*mock_surface, Attach(_, _, _)).Times(0);
  EXPECT_CALL(*mock_surface, Frame(_)).Times(0);
  EXPECT_CALL(*mock_surface, Commit()).Times(0);

  buffer_manager_gpu_->CommitBuffer(widget, kDmabufBufferId,
                                    window_->GetBounds());
  Sync();

  EXPECT_CALL(*mock_surface, Attach(_, _, _)).Times(1);
  EXPECT_CALL(*mock_surface, Frame(_)).Times(1);
  EXPECT_CALL(*mock_surface, Commit()).Times(1);

  ProcessCreatedBufferResourcesWithExpectation(1u /* expected size */,
                                               false /* fail */);

  Sync();

  // Once the client receives a "...send_created" call, it must destroy the
  // params resource.
  EXPECT_TRUE(linux_dmabuf->buffer_params().empty());

  // Part 2: the surface mustn't have a buffer attached until frame callback is
  // sent by the server.

  EXPECT_CALL(*linux_dmabuf, CreateParams(_, _, _)).Times(1);
  CreateDmabufBasedBufferAndSetTerminateExpecation(false /*fail*/, widget,
                                                   kDmabufBufferId2);

  ProcessCreatedBufferResourcesWithExpectation(1u /* expected size */,
                                               false /* fail */);

  Sync();

  EXPECT_CALL(*mock_surface, Attach(_, _, _)).Times(0);
  EXPECT_CALL(*mock_surface, Frame(_)).Times(0);
  EXPECT_CALL(*mock_surface, Commit()).Times(0);

  buffer_manager_gpu_->CommitBuffer(widget, kDmabufBufferId2,
                                    window_->GetBounds());

  Sync();

  // After the frame callback is sent, the pending buffer will be committed.
  EXPECT_CALL(*mock_surface, Attach(_, _, _)).Times(1);
  EXPECT_CALL(*mock_surface, Frame(_)).Times(1);
  EXPECT_CALL(*mock_surface, Commit()).Times(1);

  mock_surface->SendFrameCallback();

  Sync();
}

INSTANTIATE_TEST_SUITE_P(XdgVersionV5Test,
                         WaylandBufferManagerTest,
                         ::testing::Values(kXdgShellV5));
INSTANTIATE_TEST_SUITE_P(XdgVersionV6Test,
                         WaylandBufferManagerTest,
                         ::testing::Values(kXdgShellV6));

}  // namespace ui
