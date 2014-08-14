// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/native_viewport/native_viewport_service.h"

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/time/time.h"
#include "mojo/public/cpp/application/application_delegate.h"
#include "mojo/public/cpp/application/interface_factory.h"
#include "mojo/services/gles2/command_buffer_impl.h"
#include "mojo/services/native_viewport/platform_viewport.h"
#include "mojo/services/public/cpp/geometry/geometry_type_converters.h"
#include "mojo/services/public/cpp/input_events/input_events_type_converters.h"
#include "mojo/services/public/interfaces/native_viewport/native_viewport.mojom.h"
#include "ui/events/event.h"

namespace mojo {
namespace services {
namespace {

bool IsRateLimitedEventType(ui::Event* event) {
  return event->type() == ui::ET_MOUSE_MOVED ||
         event->type() == ui::ET_MOUSE_DRAGGED ||
         event->type() == ui::ET_TOUCH_MOVED;
}

}  // namespace

class NativeViewportImpl : public InterfaceImpl<NativeViewport>,
                           public PlatformViewport::Delegate {
 public:
  NativeViewportImpl()
      : widget_(gfx::kNullAcceleratedWidget),
        waiting_for_event_ack_(false),
        weak_factory_(this) {}
  virtual ~NativeViewportImpl() {
    // Destroy the NativeViewport early on as it may call us back during
    // destruction and we want to be in a known state.
    platform_viewport_.reset();
  }

  virtual void Create(RectPtr bounds) OVERRIDE {
    platform_viewport_ = PlatformViewport::Create(this);
    platform_viewport_->Init(bounds.To<gfx::Rect>());
    client()->OnCreated();
    OnBoundsChanged(bounds.To<gfx::Rect>());
  }

  virtual void Show() OVERRIDE {
    platform_viewport_->Show();
  }

  virtual void Hide() OVERRIDE {
    platform_viewport_->Hide();
  }

  virtual void Close() OVERRIDE {
    command_buffer_.reset();
    DCHECK(platform_viewport_);
    platform_viewport_->Close();
  }

  virtual void SetBounds(RectPtr bounds) OVERRIDE {
    platform_viewport_->SetBounds(bounds.To<gfx::Rect>());
  }

  virtual void CreateGLES2Context(
      InterfaceRequest<CommandBuffer> command_buffer_request) OVERRIDE {
    if (command_buffer_ || command_buffer_request_.is_pending()) {
      LOG(ERROR) << "Can't create multiple contexts on a NativeViewport";
      return;
    }
    command_buffer_request_ = command_buffer_request.Pass();
    CreateCommandBufferIfNeeded();
  }

  void AckEvent() {
    waiting_for_event_ack_ = false;
  }

  void CreateCommandBufferIfNeeded() {
    if (!command_buffer_request_.is_pending())
      return;
    DCHECK(!command_buffer_.get());
    if (widget_ == gfx::kNullAcceleratedWidget)
      return;
    gfx::Size size = platform_viewport_->GetSize();
    if (size.IsEmpty())
      return;
    command_buffer_.reset(
        new CommandBufferImpl(widget_, platform_viewport_->GetSize()));
    WeakBindToRequest(command_buffer_.get(), &command_buffer_request_);
  }

  virtual bool OnEvent(ui::Event* ui_event) OVERRIDE {
    // Must not return early before updating capture.
    switch (ui_event->type()) {
      case ui::ET_MOUSE_PRESSED:
      case ui::ET_TOUCH_PRESSED:
        platform_viewport_->SetCapture();
        break;
      case ui::ET_MOUSE_RELEASED:
      case ui::ET_TOUCH_RELEASED:
        platform_viewport_->ReleaseCapture();
        break;
      default:
        break;
    }

    if (waiting_for_event_ack_ && IsRateLimitedEventType(ui_event))
      return false;

    client()->OnEvent(
        TypeConverter<EventPtr, ui::Event>::ConvertFrom(*ui_event),
        base::Bind(&NativeViewportImpl::AckEvent,
                   weak_factory_.GetWeakPtr()));
    waiting_for_event_ack_ = true;
    return false;
  }

  virtual void OnAcceleratedWidgetAvailable(
      gfx::AcceleratedWidget widget) OVERRIDE {
    widget_ = widget;
    CreateCommandBufferIfNeeded();
  }

  virtual void OnBoundsChanged(const gfx::Rect& bounds) OVERRIDE {
    CreateCommandBufferIfNeeded();
    client()->OnBoundsChanged(Rect::From(bounds));
  }

  virtual void OnDestroyed() OVERRIDE {
    client()->OnDestroyed(base::Bind(&NativeViewportImpl::AckDestroyed,
                                     base::Unretained(this)));
  }

 private:
  void AckDestroyed() {
    command_buffer_.reset();
  }

  gfx::AcceleratedWidget widget_;
  scoped_ptr<PlatformViewport> platform_viewport_;
  InterfaceRequest<CommandBuffer> command_buffer_request_;
  scoped_ptr<CommandBufferImpl> command_buffer_;
  bool waiting_for_event_ack_;
  base::WeakPtrFactory<NativeViewportImpl> weak_factory_;
};

class NVSDelegate : public ApplicationDelegate,
                    public InterfaceFactory<mojo::NativeViewport> {
 public:
  NVSDelegate() {}
  virtual ~NVSDelegate() {}

  // ApplicationDelegate implementation.
  virtual bool ConfigureIncomingConnection(
      mojo::ApplicationConnection* connection) OVERRIDE {
    connection->AddService(this);
    return true;
  }

  // ServiceFactory<mojo::NativeViewport> implementation.
  virtual void Create(ApplicationConnection* connection,
                      InterfaceRequest<mojo::NativeViewport> request) OVERRIDE {
    BindToRequest(new NativeViewportImpl, &request);
  }
};

MOJO_NATIVE_VIEWPORT_EXPORT mojo::ApplicationImpl*
    CreateNativeViewportService(
        ScopedMessagePipeHandle service_provider_handle) {
  ApplicationImpl* app = new ApplicationImpl(
      new NVSDelegate(), service_provider_handle.Pass());
  return app;
}

}  // namespace services
}  // namespace mojo

