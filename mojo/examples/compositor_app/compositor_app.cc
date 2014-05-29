// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>
#include <string>

#include "base/macros.h"
#include "mojo/examples/compositor_app/compositor_host.h"
#include "mojo/public/cpp/application/application.h"
#include "mojo/public/cpp/bindings/allocation_scope.h"
#include "mojo/public/cpp/gles2/gles2.h"
#include "mojo/public/cpp/system/core.h"
#include "mojo/public/interfaces/service_provider/service_provider.mojom.h"
#include "mojo/services/native_viewport/native_viewport.mojom.h"
#include "mojo/services/public/cpp/geometry/geometry_type_converters.h"
#include "ui/gfx/rect.h"

namespace mojo {
namespace examples {

class SampleApp : public Application, public NativeViewportClient {
 public:
  SampleApp() {}
  virtual ~SampleApp() {}

  virtual void Initialize() OVERRIDE {
    AllocationScope scope;

    ConnectTo("mojo:mojo_native_viewport_service", &viewport_);
    viewport_.set_client(this);

    viewport_->Create(gfx::Rect(10, 10, 800, 600));
    viewport_->Show();

    MessagePipe gles2_pipe;
    viewport_->CreateGLES2Context(gles2_pipe.handle0.Pass());
    host_.reset(new CompositorHost(gles2_pipe.handle1.Pass()));
  }

  virtual void OnCreated() OVERRIDE {
  }

  virtual void OnDestroyed() OVERRIDE {
    base::MessageLoop::current()->Quit();
  }

  virtual void OnBoundsChanged(const Rect& bounds) OVERRIDE {
    AllocationScope scope;
    Size::Builder size;
    size.set_width(bounds.width());
    size.set_height(bounds.height());
    host_->SetSize(size.Finish());
  }

  virtual void OnEvent(const Event& event,
                       const mojo::Callback<void()>& callback) OVERRIDE {
    callback.Run();
  }

 private:
  mojo::GLES2Initializer gles2;
  NativeViewportPtr viewport_;
  scoped_ptr<CompositorHost> host_;
  DISALLOW_COPY_AND_ASSIGN(SampleApp);
};

}  // namespace examples

// static
Application* Application::Create() {
  return new examples::SampleApp();
}

}  // namespace mojo
