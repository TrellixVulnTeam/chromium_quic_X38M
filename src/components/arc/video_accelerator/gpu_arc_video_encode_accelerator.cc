// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/video_accelerator/gpu_arc_video_encode_accelerator.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/platform_shared_memory_region.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/numerics/checked_math.h"
#include "base/numerics/safe_conversions.h"
#include "base/system/sys_info.h"
#include "components/arc/video_accelerator/arc_video_accelerator_util.h"
#include "media/base/video_types.h"
#include "media/gpu/gpu_video_encode_accelerator_factory.h"
#include "media/gpu/macros.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "mojo/public/cpp/bindings/type_converter.h"
#include "mojo/public/cpp/system/platform_handle.h"

namespace arc {

GpuArcVideoEncodeAccelerator::GpuArcVideoEncodeAccelerator(
    const gpu::GpuPreferences& gpu_preferences)
    : gpu_preferences_(gpu_preferences),
      input_storage_type_(
          media::VideoEncodeAccelerator::Config::StorageType::kShmem),
      bitstream_buffer_serial_(0) {}

GpuArcVideoEncodeAccelerator::~GpuArcVideoEncodeAccelerator() = default;

// VideoEncodeAccelerator::Client implementation.
void GpuArcVideoEncodeAccelerator::RequireBitstreamBuffers(
    unsigned int input_count,
    const gfx::Size& coded_size,
    size_t output_buffer_size) {
  DVLOGF(2) << "input_count=" << input_count
            << ", coded_size=" << coded_size.ToString()
            << ", output_buffer_size=" << output_buffer_size;
  DCHECK(client_);
  coded_size_ = coded_size;
  client_->RequireBitstreamBuffers(input_count, coded_size, output_buffer_size);
}

void GpuArcVideoEncodeAccelerator::BitstreamBufferReady(
    int32_t bitstream_buffer_id,
    const media::BitstreamBufferMetadata& metadata) {
  DVLOGF(2) << "id=" << bitstream_buffer_id;
  DCHECK(client_);
  auto iter = use_bitstream_cbs_.find(bitstream_buffer_id);
  DCHECK(iter != use_bitstream_cbs_.end());
  std::move(iter->second)
      .Run(metadata.payload_size_bytes, metadata.key_frame,
           metadata.timestamp.InMicroseconds());
  use_bitstream_cbs_.erase(iter);
}

void GpuArcVideoEncodeAccelerator::NotifyError(Error error) {
  DVLOGF(2) << "error=" << error;
  DCHECK(client_);
  client_->NotifyError(error);
}

// ::arc::mojom::VideoEncodeAccelerator implementation.
void GpuArcVideoEncodeAccelerator::GetSupportedProfiles(
    GetSupportedProfilesCallback callback) {
  std::move(callback).Run(
      media::GpuVideoEncodeAcceleratorFactory::GetSupportedProfiles(
          gpu_preferences_));
}

void GpuArcVideoEncodeAccelerator::Initialize(
    const media::VideoEncodeAccelerator::Config& config,
    VideoEncodeClientPtr client,
    InitializeCallback callback) {
  DVLOGF(2) << config.AsHumanReadableString();
  if (!config.storage_type.has_value()) {
    DLOG(ERROR) << "storage type must be specified";
    std::move(callback).Run(false);
    return;
  }
  input_pixel_format_ = config.input_format;
  input_storage_type_ = *config.storage_type;
  visible_size_ = config.input_visible_size;
  accelerator_ = media::GpuVideoEncodeAcceleratorFactory::CreateVEA(
      config, this, gpu_preferences_);
  if (accelerator_ == nullptr) {
    DLOG(ERROR) << "Failed to create a VideoEncodeAccelerator.";
    std::move(callback).Run(false);
    return;
  }
  client_ = std::move(client);
  std::move(callback).Run(true);
}

void GpuArcVideoEncodeAccelerator::Encode(
    media::VideoPixelFormat format,
    mojo::ScopedHandle handle,
    std::vector<::arc::VideoFramePlane> planes,
    int64_t timestamp,
    bool force_keyframe,
    EncodeCallback callback) {
  DVLOGF(2) << "timestamp=" << timestamp;
  if (!accelerator_) {
    DLOG(ERROR) << "Accelerator is not initialized.";
    return;
  }

  if (planes.empty()) {  // EOS
    accelerator_->Encode(media::VideoFrame::CreateEOSFrame(), force_keyframe);
    return;
  }

  base::ScopedFD fd = UnwrapFdFromMojoHandle(std::move(handle));
  if (!fd.is_valid()) {
    client_->NotifyError(Error::kPlatformFailureError);
    return;
  }

  if (input_storage_type_ ==
      media::VideoEncodeAccelerator::Config::StorageType::kShmem) {
    EncodeSharedMemory(std::move(fd), format, planes, timestamp, force_keyframe,
                       std::move(callback));
  } else {
    EncodeDmabuf(std::move(fd), format, planes, timestamp, force_keyframe,
                 std::move(callback));
  }
}

void GpuArcVideoEncodeAccelerator::EncodeDmabuf(
    base::ScopedFD fd,
    media::VideoPixelFormat format,
    const std::vector<::arc::VideoFramePlane>& planes,
    int64_t timestamp,
    bool force_keyframe,
    EncodeCallback callback) {
  client_->NotifyError(Error::kInvalidArgumentError);
  NOTIMPLEMENTED();
}

void GpuArcVideoEncodeAccelerator::EncodeSharedMemory(
    base::ScopedFD fd,
    media::VideoPixelFormat format,
    const std::vector<::arc::VideoFramePlane>& planes,
    int64_t timestamp,
    bool force_keyframe,
    EncodeCallback callback) {
  if (format != media::PIXEL_FORMAT_I420) {
    DLOG(ERROR) << "Formats other than I420 are unsupported. format=" << format;
    client_->NotifyError(Error::kInvalidArgumentError);
    return;
  }

  base::Optional<media::VideoFrameLayout> layout;
  auto gmb_handle = CreateGpuMemoryBufferHandle(
      format, coded_size_, base::ScopedFD(HANDLE_EINTR(dup(fd.get()))), planes);
  if (!gmb_handle) {
    DLOG(ERROR) << "Failed to create GpuMemoryBufferHandle";
    client_->NotifyError(Error::kInvalidArgumentError);
    return;
  }

  const size_t num_planes = gmb_handle->native_pixmap_handle.planes.size();
  // This is guaranteed because format is I420 here.
  DCHECK_EQ(num_planes, 3u);
  std::vector<media::VideoFrameLayout::Plane> layout_planes(num_planes);
  for (size_t i = 0; i < num_planes; i++) {
    const auto& plane = gmb_handle->native_pixmap_handle.planes[i];
    if (!base::IsValueInRangeForNumericType<int32_t>(plane.stride)) {
      DLOG(ERROR) << "Invalid stride";
      client_->NotifyError(Error::kInvalidArgumentError);
      return;
    }
    if (!base::IsValueInRangeForNumericType<size_t>(plane.offset)) {
      DLOG(ERROR) << "Invalid offset";
      client_->NotifyError(Error::kInvalidArgumentError);
      return;
    }
    if (!base::IsValueInRangeForNumericType<size_t>(plane.size)) {
      DLOG(ERROR) << "Invalid size";
      client_->NotifyError(Error::kInvalidArgumentError);
      return;
    }

    // convert uint32_t -> int32_t.
    layout_planes[i].stride = base::checked_cast<int32_t>(plane.stride);
    // convert uint64_t -> size_t
    layout_planes[i].offset = base::checked_cast<size_t>(plane.offset);
    // convert uint64_t -> size_t
    layout_planes[i].size = base::checked_cast<size_t>(plane.size);
  }

  layout = media::VideoFrameLayout::CreateWithPlanes(
      format, gfx::Size(layout_planes[0].stride, coded_size_.height()),
      std::move(layout_planes));
  if (!layout) {
    DLOG(ERROR) << "Failed to create VideoFrameLayout.";
    client_->NotifyError(Error::kInvalidArgumentError);
    return;
  }

  base::CheckedNumeric<size_t> map_size = 0;
  for (const auto& plane : layout->planes()) {
    map_size = map_size.Max(plane.offset + plane.size);
  }
  if (!map_size.IsValid()) {
    DLOG(ERROR) << "Invalid map_size";
    client_->NotifyError(Error::kInvalidArgumentError);
    return;
  }

  // TODO(rockot): Pass GUIDs through Mojo. https://crbug.com/713763.
  // TODO(rockot): This fd comes from a mojo::ScopedHandle in
  // GpuArcVideoService::BindSharedMemory. That should be passed through,
  // rather than pulling out the fd. https://crbug.com/713763.
  base::UnguessableToken guid = base::UnguessableToken::Create();
  base::subtle::PlatformSharedMemoryRegion platform_region =
      base::subtle::PlatformSharedMemoryRegion::Take(
          std::move(fd),
          base::subtle::PlatformSharedMemoryRegion::Mode::kUnsafe,
          map_size.ValueOrDie(), guid);
  base::UnsafeSharedMemoryRegion shared_region =
      base::UnsafeSharedMemoryRegion::Deserialize(std::move(platform_region));
  base::WritableSharedMemoryMapping mapping =
      shared_region.MapAt(0u, map_size.ValueOrDie());
  if (!mapping.IsValid()) {
    DLOG(ERROR) << "Failed to map memory.";
    client_->NotifyError(Error::kPlatformFailureError);
    return;
  }

  uint8_t* shm_memory = mapping.GetMemoryAsSpan<uint8_t>().data();
  auto frame = media::VideoFrame::WrapExternalYuvDataWithLayout(
      *layout, gfx::Rect(visible_size_), visible_size_,
      shm_memory + layout->planes()[0].offset,
      shm_memory + layout->planes()[1].offset,
      shm_memory + layout->planes()[2].offset,
      base::TimeDelta::FromMicroseconds(timestamp));
  if (!frame) {
    DLOG(ERROR) << "Failed to create VideoFrame";
    client_->NotifyError(Error::kInvalidArgumentError);
    return;
  }
  frame->BackWithOwnedSharedMemory(std::move(shared_region),
                                   std::move(mapping));
  // Add the function to |callback| to |frame|'s  destruction observer. When the
  // |frame| goes out of scope, it executes |callback|.
  frame->AddDestructionObserver(std::move(callback));
  accelerator_->Encode(frame, force_keyframe);
}

void GpuArcVideoEncodeAccelerator::UseBitstreamBuffer(
    mojo::ScopedHandle shmem_fd,
    uint32_t offset,
    uint32_t size,
    UseBitstreamBufferCallback callback) {
  DVLOGF(2) << "serial=" << bitstream_buffer_serial_;
  if (!accelerator_) {
    DLOG(ERROR) << "Accelerator is not initialized.";
    return;
  }

  base::ScopedFD fd = UnwrapFdFromMojoHandle(std::move(shmem_fd));
  if (!fd.is_valid()) {
    client_->NotifyError(Error::kPlatformFailureError);
    return;
  }

  size_t shmem_size;
  if (!GetFileSize(fd.get(), &shmem_size)) {
    client_->NotifyError(Error::kInvalidArgumentError);
    return;
  }

  // TODO(rockot): Pass GUIDs through Mojo. https://crbug.com/713763.
  // TODO(rockot): This fd comes from a mojo::ScopedHandle in
  // GpuArcVideoService::BindSharedMemory. That should be passed through,
  // rather than pulling out the fd. https://crbug.com/713763.
  // TODO(rockot): Pass through a real size rather than |0|.
  base::UnguessableToken guid = base::UnguessableToken::Create();
  auto shm_region = base::subtle::PlatformSharedMemoryRegion::Take(
      std::move(fd), base::subtle::PlatformSharedMemoryRegion::Mode::kUnsafe,
      shmem_size, guid);
  if (!shm_region.IsValid()) {
    client_->NotifyError(Error::kInvalidArgumentError);
    return;
  }
  use_bitstream_cbs_.emplace(bitstream_buffer_serial_, std::move(callback));
  accelerator_->UseOutputBitstreamBuffer(media::BitstreamBuffer(
      bitstream_buffer_serial_, std::move(shm_region), size, offset));

  // Mask against 30 bits to avoid (undefined) wraparound on signed integer.
  bitstream_buffer_serial_ = (bitstream_buffer_serial_ + 1) & 0x3FFFFFFF;
}

void GpuArcVideoEncodeAccelerator::RequestEncodingParametersChange(
    uint32_t bitrate,
    uint32_t framerate) {
  DVLOGF(2) << "bitrate=" << bitrate << ", framerate=" << framerate;
  if (!accelerator_) {
    DLOG(ERROR) << "Accelerator is not initialized.";
    return;
  }
  accelerator_->RequestEncodingParametersChange(bitrate, framerate);
}

void GpuArcVideoEncodeAccelerator::Flush(FlushCallback callback) {
  DVLOGF(2);
  if (!accelerator_) {
    DLOG(ERROR) << "Accelerator is not initialized.";
    return;
  }
  accelerator_->Flush(std::move(callback));
}

}  // namespace arc
