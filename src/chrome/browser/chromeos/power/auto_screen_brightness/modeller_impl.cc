// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/power/auto_screen_brightness/modeller_impl.h"

#include <cmath>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/important_file_writer.h"
#include "base/logging.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/task/post_task.h"
#include "base/task_runner_util.h"
#include "base/time/default_tick_clock.h"
#include "base/time/time.h"
#include "chromeos/constants/chromeos_features.h"
#include "content/public/browser/browser_thread.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"

namespace chromeos {
namespace power {
namespace auto_screen_brightness {

namespace {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class ModelLoadingStatus {
  // Global curve, personal curve and model iteration count are all loaded
  // successfully.
  kSuccess = 0,
  // Global curve data is missing.
  kMissingGlobal = 1,
  // Global curve data exists but cannot be used to create a curve.
  kIllFormattedGlobal = 2,
  // Personal curve data is missing.
  kMissingPersonal = 3,
  // Personal curve data exists but cannot be used to create a curve.
  kIllFormattedPersonal = 4,
  // Model iteration count is missing or is invalid.
  kMissingIterationCount = 5,
  kMaxValue = kMissingIterationCount
};

void LogModelLoadingStatus(ModelLoadingStatus status) {
  UMA_HISTOGRAM_ENUMERATION("AutoScreenBrightness.ModelLoadingStatus", status);
}

// Loads saved model from locations specified by |spec|. This
// should run in another thread to be non-blocking to the main thread (if
// |is_testing| is false). The ambient values read from disk should be in the
// log-domain already.
Model LoadModelFromDisk(const ModellerImpl::ModelSavingSpec& spec,
                        bool is_testing) {
  DCHECK(is_testing ||
         !content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  Model loaded_model;
  std::string content;

  // If global curve doesn't exist or can't be parsed, then we ignore all saved
  // data.
  if (!PathExists(spec.global_curve) ||
      !base::ReadFileToString(spec.global_curve, &content)) {
    LogModelLoadingStatus(ModelLoadingStatus::kMissingGlobal);
    return loaded_model;
  }
  loaded_model.global_curve = MonotoneCubicSpline::FromString(content);
  if (!loaded_model.global_curve) {
    LogModelLoadingStatus(ModelLoadingStatus::kIllFormattedGlobal);
    return loaded_model;
  }

  // If personal curve doesn't exist or can't be parsed, then we ignore any
  // saved personal model. The iteration count is implicitly set to 0.
  if (!PathExists(spec.personal_curve) ||
      !base::ReadFileToString(spec.personal_curve, &content)) {
    LogModelLoadingStatus(ModelLoadingStatus::kMissingPersonal);
    return loaded_model;
  }
  loaded_model.personal_curve = MonotoneCubicSpline::FromString(content);
  if (!loaded_model.personal_curve) {
    LogModelLoadingStatus(ModelLoadingStatus::kIllFormattedPersonal);
    return loaded_model;
  }

  int iteration_count = 0;
  // If iteration count doesn't exist or can't be parsed, it's reset to 0.
  if (!PathExists(spec.iteration_count) ||
      !base::ReadFileToString(spec.iteration_count, &content) ||
      content.empty() || !base::StringToInt(content, &iteration_count)) {
    LogModelLoadingStatus(ModelLoadingStatus::kMissingIterationCount);
    return loaded_model;
  }
  loaded_model.iteration_count = iteration_count;

  LogModelLoadingStatus(ModelLoadingStatus::kSuccess);
  return loaded_model;
}

// Saves |data| to |path|. Returns whether successful and logs error if an
// error occurs.
bool SaveDataAndLogError(const base::FilePath& path, const std::string& data) {
  const int bytes_written = base::WriteFile(path, data.data(), data.size());
  if (bytes_written != static_cast<int>(data.size())) {
    LOG(ERROR) << "Wrote " << bytes_written << " byte(s) instead of "
               << data.size() << " to " << path.value();
    return false;
  }
  return true;
}

// Trains a new curve using training |data| and returns the new curve. This
// should only be called after trainer has been initialized with a global curve
// and a latest curve.
// This should run in another thread to be non-blocking to the main
// thread (if |is_testing| is false).
MonotoneCubicSpline TrainModel(Trainer* trainer,
                               const std::vector<TrainingDataPoint>& data,
                               bool is_testing) {
  DCHECK(is_testing ||
         !content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  return trainer->Train(data);
}

// Sets initial global and personal curve.
// This should run in another thread to be non-blocking to the main
// thread (if |is_testing| is false).
bool SetInitialCurves(Trainer* trainer,
                      const MonotoneCubicSpline& global_curve,
                      const MonotoneCubicSpline& current_curve,
                      bool is_testing) {
  DCHECK(is_testing ||
         !content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  return trainer->SetInitialCurves(global_curve, current_curve);
}

}  // namespace

constexpr char ModellerImpl::kModelDir[];
constexpr char ModellerImpl::kGlobalCurveFileName[];
constexpr char ModellerImpl::kPersonalCurveFileName[];
constexpr char ModellerImpl::kModelIterationCountFileName[];

Model::Model() = default;
Model::Model(const base::Optional<MonotoneCubicSpline>& global_curve,
             const base::Optional<MonotoneCubicSpline>& personal_curve,
             int iteration_count)
    : global_curve(global_curve),
      personal_curve(personal_curve),
      iteration_count(iteration_count) {}

Model::Model(const Model& model) = default;
Model::~Model() = default;

bool SaveModelToDisk(const ModellerImpl::ModelSavingSpec& model_saving_spec,
                     const Model& model,
                     bool save_global_curve,
                     bool save_personal_curve,
                     bool is_testing) {
  DCHECK(is_testing ||
         !content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  if (save_global_curve) {
    DCHECK(model.global_curve);
    const std::string data = model.global_curve->ToString();
    DCHECK(!data.empty());
    if (!SaveDataAndLogError(model_saving_spec.global_curve, data))
      return false;
  }

  if (save_personal_curve) {
    DCHECK(model.personal_curve);
    const std::string data = model.personal_curve->ToString();
    DCHECK(!data.empty());
    if (!SaveDataAndLogError(model_saving_spec.personal_curve, data))
      return false;
  }

  const std::string data = base::NumberToString(model.iteration_count);
  DCHECK(!data.empty());
  return SaveDataAndLogError(model_saving_spec.iteration_count, data);
}

ModellerImpl::ModellerImpl(const Profile* profile,
                           AlsReader* als_reader,
                           BrightnessMonitor* brightness_monitor,
                           ModelConfigLoader* model_config_loader,
                           ui::UserActivityDetector* user_activity_detector,
                           std::unique_ptr<Trainer> trainer)
    : ModellerImpl(profile,
                   als_reader,
                   brightness_monitor,
                   model_config_loader,
                   user_activity_detector,
                   std::move(trainer),
                   base::CreateSequencedTaskRunner(
                       {base::ThreadPool(), base::TaskPriority::BEST_EFFORT,
                        base::MayBlock(),
                        base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN}),
                   base::DefaultTickClock::GetInstance()) {}

ModellerImpl::~ModellerImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void ModellerImpl::AddObserver(Modeller::Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(observer);
  observers_.AddObserver(observer);
  if (is_modeller_enabled_.has_value()) {
    NotifyObserverInitStatus(*observer);
  }
}

void ModellerImpl::RemoveObserver(Modeller::Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(observer);
  observers_.RemoveObserver(observer);
}

void ModellerImpl::OnAmbientLightUpdated(int lux) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!is_modeller_enabled_.has_value() || !*is_modeller_enabled_)
    return;

  DCHECK(log_als_values_);
  log_als_values_->SaveToBuffer({ConvertToLog(lux), tick_clock_->NowTicks()});
}

void ModellerImpl::OnAlsReaderInitialized(AlsReader::AlsInitStatus status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!als_init_status_);

  als_init_status_ = status;

  HandleStatusUpdate();
}

void ModellerImpl::OnBrightnessMonitorInitialized(bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!brightness_monitor_success_.has_value());

