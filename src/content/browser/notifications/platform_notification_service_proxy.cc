// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/notifications/platform_notification_service_proxy.h"

#include <memory>
#include <utility>

#include "base/logging.h"
#include "base/task/post_task.h"
#include "content/browser/notifications/devtools_event_logging.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/notification_database_data.h"
#include "content/public/browser/platform_notification_service.h"

namespace content {

PlatformNotificationServiceProxy::PlatformNotificationServiceProxy(
    scoped_refptr<ServiceWorkerContextWrapper> service_worker_context,
    BrowserContext* browser_context)
    : service_worker_context_(service_worker_context),
      browser_context_(browser_context),
      notification_service_(
          GetContentClient()->browser()->GetPlatformNotificationService(
              browser_context)) {}

PlatformNotificationServiceProxy::~PlatformNotificationServiceProxy() = default;

void PlatformNotificationServiceProxy::Shutdown() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  weak_ptr_factory_ui_.InvalidateWeakPtrs();
}

base::WeakPtr<PlatformNotificationServiceProxy>
PlatformNotificationServiceProxy::AsWeakPtr() {
  return weak_ptr_factory_ui_.GetWeakPtr();
}

void PlatformNotificationServiceProxy::DoDisplayNotification(
    const NotificationDatabaseData& data,
    const GURL& service_worker_scope,
    DisplayResultCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (notification_service_) {
    notification_service_->DisplayPersistentNotification(
        data.notification_id, service_worker_scope, data.origin,
        data.notification_data,
        data.notification_resources.value_or(blink::NotificationResources()));
    notifications::LogNotificationDisplayedEventToDevTools(browser_context_,
                                                           data);
  }
  std::move(callback).Run(/* success= */ true, data.notification_id);
}

void PlatformNotificationServiceProxy::VerifyServiceWorkerScope(
    const NotificationDatabaseData& data,
    DisplayResultCallback callback,
    blink::ServiceWorkerStatusCode status,
    scoped_refptr<ServiceWorkerRegistration> registration) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (status == blink::ServiceWorkerStatusCode::kOk &&
      registration->scope().GetOrigin() == data.origin) {
    base::PostTask(
        FROM_HERE, {BrowserThread::UI, base::TaskPriority::USER_VISIBLE},
        base::BindOnce(&PlatformNotificationServiceProxy::DoDisplayNotification,
                       AsWeakPtr(), data, registration->scope(),
                       std::move(callback)));
  } else {
    base::PostTask(FROM_HERE,
                   {BrowserThread::UI, base::TaskPriority::USER_VISIBLE},
                   base::BindOnce(std::move(callback), /* success= */ false,
                                  /* notification_id= */ ""));
  }
}

void PlatformNotificationServiceProxy::DisplayNotification(
    const NotificationDatabaseData& data,
    DisplayResultCallback callback) {
  if (!service_worker_context_) {
    base::PostTask(
        FROM_HERE, {BrowserThread::UI, base::TaskPriority::USER_VISIBLE},
        base::BindOnce(&PlatformNotificationServiceProxy::DoDisplayNotification,
                       AsWeakPtr(), data, GURL(), std::move(callback)));
    return;
  }

  base::PostTask(
      FROM_HERE, {BrowserThread::IO, base::TaskPriority::USER_VISIBLE},
      base::BindOnce(
          &ServiceWorkerContextWrapper::FindReadyRegistrationForId,
          service_worker_context_, data.service_worker_registration_id,
          data.origin,
          base::BindOnce(
              &PlatformNotificationServiceProxy::VerifyServiceWorkerScope,
              weak_ptr_factory_io_.GetWeakPtr(), data, std::move(callback))));
}

void PlatformNotificationServiceProxy::CloseNotification(
    const std::string& notification_id) {
  if (!notification_service_)
    return;
  base::PostTask(
      FROM_HERE, {BrowserThread::UI, base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&PlatformNotificationServiceProxy::DoCloseNotification,
                     AsWeakPtr(), notification_id));
}

void PlatformNotificationServiceProxy::DoCloseNotification(
    const std::string& notification_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  notification_service_->ClosePersistentNotification(notification_id);
}

void PlatformNotificationServiceProxy::ScheduleTrigger(base::Time timestamp) {
  if (!notification_service_)
    return;
  base::PostTask(
      FROM_HERE, {BrowserThread::UI, base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&PlatformNotificationServiceProxy::DoScheduleTrigger,
                     AsWeakPtr(), timestamp));
}

void PlatformNotificationServiceProxy::DoScheduleTrigger(base::Time timestamp) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  notification_service_->ScheduleTrigger(timestamp);
}

void PlatformNotificationServiceProxy::ScheduleNotification(
    const NotificationDatabaseData& data) {
  DCHECK(data.notification_data.show_trigger_timestamp.has_value());
  if (!notification_service_)
    return;
  base::PostTask(
      FROM_HERE, {BrowserThread::UI, base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&PlatformNotificationServiceProxy::DoScheduleNotification,
                     AsWeakPtr(), data));
}

void PlatformNotificationServiceProxy::DoScheduleNotification(
    const NotificationDatabaseData& data) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::Time show_trigger_timestamp =
      data.notification_data.show_trigger_timestamp.value();
  notifications::LogNotificationScheduledEventToDevTools(
      browser_context_, data, show_trigger_timestamp);
  notification_service_->ScheduleTrigger(show_trigger_timestamp);
}

base::Time PlatformNotificationServiceProxy::GetNextTrigger() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!notification_service_)
    return base::Time::Max();
  return notification_service_->ReadNextTriggerTimestamp();
}

void PlatformNotificationServiceProxy::RecordNotificationUkmEvent(
    const NotificationDatabaseData& data) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!notification_service_)
    return;
  notification_service_->RecordNotificationUkmEvent(data);
}

bool PlatformNotificationServiceProxy::ShouldLogClose(const GURL& origin) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return notifications::ShouldLogNotificationEventToDevTools(browser_context_,
                                                             origin);
}

void PlatformNotificationServiceProxy::LogClose(
    const NotificationDatabaseData& data) {
  base::PostTask(FROM_HERE,
                 {BrowserThread::UI, base::TaskPriority::BEST_EFFORT},
                 base::BindOnce(&PlatformNotificationServiceProxy::DoLogClose,
                                AsWeakPtr(), data));
}

void PlatformNotificationServiceProxy::DoLogClose(
    const NotificationDatabaseData& data) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  notifications::LogNotificationClosedEventToDevTools(browser_context_, data);
}

}  // namespace content
