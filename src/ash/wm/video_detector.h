// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_VIDEO_DETECTOR_H_
#define ASH_WM_VIDEO_DETECTOR_H_

#include <map>
#include <memory>
#include <set>

#include "ash/ash_export.h"
#include "ash/session/session_observer.h"
#include "ash/shell_observer.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "base/scoped_observer.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/viz/public/mojom/compositing/video_detector_observer.mojom.h"
#include "ui/aura/env_observer.h"
#include "ui/aura/window_observer.h"

namespace aura {
class Window;
}

namespace ash {

// Receives notifications from viz::VideoDetector about whether it is likely
// that a video is being played on screen. If video activity is detected, this
// class will classify it as full screen or windowed.
class ASH_EXPORT VideoDetector : public aura::EnvObserver,
                                 public aura::WindowObserver,
                                 public SessionObserver,
                                 public ShellObserver,
                                 public viz::mojom::VideoDetectorObserver {
 public:
  // State of detected video activity.
  enum class State {
    // Video activity has been detected recently and there are no fullscreen
    // windows.
    PLAYING_WINDOWED,
    // Video activity has been detected recently and there is at least one
    // fullscreen window.
    PLAYING_FULLSCREEN,
    // Video activity has not been detected recently.
    NOT_PLAYING,
  };

  class Observer {
   public:
    // Invoked when the video playback state has changed.
    virtual void OnVideoStateChanged(VideoDetector::State state) = 0;

   protected:
    virtual ~Observer() {}
  };

  VideoDetector();
  ~VideoDetector() override;

  State state() const { return state_; }

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // EnvObserver overrides.
  void OnWindowInitialized(aura::Window* window) override;

  // aura::WindowObserver overrides.
  void OnWindowDestroyed(aura::Window* window) override;
  void OnWindowDestroying(aura::Window* window) override;

  // SessionStateController overrides.
  void OnChromeTerminating() override;

  // ShellObserver overrides.
  void OnFullscreenStateChanged(bool is_fullscreen,
                                aura::Window* container) override;

  // viz::mojom::VideoDetectorObserver implementation.
  void OnVideoActivityStarted() override;
  void OnVideoActivityEnded() override;

 private:
  // Updates |state_| and notifies |observers_| if it changed.
  void UpdateState();

  // Connects to Viz and starts observing video activities.
  void EstablishConnectionToViz();

  // Called when connection to Viz is lost. The connection will be
  // re-established after a short delay.
  void OnConnectionError();

  // Current playback state.
  State state_;

  // True if video has been observed in the last |kVideoTimeoutMs|.
  bool video_is_playing_;

  // Currently-fullscreen desks containers windows.
  std::set<aura::Window*> fullscreen_desks_containers_;

  base::ObserverList<Observer>::Unchecked observers_;

  ScopedObserver<aura::Window, aura::WindowObserver> window_observer_manager_;
  ScopedSessionObserver scoped_session_observer_;

  bool is_shutting_down_;

  mojo::Binding<viz::mojom::VideoDetectorObserver> binding_;

  base::WeakPtrFactory<VideoDetector> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(VideoDetector);
};

}  // namespace ash

#endif  // ASH_WM_VIDEO_DETECTOR_H_