  brightness_monitor_success_ = success;
  HandleStatusUpdate();
}

void ModellerImpl::OnUserBrightnessChanged(double old_brightness_percent,
                                           double new_brightness_percent) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!is_modeller_enabled_.has_value() || !*is_modeller_enabled_)
    return;

  DCHECK(log_als_values_);
  const base::TimeTicks now = tick_clock_->NowTicks();
  // We don't add any training data if there is no ambient light sample.
  const base::Optional<AlsAvgStdDev> log_als_avg_stddev =
      log_als_values_->AverageAmbientWithStdDev(now);
  if (!log_als_avg_stddev)
    return;

  data_cache_.push_back({old_brightness_percent, new_brightness_percent,
                         log_als_avg_stddev->avg, now});

  ScheduleTrainerStart();
}

void ModellerImpl::OnUserBrightnessChangeRequested() {}

void ModellerImpl::OnModelConfigLoaded(
    base::Optional<ModelConfig> model_config) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!model_config_exists_.has_value());

  model_config_exists_ = model_config.has_value();
  if (model_config_exists_.value()) {
    model_config_ = model_config.value();
  }

  HandleStatusUpdate();
}

void ModellerImpl::OnUserActivity(const ui::Event* event) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!event)
    return;
  ScheduleTrainerStart();
}

