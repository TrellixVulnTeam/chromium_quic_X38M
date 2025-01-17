// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_GENERIC_SENSOR_PLATFORM_SENSOR_READER_WINRT_H_
#define SERVICES_DEVICE_GENERIC_SENSOR_PLATFORM_SENSOR_READER_WINRT_H_

#include <windows.devices.sensors.h>
#include <windows.foundation.h>
#include <wrl/client.h>
#include <wrl/event.h>
#include <functional>
#include <memory>

#include "base/callback.h"
#include "base/optional.h"
#include "base/synchronization/lock.h"
#include "services/device/generic_sensor/platform_sensor_reader_win_base.h"

namespace device {

namespace mojom {
enum class SensorType;
}

// Helper class used to create PlatformSensorReaderWinrt instances
class PlatformSensorReaderWinrtFactory {
 public:
  static std::unique_ptr<PlatformSensorReaderWinBase> Create(
      mojom::SensorType type);
};

enum SensorWinrtCreateFailure {
  kOk = 0,
  kErrorISensorWinrtStaticsActivationFailed = 1,
  kErrorGetDefaultSensorFailed = 2,
  kErrorDefaultSensorNull = 3,
  kErrorGetMinReportIntervalFailed = 4
};

// Base class that contains common helper functions used between all low
// level sensor types based on the Windows.Devices.Sensors API. Derived
// classes will specialize the template into a specific sensor. See
// PlatformSensorReaderWinrtLightSensor as an example of what WinRT
// interfaces should be passed in. The owner of this class must guarantee
// construction and destruction occur on the same thread and that no
// other thread is accessing it during destruction.
template <wchar_t const* runtime_class_id,
          class ISensorWinrtStatics,
          class ISensorWinrtClass,
          class ISensorReadingChangedHandler,
          class ISensorReadingChangedEventArgs>
class PlatformSensorReaderWinrtBase : public PlatformSensorReaderWinBase {
 public:
  using GetSensorFactoryFunctor =
      base::Callback<HRESULT(ISensorWinrtStatics**)>;

  // Sets the client to notify changes about. The consumer should always
  // ensure the lifetime of the client surpasses the lifetime of this class.
  void SetClient(Client* client) override;

  // Allows tests to specify their own implementation of the underlying sensor.
  // This function should be called before Initialize().
  void InitForTests(GetSensorFactoryFunctor get_sensor_factory_callback) {
    get_sensor_factory_callback_ = get_sensor_factory_callback;
  }

  SensorWinrtCreateFailure Initialize() WARN_UNUSED_RESULT;

  bool StartSensor(const PlatformSensorConfiguration& configuration) override
      WARN_UNUSED_RESULT;
  base::TimeDelta GetMinimalReportingInterval() const override;
  void StopSensor() override;

 protected:
  PlatformSensorReaderWinrtBase();
  virtual ~PlatformSensorReaderWinrtBase();

  // Determines if the SensorWinrtCreateFailure code means a WinRT sensor
  // was successfully created or not.
  static bool IsSensorCreateSuccess(
      SensorWinrtCreateFailure create_return_code);

  // Derived classes should implement this function to handle sensor specific
  // parsing of the sensor reading.
  virtual HRESULT OnReadingChangedCallback(
      ISensorWinrtClass* sensor,
      ISensorReadingChangedEventArgs* reading_changed_args) = 0;

  // Helper function which converts the DateTime timestamp format the
  // Windows.Devices.Sensors API uses to the second time ticks the
  // client expects.
  template <class ISensorReading>
  HRESULT ConvertSensorReadingTimeStamp(
      Microsoft::WRL::ComPtr<ISensorReading> sensor_reading,
      base::TimeDelta* timestamp_delta);

  // Following class member is protected by lock since SetClient,
  // StartSensor, and StopSensor can all be called from different
  // threads by PlatformSensorWin.
  base::Lock lock_;
  // Null if there is no client to notify, non-null otherwise.
  Client* client_;

 private:
  base::TimeDelta GetMinimumReportIntervalFromSensor();

  GetSensorFactoryFunctor get_sensor_factory_callback_;

  // base::nullopt if the sensor has not been started, non-empty otherwise.
  base::Optional<EventRegistrationToken> reading_callback_token_;

