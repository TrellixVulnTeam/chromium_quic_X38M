// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/blink/prediction/input_predictor_unittest_helpers.h"
#include "ui/events/blink/prediction/kalman_predictor.h"

namespace ui {
namespace test {
namespace {

constexpr uint32_t kExpectedStableIterNum = 4;

struct DataSet {
  double initial_observation;
  std::vector<double> observation;
  std::vector<double> position;
  std::vector<double> velocity;
  std::vector<double> acceleration;
};

void ValidateSingleKalmanFilter(const DataSet& data) {
  std::unique_ptr<KalmanFilter> kalman_filter =
      std::make_unique<KalmanFilter>();
  constexpr double kEpsilon = 0.001;
  constexpr double kDtMillisecond = 8;
  kalman_filter->Update(data.initial_observation, kDtMillisecond);
  for (size_t i = 0; i < data.observation.size(); i++) {
    kalman_filter->Update(data.observation[i], kDtMillisecond);
    EXPECT_NEAR(data.position[i], kalman_filter->GetPosition(), kEpsilon);
    EXPECT_NEAR(data.velocity[i], kalman_filter->GetVelocity(), kEpsilon);
    EXPECT_NEAR(data.acceleration[i], kalman_filter->GetAcceleration(),
                kEpsilon);
  }
}

}  // namespace

class KalmanPredictorTest : public InputPredictorTest {
 public:
  explicit KalmanPredictorTest() {}

  void SetUp() override {
    predictor_ = std::make_unique<ui::KalmanPredictor>();
  }

  DISALLOW_COPY_AND_ASSIGN(KalmanPredictorTest);
};

// Test the a single axle kalman filter behavior with preset datas.
TEST_F(KalmanPredictorTest, KalmanFilterPredictedValue) {
  DataSet data;
  data.initial_observation = 0;
  data.observation = {1, 2, 3, 4, 5, 6};
  data.position = {0.999, 2.007, 3.001, 3.999, 5.000, 6.000};
  data.velocity = {0.242, 0.130, 0.122, 0.124, 0.125, 0.125};
  data.acceleration = {0.029, 0.000, 0.000, 0.000, 0.000, 0.000};
  ValidateSingleKalmanFilter(data);

  data.initial_observation = 0;
  data.observation = {1, 2, 4, 8, 16, 32};
  data.position = {0.999, 2.007, 3.976, 7.970, 15.950, 31.896};
  data.velocity = {0.242, 0.130, 0.298, 0.623, 1.240, 2.475};
  data.acceleration = {0.029, 0.000, 0.015, 0.034, 0.065, 0.130};
  ValidateSingleKalmanFilter(data);
}

TEST_F(KalmanPredictorTest, ShouldHasPrediction) {
  for (uint32_t i = 0; i < kExpectedStableIterNum; i++) {
    EXPECT_FALSE(predictor_->HasPrediction());
    InputPredictor::InputData data = {gfx::PointF(1, 1),
                                      FromMilliseconds(8 * i)};
    predictor_->Update(data);
  }
  EXPECT_TRUE(predictor_->HasPrediction());

  predictor_->Reset();
  EXPECT_FALSE(predictor_->HasPrediction());
}

// Tests the kalman predictor constant position.
TEST_F(KalmanPredictorTest, PredictConstantValue) {
  std::vector<double> x = {50, 50, 50, 50, 50, 50};
  std::vector<double> y = {-50, -50, -50, -50, -50, -50};
  std::vector<double> t = {8, 16, 24, 32, 40, 48};
  ValidatePredictor(x, y, t);
}

// Tests the kalman predictor predict constant velocity.
TEST_F(KalmanPredictorTest, PredictLinearValue) {
  // The kalman filter is initialized with a velocity of zero. The change of
  // velocity from zero to the constant value will be attributed to
  // acceleration. Given how the filter works, it will take a few updates for it
  // to get accustomed to a constant velocity.
  std::vector<double> x_stabilizer = {-40, -32, -24, -16, -8};
  std::vector<double> y_stabilizer = {-10, -2, 6, 14, 22};
  std::vector<double> t_stabilizer = {-40, -32, -24, -16, -8};
  for (size_t i = 0; i < t_stabilizer.size(); i++) {
    InputPredictor::InputData data = {
        gfx::PointF(x_stabilizer[i], y_stabilizer[i]),
        FromMilliseconds(t_stabilizer[i])};
    predictor_->Update(data);
  }

  std::vector<double> x = {0, 8, 16, 24, 32, 40, 48, 60};
  std::vector<double> y = {30, 38, 46, 54, 62, 70, 78, 90};
  std::vector<double> t = {0, 8, 16, 24, 32, 40, 48, 60};
  for (size_t i = 0; i < t.size(); i++) {
    if (predictor_->HasPrediction()) {
      ui::InputPredictor::InputData result;
      EXPECT_TRUE(
          predictor_->GeneratePrediction(FromMilliseconds(t[i]), &result));
      EXPECT_NEAR(result.pos.x(), x[i], kEpsilon);
      EXPECT_NEAR(result.pos.y(), y[i], kEpsilon);
    }
    InputPredictor::InputData data = {gfx::PointF(x[i], y[i]),
                                      FromMilliseconds(t[i])};
    predictor_->Update(data);
  }
}

// Tests the kalman predictor predict constant acceleration.
TEST_F(KalmanPredictorTest, PredictQuadraticValue) {
  std::vector<double> x = {0, 2, 8, 18, 32, 50, 72, 98};
  std::vector<double> y = {10, 11, 14, 19, 26, 35, 46, 59};
  std::vector<double> t = {8, 16, 24, 32, 40, 48, 56, 64};
  ValidatePredictor(x, y, t);
}

// Tests the kalman predictor time interval filter.
TEST_F(KalmanPredictorTest, TimeInterval) {
  predictor_ = std::make_unique<ui::KalmanPredictor>();
  EXPECT_EQ(predictor_->TimeInterval(), kExpectedDefaultTimeInterval);
  std::vector<double> x = {0, 2, 8, 18};
  std::vector<double> y = {10, 11, 14, 19};
  std::vector<double> t = {7, 14, 21, 28};
  for (size_t i = 0; i < t.size(); i++) {
    InputPredictor::InputData data = {gfx::PointF(x[i], y[i]),
                                      FromMilliseconds(t[i])};
    predictor_->Update(data);
  }
  EXPECT_EQ(predictor_->TimeInterval().InMillisecondsF(),
            base::TimeDelta::FromMilliseconds(7).InMillisecondsF());
}

}  // namespace test
}  // namespace ui