std::unique_ptr<ModellerImpl> ModellerImpl::CreateForTesting(
    const Profile* profile,
    AlsReader* als_reader,
    BrightnessMonitor* brightness_monitor,
    ModelConfigLoader* model_config_loader,
    ui::UserActivityDetector* user_activity_detector,
    std::unique_ptr<Trainer> trainer,
    scoped_refptr<base::SequencedTaskRunner> blocking_task_runner,
    const base::TickClock* tick_clock) {
  return base::WrapUnique(new ModellerImpl(
      profile, als_reader, brightness_monitor, model_config_loader,
      user_activity_detector, std::move(trainer), blocking_task_runner,
      tick_clock, true /* is_testing */));
}

base::Optional<double> ModellerImpl::AverageAmbientForTesting(
    base::TimeTicks now) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(log_als_values_);
  const base::Optional<AlsAvgStdDev> log_als_avg_stddev =
      log_als_values_->AverageAmbientWithStdDev(now);
  if (!log_als_avg_stddev)
    return base::nullopt;

  return log_als_avg_stddev->avg;
}

size_t ModellerImpl::NumberTrainingDataPointsForTesting() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return data_cache_.size();
}

size_t ModellerImpl::GetMaxTrainingDataPointsForTesting() const {
  return max_training_data_points_;
}

base::TimeDelta ModellerImpl::GetTrainingDelayForTesting() const {
  return training_delay_;
}

ModelConfig ModellerImpl::GetModelConfigForTesting() const {
  return model_config_;
}

ModellerImpl::ModelSavingSpec ModellerImpl::GetModelSavingSpecFromProfile(
    const Profile* profile) {
  DCHECK(profile);

  ModelSavingSpec model_saving_spec;
  const base::FilePath profile_path = profile->GetPath();
  if (profile_path.empty()) {
    return model_saving_spec;
  }

  const base::FilePath model_dir = profile_path.Append(kModelDir);
  if (!base::DirectoryExists(model_dir) && !base::CreateDirectory(model_dir)) {
    VLOG(1) << "Auto screen brightness model dir does not exist.";
    return model_saving_spec;
  }

  VLOG(1) << "Auto screen brightness model dir: " << model_dir.value();
  model_saving_spec.global_curve = model_dir.Append(kGlobalCurveFileName);
  model_saving_spec.personal_curve = model_dir.Append(kPersonalCurveFileName);
  model_saving_spec.iteration_count =
      model_dir.Append(kModelIterationCountFileName);

  return model_saving_spec;
}

ModellerImpl::ModellerImpl(
    const Profile* profile,
    AlsReader* als_reader,
    BrightnessMonitor* brightness_monitor,
    ModelConfigLoader* model_config_loader,
    ui::UserActivityDetector* user_activity_detector,
    std::unique_ptr<Trainer> trainer,
    const scoped_refptr<base::SequencedTaskRunner> blocking_task_runner,
    const base::TickClock* tick_clock,
    bool is_testing)
    : is_testing_(is_testing),
      als_reader_observer_(this),
      brightness_monitor_observer_(this),
      model_config_loader_observer_(this),
      user_activity_observer_(this),
      blocking_task_runner_(blocking_task_runner),
      trainer_(trainer.release(),
               base::OnTaskRunnerDeleter(blocking_task_runner_)),
      tick_clock_(tick_clock),
      model_timer_(tick_clock_),
      weak_ptr_factory_(this) {
  DCHECK(als_reader);
  DCHECK(brightness_monitor);
  DCHECK(model_config_loader);

  DCHECK(trainer_);
  DCHECK(user_activity_detector);

  if (!profile) {
    is_modeller_enabled_ = false;
    return;
  }

  if (!trainer_->HasValidConfiguration()) {
    is_modeller_enabled_ = false;
    return;
  }

  model_saving_spec_ = GetModelSavingSpecFromProfile(profile);
  if (model_saving_spec_.global_curve.empty()) {
    is_modeller_enabled_ = false;
    return;
  }

  als_reader_observer_.Add(als_reader);
  brightness_monitor_observer_.Add(brightness_monitor);
  model_config_loader_observer_.Add(model_config_loader);

  user_activity_observer_.Add(user_activity_detector);
}