  base::TimeDelta minimum_report_interval_;
  Microsoft::WRL::ComPtr<ISensorWinrtClass> sensor_;
};

class PlatformSensorReaderWinrtLightSensor final
    : public PlatformSensorReaderWinrtBase<
          RuntimeClass_Windows_Devices_Sensors_LightSensor,
          ABI::Windows::Devices::Sensors::ILightSensorStatics,
          ABI::Windows::Devices::Sensors::ILightSensor,
          Microsoft::WRL::Implements<
              Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
              ABI::Windows::Foundation::ITypedEventHandler<
                  ABI::Windows::Devices::Sensors::LightSensor*,
                  ABI::Windows::Devices::Sensors::
                      LightSensorReadingChangedEventArgs*>,
              Microsoft::WRL::FtmBase>,
          ABI::Windows::Devices::Sensors::ILightSensorReadingChangedEventArgs> {
 public:
  static std::unique_ptr<PlatformSensorReaderWinBase> Create();

  PlatformSensorReaderWinrtLightSensor() = default;
  ~PlatformSensorReaderWinrtLightSensor() override = default;

 protected:
  HRESULT OnReadingChangedCallback(
      ABI::Windows::Devices::Sensors::ILightSensor* sensor,
      ABI::Windows::Devices::Sensors::ILightSensorReadingChangedEventArgs*
          reading_changed_args) override;

 private:
  PlatformSensorReaderWinrtLightSensor(
      const PlatformSensorReaderWinrtLightSensor&) = delete;
  PlatformSensorReaderWinrtLightSensor& operator=(
      const PlatformSensorReaderWinrtLightSensor&) = delete;
};

class PlatformSensorReaderWinrtAccelerometer final
    : public PlatformSensorReaderWinrtBase<
          RuntimeClass_Windows_Devices_Sensors_Accelerometer,
          ABI::Windows::Devices::Sensors::IAccelerometerStatics,
          ABI::Windows::Devices::Sensors::IAccelerometer,
          Microsoft::WRL::Implements<
              Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
              ABI::Windows::Foundation::ITypedEventHandler<
                  ABI::Windows::Devices::Sensors::Accelerometer*,
                  ABI::Windows::Devices::Sensors::
                      AccelerometerReadingChangedEventArgs*>,
              Microsoft::WRL::FtmBase>,
          ABI::Windows::Devices::Sensors::
              IAccelerometerReadingChangedEventArgs> {
 public:
  static std::unique_ptr<PlatformSensorReaderWinBase> Create();

  PlatformSensorReaderWinrtAccelerometer() = default;
  ~PlatformSensorReaderWinrtAccelerometer() override = default;

 protected:
  HRESULT OnReadingChangedCallback(
      ABI::Windows::Devices::Sensors::IAccelerometer* sensor,
      ABI::Windows::Devices::Sensors::IAccelerometerReadingChangedEventArgs*
          reading_changed_args) override;

 private:
  PlatformSensorReaderWinrtAccelerometer(
      const PlatformSensorReaderWinrtAccelerometer&) = delete;
  PlatformSensorReaderWinrtAccelerometer& operator=(
      const PlatformSensorReaderWinrtAccelerometer&) = delete;
};

class PlatformSensorReaderWinrtGyrometer final
    : public PlatformSensorReaderWinrtBase<
          RuntimeClass_Windows_Devices_Sensors_Gyrometer,
          ABI::Windows::Devices::Sensors::IGyrometerStatics,
          ABI::Windows::Devices::Sensors::IGyrometer,
          Microsoft::WRL::Implements<
              Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
              ABI::Windows::Foundation::ITypedEventHandler<
                  ABI::Windows::Devices::Sensors::Gyrometer*,
                  ABI::Windows::Devices::Sensors::
                      GyrometerReadingChangedEventArgs*>,
              Microsoft::WRL::FtmBase>,
          ABI::Windows::Devices::Sensors::IGyrometerReadingChangedEventArgs> {
 public:
  static std::unique_ptr<PlatformSensorReaderWinBase> Create();

  PlatformSensorReaderWinrtGyrometer() = default;
  ~PlatformSensorReaderWinrtGyrometer() override = default;

 protected:
  HRESULT OnReadingChangedCallback(
      ABI::Windows::Devices::Sensors::IGyrometer* sensor,
      ABI::Windows::Devices::Sensors::IGyrometerReadingChangedEventArgs*
          reading_changed_args) override;

 private:
  PlatformSensorReaderWinrtGyrometer(
      const PlatformSensorReaderWinrtGyrometer&) = delete;
  PlatformSensorReaderWinrtGyrometer& operator=(
      const PlatformSensorReaderWinrtGyrometer&) = delete;
};

class PlatformSensorReaderWinrtMagnetometer final
    : public PlatformSensorReaderWinrtBase<
          RuntimeClass_Windows_Devices_Sensors_Magnetometer,
          ABI::Windows::Devices::Sensors::IMagnetometerStatics,
          ABI::Windows::Devices::Sensors::IMagnetometer,
          Microsoft::WRL::Implements<
              Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
              ABI::Windows::Foundation::ITypedEventHandler<
                  ABI::Windows::Devices::Sensors::Magnetometer*,
                  ABI::Windows::Devices::Sensors::
                      MagnetometerReadingChangedEventArgs*>,
              Microsoft::WRL::FtmBase>,
          ABI::Windows::Devices::Sensors::
              IMagnetometerReadingChangedEventArgs> {
 public:
  static std::unique_ptr<PlatformSensorReaderWinBase> Create();

  PlatformSensorReaderWinrtMagnetometer() = default;
  ~PlatformSensorReaderWinrtMagnetometer() override = default;

 protected:
  HRESULT OnReadingChangedCallback(
      ABI::Windows::Devices::Sensors::IMagnetometer* sensor,
      ABI::Windows::Devices::Sensors::IMagnetometerReadingChangedEventArgs*
          reading_changed_args) override;

 private:
  PlatformSensorReaderWinrtMagnetometer(
      const PlatformSensorReaderWinrtMagnetometer&) = delete;
  PlatformSensorReaderWinrtMagnetometer& operator=(
      const PlatformSensorReaderWinrtMagnetometer&) = delete;
};

class PlatformSensorReaderWinrtAbsOrientationEulerAngles final
    : public PlatformSensorReaderWinrtBase<
          RuntimeClass_Windows_Devices_Sensors_Inclinometer,
          ABI::Windows::Devices::Sensors::IInclinometerStatics,
          ABI::Windows::Devices::Sensors::IInclinometer,
          Microsoft::WRL::Implements<
              Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
              ABI::Windows::Foundation::ITypedEventHandler<
                  ABI::Windows::Devices::Sensors::Inclinometer*,
                  ABI::Windows::Devices::Sensors::
                      InclinometerReadingChangedEventArgs*>,
              Microsoft::WRL::FtmBase>,
          ABI::Windows::Devices::Sensors::
              IInclinometerReadingChangedEventArgs> {
 public:
  static std::unique_ptr<PlatformSensorReaderWinBase> Create();

  PlatformSensorReaderWinrtAbsOrientationEulerAngles() = default;
  ~PlatformSensorReaderWinrtAbsOrientationEulerAngles() override = default;

 protected:
  HRESULT OnReadingChangedCallback(
      ABI::Windows::Devices::Sensors::IInclinometer* sensor,
      ABI::Windows::Devices::Sensors::IInclinometerReadingChangedEventArgs*
          reading_changed_args) override;

 private:
  PlatformSensorReaderWinrtAbsOrientationEulerAngles(
      const PlatformSensorReaderWinrtAbsOrientationEulerAngles&) = delete;
  PlatformSensorReaderWinrtAbsOrientationEulerAngles& operator=(
      const PlatformSensorReaderWinrtAbsOrientationEulerAngles&) = delete;
};

class PlatformSensorReaderWinrtAbsOrientationQuaternion final
    : public PlatformSensorReaderWinrtBase<
          RuntimeClass_Windows_Devices_Sensors_OrientationSensor,
          ABI::Windows::Devices::Sensors::IOrientationSensorStatics,
          ABI::Windows::Devices::Sensors::IOrientationSensor,
          Microsoft::WRL::Implements<
              Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
              ABI::Windows::Foundation::ITypedEventHandler<
                  ABI::Windows::Devices::Sensors::OrientationSensor*,
                  ABI::Windows::Devices::Sensors::
                      OrientationSensorReadingChangedEventArgs*>,
              Microsoft::WRL::FtmBase>,
          ABI::Windows::Devices::Sensors::
              IOrientationSensorReadingChangedEventArgs> {
 public:
  static std::unique_ptr<PlatformSensorReaderWinBase> Create();

  PlatformSensorReaderWinrtAbsOrientationQuaternion() = default;
  ~PlatformSensorReaderWinrtAbsOrientationQuaternion() override = default;

 protected:
  HRESULT OnReadingChangedCallback(
      ABI::Windows::Devices::Sensors::IOrientationSensor* sensor,
      ABI::Windows::Devices::Sensors::IOrientationSensorReadingChangedEventArgs*
          reading_changed_args) override;

 private:
  PlatformSensorReaderWinrtAbsOrientationQuaternion(
      const PlatformSensorReaderWinrtAbsOrientationQuaternion&) = delete;
  PlatformSensorReaderWinrtAbsOrientationQuaternion& operator=(
      const PlatformSensorReaderWinrtAbsOrientationQuaternion&) = delete;
};

}  // namespace device

#endif  // SERVICES_DEVICE_GENERIC_SENSOR_PLATFORM_SENSOR_READER_WINRT_H_