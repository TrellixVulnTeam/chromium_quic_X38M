// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SWAP_CHAIN_FACTORY_DXGI_H_
#define GPU_COMMAND_BUFFER_SERVICE_SWAP_CHAIN_FACTORY_DXGI_H_

#include <d3d11.h>
#include <dxgi1_2.h>
#include <windows.h>
#include <wrl/client.h>

#include <memory>

#include "base/macros.h"
#include "components/viz/common/resources/resource_format.h"
#include "gpu/gpu_gles2_export.h"

namespace gfx {
class Size;
class ColorSpace;
}  // namespace gfx

namespace gpu {
class SharedImageBacking;
struct Mailbox;

class GPU_GLES2_EXPORT SwapChainFactoryDXGI {
 public:
  explicit SwapChainFactoryDXGI(bool use_passthrough);
  ~SwapChainFactoryDXGI();

  // Returns true if DXGI swap chain shared images for overlays are supported.
  static bool IsSupported();

  struct GPU_GLES2_EXPORT SwapChainBackings {
    SwapChainBackings(std::unique_ptr<SharedImageBacking> front_buffer,
                      std::unique_ptr<SharedImageBacking> back_buffer);
    ~SwapChainBackings();
    SwapChainBackings(SwapChainBackings&&);
    SwapChainBackings& operator=(SwapChainBackings&&);

    std::unique_ptr<SharedImageBacking> front_buffer;
    std::unique_ptr<SharedImageBacking> back_buffer;

   private:
    DISALLOW_COPY_AND_ASSIGN(SwapChainBackings);
  };

  // Creates IDXGI Swap Chain and exposes front and back buffers as Shared Image
  // mailboxes.
  SwapChainBackings CreateSwapChain(const Mailbox& front_buffer_mailbox,
                                    const Mailbox& back_buffer_mailbox,
                                    viz::ResourceFormat format,
                                    const gfx::Size& size,
                                    const gfx::ColorSpace& color_space,
                                    uint32_t usage);

 private:
  // Wraps the swap chain buffer (front buffer/back buffer) into GLimage and
  // creates a GL texture and stores it as gles2::Texture or as
  // gles2::TexturePassthrough in the backing that is created.
  std::unique_ptr<SharedImageBacking> MakeBacking(
      const Mailbox& mailbox,
      viz::ResourceFormat format,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      uint32_t usage,
      const Microsoft::WRL::ComPtr<IDXGISwapChain1>& swap_chain,
      size_t buffer_index);

  // Whether we're using the passthrough command decoder and should generate
  // passthrough textures.
  const bool use_passthrough_ = false;
  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device_;
  DISALLOW_COPY_AND_ASSIGN(SwapChainFactoryDXGI);
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SWAP_CHAIN_FACTORY_DXGI_H_