void ModellerImpl::HandleStatusUpdate() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (is_modeller_enabled_.has_value())
    return;

  if (!als_init_status_.has_value())
    return;

  const bool als_success =
      *als_init_status_ == AlsReader::AlsInitStatus::kSuccess;
  if (!als_success) {
    is_modeller_enabled_ = false;
    OnInitializationComplete();
    return;
  }

  if (!brightness_monitor_success_.has_value()) {
    return;
  }
  if (!*brightness_monitor_success_) {
    is_modeller_enabled_ = false;
    OnInitializationComplete();
    return;
  }

  if (!model_config_exists_.has_value())
    return;

  if (!model_config_exists_.value()) {
    is_modeller_enabled_ = false;
    OnInitializationComplete();
    return;
  }

  if (!ApplyCustomization()) {
    is_modeller_enabled_ = false;
    OnInitializationComplete();
    return;
  }

  base::PostTaskAndReplyWithResult(
      blocking_task_runner_.get(), FROM_HERE,
      base::BindOnce(&LoadModelFromDisk, model_saving_spec_, is_testing_),
      base::BindOnce(&ModellerImpl::OnModelLoadedFromDisk,
                     weak_ptr_factory_.GetWeakPtr()));
}

bool ModellerImpl::ApplyCustomization() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(*model_config_exists_);

  initial_global_curve_ = MonotoneCubicSpline::CreateMonotoneCubicSpline(
      model_config_.log_lux, model_config_.brightness);
  if (!initial_global_curve_)
    return false;

  log_als_values_ = std::make_unique<AmbientLightSampleBuffer>(
      base::TimeDelta::FromSeconds(model_config_.model_als_horizon_seconds));

  // TODO(jiameng): the following params are probably not useful and can be
  // removed.
  const int max_training_data_points = GetFieldTrialParamByFeatureAsInt(
      features::kAutoScreenBrightness, "max_training_data_points", -1);
  if (max_training_data_points > 0) {
    max_training_data_points_ = max_training_data_points;
  }

  const int training_delay_in_seconds = GetFieldTrialParamByFeatureAsInt(
      features::kAutoScreenBrightness, "training_delay_in_seconds",
      training_delay_.InSeconds());
  if (training_delay_in_seconds >= 0) {
    training_delay_ = base::TimeDelta::FromSeconds(training_delay_in_seconds);
  }

  return true;
}

void ModellerImpl::OnInitializationComplete() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(is_modeller_enabled_.has_value());
  DCHECK(*is_modeller_enabled_ == model_.global_curve.has_value());

  UMA_HISTOGRAM_COUNTS_1000(
      "AutoScreenBrightness.ModelIterationCountAtInitialization",
      model_.iteration_count);

  for (auto& observer : observers_) {
    NotifyObserverInitStatus(observer);
  }
}

void ModellerImpl::NotifyObserverInitStatus(Modeller::Observer& observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(is_modeller_enabled_.has_value());
  observer.OnModelInitialized(model_);
}

