/*
 * Copyright (C) 2012, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#include "third_party/blink/renderer/modules/filesystem/dom_window_file_system.h"

#include "third_party/blink/public/mojom/filesystem/file_system.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/fileapi/file_error.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/modules/filesystem/async_callback_helper.h"
#include "third_party/blink/renderer/modules/filesystem/directory_entry.h"
#include "third_party/blink/renderer/modules/filesystem/dom_file_system.h"
#include "third_party/blink/renderer/modules/filesystem/file_system_callbacks.h"
#include "third_party/blink/renderer/modules/filesystem/file_system_dispatcher.h"
#include "third_party/blink/renderer/modules/filesystem/local_file_system.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/weborigin/scheme_registry.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {

void DOMWindowFileSystem::webkitRequestFileSystem(
    LocalDOMWindow& window,
    int type,
    int64_t size,
    V8FileSystemCallback* success_callback,
    V8ErrorCallback* error_callback) {
  if (!window.IsCurrentlyDisplayedInFrame())
    return;

  Document* document = window.document();
  if (!document)
    return;

  auto error_callback_wrapper =
      AsyncCallbackHelper::ErrorCallback(error_callback);

  if (SchemeRegistry::SchemeShouldBypassContentSecurityPolicy(
          document->GetSecurityOrigin()->Protocol()))
    UseCounter::Count(document, WebFeature::kRequestFileSystemNonWebbyOrigin);

  if (!document->GetSecurityOrigin()->CanAccessFileSystem()) {
    DOMFileSystem::ReportError(document, std::move(error_callback_wrapper),
                               base::File::FILE_ERROR_SECURITY);
    return;
  } else if (document->GetSecurityOrigin()->IsLocal()) {
    UseCounter::Count(document, WebFeature::kFileAccessedFileSystem);
  }

  mojom::blink::FileSystemType file_system_type =
      static_cast<mojom::blink::FileSystemType>(type);
  if (!DOMFileSystemBase::IsValidType(file_system_type)) {
    DOMFileSystem::ReportError(document, std::move(error_callback_wrapper),
                               base::File::FILE_ERROR_INVALID_OPERATION);
    return;
  }

  if (file_system_type == mojom::blink::FileSystemType::kTemporary) {
    UseCounter::Count(document, WebFeature::kRequestedFileSystemTemporary);
  } else if (file_system_type == mojom::blink::FileSystemType::kPersistent) {
    UseCounter::Count(document, WebFeature::kRequestedFileSystemPersistent);
  }

  auto success_callback_wrapper =
      AsyncCallbackHelper::SuccessCallback<DOMFileSystem>(success_callback);

  LocalFileSystem::From(*document)->RequestFileSystem(
      document, file_system_type, size,
      std::make_unique<FileSystemCallbacks>(std::move(success_callback_wrapper),
                                            std::move(error_callback_wrapper),
                                            document, file_system_type),
      LocalFileSystem::kAsynchronous);
}

void DOMWindowFileSystem::webkitResolveLocalFileSystemURL(
    LocalDOMWindow& window,
    const String& url,
    V8EntryCallback* success_callback,
    V8ErrorCallback* error_callback) {
  if (!window.IsCurrentlyDisplayedInFrame())
    return;

  Document* document = window.document();
  if (!document)
    return;

  auto error_callback_wrapper =
      AsyncCallbackHelper::ErrorCallback(error_callback);

  const SecurityOrigin* security_origin = document->GetSecurityOrigin();
  KURL completed_url = document->CompleteURL(url);
  if (!security_origin->CanAccessFileSystem() ||
      !security_origin->CanRequest(completed_url)) {
    DOMFileSystem::ReportError(document, std::move(error_callback_wrapper),
                               base::File::FILE_ERROR_SECURITY);
    return;
  } else if (document->GetSecurityOrigin()->IsLocal()) {
    UseCounter::Count(document, WebFeature::kFileAccessedFileSystem);
  }

  if (!completed_url.IsValid()) {
    DOMFileSystem::ReportError(document, std::move(error_callback_wrapper),
                               base::File::FILE_ERROR_INVALID_URL);
    return;
  }

  auto success_callback_wrapper =
      AsyncCallbackHelper::SuccessCallback<Entry>(success_callback);

  LocalFileSystem::From(*document)->ResolveURL(
      document, completed_url,
      std::make_unique<ResolveURICallbacks>(std::move(success_callback_wrapper),
                                            std::move(error_callback_wrapper),
                                            document),
      LocalFileSystem::kAsynchronous);
}

static_assert(
    static_cast<int>(DOMWindowFileSystem::kTemporary) ==
        static_cast<int>(mojom::blink::FileSystemType::kTemporary),
    "DOMWindowFileSystem::kTemporary should match FileSystemTypeTemporary");
static_assert(
    static_cast<int>(DOMWindowFileSystem::kPersistent) ==
        static_cast<int>(mojom::blink::FileSystemType::kPersistent),
    "DOMWindowFileSystem::kPersistent should match FileSystemTypePersistent");

}  // namespace blink