void ModellerImpl::OnModelLoadedFromDisk(const Model& model) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(initial_global_curve_);

  model_ = model;
  if (!model_.global_curve || *model_.global_curve != *initial_global_curve_) {
    // Reset the model.
    model_.global_curve = initial_global_curve_;
    model_.personal_curve = base::nullopt;
    model_.iteration_count = 0;
    global_curve_reset_ = true;
  }

  DCHECK(model_.global_curve);
  // Run SetInitialCurves calculations on background thread to avoid blocking UI
  // thread.
  base::PostTaskAndReplyWithResult(
      blocking_task_runner_.get(), FROM_HERE,
      base::BindOnce(
          &SetInitialCurves, trainer_.get(), *model_.global_curve,
          model_.personal_curve ? *model_.personal_curve : *model_.global_curve,
          is_testing_),
      base::BindOnce(&ModellerImpl::OnSetInitialCurves,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ModellerImpl::OnModelSavedToDisk(bool is_successful) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const base::TimeTicks now = tick_clock_->NowTicks();

  UMA_HISTOGRAM_BOOLEAN("AutoScreenBrightness.NewCurveSaved.Success",
                        is_successful);
  if (is_successful) {
    UMA_HISTOGRAM_TIMES("AutoScreenBrightness.NewCurveSaved.Duration",
                        now - training_start_.value());
  }

  // We don't want to repeatedly save the global curve.
  global_curve_reset_ = false;
}

void ModellerImpl::OnSetInitialCurves(bool is_personal_curve_valid) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  UMA_HISTOGRAM_BOOLEAN("AutoScreenBrightness.PersonalCurveValid",
                        is_personal_curve_valid);

  const bool has_loaded_and_valid_personal_curve =
      model_.personal_curve && is_personal_curve_valid;
  DCHECK(model_.global_curve);
  DCHECK(trainer_->GetGlobalCurve() == *model_.global_curve);
  DCHECK(trainer_->GetCurrentCurve() == (has_loaded_and_valid_personal_curve
                                             ? *model_.personal_curve
                                             : *model_.global_curve));

  if (!has_loaded_and_valid_personal_curve) {
    model_.personal_curve = base::nullopt;
    model_.iteration_count = 0;
  } else if (model_.iteration_count == 0) {
    model_.iteration_count = 1;
  }

  is_modeller_enabled_ = true;
  OnInitializationComplete();

  // We may have received a brightness change as a training example before the
  // model is set up. Call |ScheduleTrainerStart| to prepare training.
  ScheduleTrainerStart();
}

void ModellerImpl::ScheduleTrainerStart() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!is_modeller_enabled_.has_value() || !*is_modeller_enabled_)
    return;

  if (data_cache_.size() >= max_training_data_points_ ||
      training_delay_.is_zero()) {
    model_timer_.Stop();
    StartTraining();
    return;
  }

  // Reset the timer if it's already running.
  model_timer_.Start(FROM_HERE, training_delay_, this,
                     &ModellerImpl::StartTraining);
}

void ModellerImpl::StartTraining() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (data_cache_.empty()) {
    return;
  }

  training_start_ = tick_clock_->NowTicks();
  base::PostTaskAndReplyWithResult(
      blocking_task_runner_.get(), FROM_HERE,
      base::BindOnce(&TrainModel, trainer_.get(), std::move(data_cache_),
                     is_testing_),
      base::BindOnce(&ModellerImpl::OnTrainingFinished,
                     weak_ptr_factory_.GetWeakPtr()));
  data_cache_ = std::vector<TrainingDataPoint>();
}

void ModellerImpl::OnTrainingFinished(const MonotoneCubicSpline& curve) {
  const base::TimeTicks now = tick_clock_->NowTicks();
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  ++model_.iteration_count;
  for (auto& observer : observers_)
    observer.OnModelTrained(curve);

  // Save personal curve if it doesn't exist or has been updated.
  const bool save_personal_curve =
      !model_.personal_curve || *model_.personal_curve != curve;
  const std::string histogram_name =
      std::string("AutoScreenBrightness.TrainingCompleteDuration.") +
      (save_personal_curve ? "NewCurve" : "NoNewCurve");
  base::UmaHistogramTimes(histogram_name, now - training_start_.value());

  if (save_personal_curve)
    model_.personal_curve = curve;

  base::PostTaskAndReplyWithResult(
      blocking_task_runner_.get(), FROM_HERE,
      base::BindOnce(&SaveModelToDisk, model_saving_spec_, model_,
                     global_curve_reset_, save_personal_curve, is_testing_),
      base::BindOnce(&ModellerImpl::OnModelSavedToDisk,
                     weak_ptr_factory_.GetWeakPtr()));
}

}  // namespace auto_screen_brightness
}  // namespace power
}  // namespace chromeos
