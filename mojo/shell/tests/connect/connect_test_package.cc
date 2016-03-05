// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/threading/simple_thread.h"
#include "mojo/public/c/system/main.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/shell/public/cpp/application_runner.h"
#include "mojo/shell/public/cpp/interface_factory.h"
#include "mojo/shell/public/cpp/shell_client.h"
#include "mojo/shell/public/interfaces/shell_client_factory.mojom.h"
#include "mojo/shell/tests/connect/connect_test.mojom.h"

// Tests that multiple applications can be packaged in a single Mojo application
// implementing ShellClientFactory; that these applications can be specified by
// the package's manifest and are thus registered with the PackageManager.

namespace mojo {
namespace shell {

using GetTitleCallback = test::mojom::ConnectTestService::GetTitleCallback;

class ProvidedShellClient
    : public ShellClient,
      public InterfaceFactory<test::mojom::ConnectTestService>,
      public InterfaceFactory<test::mojom::BlockedInterface>,
      public test::mojom::ConnectTestService,
      public test::mojom::BlockedInterface,
      public base::SimpleThread {
 public:
  ProvidedShellClient(const std::string& title,
                      mojom::ShellClientRequest request)
      : base::SimpleThread(title),
        title_(title),
        request_(std::move(request)) {
    Start();
  }
  ~ProvidedShellClient() override {
    Join();
  }

 private:
  // mojo::ShellClient:
  void Initialize(Connector* connector, const std::string& name,
                  const std::string& user_id, uint32_t id) override {
    name_ = name;
    id_ = id;
    userid_ = user_id;
    bindings_.set_connection_error_handler(
        base::Bind(&ProvidedShellClient::OnConnectionError,
                   base::Unretained(this)));
  }
  bool AcceptConnection(Connection* connection) override {
    connection->AddInterface<test::mojom::ConnectTestService>(this);
    connection->AddInterface<test::mojom::BlockedInterface>(this);

    uint32_t remote_id = mojom::Connector::kInvalidApplicationID;
    connection->GetRemoteApplicationID(&remote_id);
    test::mojom::ConnectionStatePtr state(test::mojom::ConnectionState::New());
    state->connection_local_name = connection->GetConnectionName();
    state->connection_remote_name = connection->GetRemoteApplicationName();
    state->connection_remote_userid = connection->GetRemoteUserID();
    state->connection_remote_id = remote_id;
    state->initialize_local_name = name_;
    state->initialize_id = id_;
    state->initialize_userid = userid_;
    connection->GetInterface(&caller_);
    caller_->ConnectionAccepted(std::move(state));

    return true;
  }

  // InterfaceFactory<test::mojom::ConnectTestService>:
  void Create(Connection* connection,
              test::mojom::ConnectTestServiceRequest request) override {
    bindings_.AddBinding(this, std::move(request));
  }

  // InterfaceFactory<test::mojom::BlockedInterface>:
  void Create(Connection* connection,
              test::mojom::BlockedInterfaceRequest request) override {
    blocked_bindings_.AddBinding(this, std::move(request));
  }

  // test::mojom::ConnectTestService:
  void GetTitle(const GetTitleCallback& callback) override {
    callback.Run(title_);
  }

  // test::mojom::BlockedInterface:
  void GetTitleBlocked(const GetTitleBlockedCallback& callback) override {
    callback.Run("Called Blocked Interface!");
  }

  // base::SimpleThread:
  void Run() override {
    ApplicationRunner(this).Run(request_.PassMessagePipe().release().value(),
                                false);
    delete this;
  }

  void OnConnectionError() {
    if (bindings_.empty())
      base::MessageLoop::current()->QuitWhenIdle();
  }

  std::string name_;
  uint32_t id_ = mojom::Connector::kInvalidApplicationID;
  std::string userid_ = mojom::kRootUserID;
  const std::string title_;
  mojom::ShellClientRequest request_;
  test::mojom::ExposedInterfacePtr caller_;
  BindingSet<test::mojom::ConnectTestService> bindings_;
  BindingSet<test::mojom::BlockedInterface> blocked_bindings_;

  DISALLOW_COPY_AND_ASSIGN(ProvidedShellClient);
};

class ConnectTestShellClient
    : public ShellClient,
      public InterfaceFactory<mojom::ShellClientFactory>,
      public InterfaceFactory<test::mojom::ConnectTestService>,
      public mojom::ShellClientFactory,
      public test::mojom::ConnectTestService {
 public:
  ConnectTestShellClient() {}
  ~ConnectTestShellClient() override {}

 private:
  // mojo::ShellClient:
  void Initialize(Connector* connector, const std::string& name,
                  const std::string& user_id, uint32_t id) override {
    bindings_.set_connection_error_handler(
        base::Bind(&ConnectTestShellClient::OnConnectionError,
                   base::Unretained(this)));
  }
  bool AcceptConnection(Connection* connection) override {
    connection->AddInterface<ShellClientFactory>(this);
    connection->AddInterface<test::mojom::ConnectTestService>(this);
    return true;
  }
  void ShellConnectionLost() override {
    if (base::MessageLoop::current() &&
        base::MessageLoop::current()->is_running()) {
      base::MessageLoop::current()->QuitWhenIdle();
    }
  }

  // InterfaceFactory<mojom::ShellClientFactory>:
  void Create(Connection* connection,
              mojom::ShellClientFactoryRequest request) override {
    shell_client_factory_bindings_.AddBinding(this, std::move(request));
  }

  // InterfaceFactory<test::mojom::ConnectTestService>:
  void Create(Connection* connection,
              test::mojom::ConnectTestServiceRequest request) override {
    bindings_.AddBinding(this, std::move(request));
  }

  // mojom::ShellClientFactory:
  void CreateShellClient(mojom::ShellClientRequest request,
                         const String& name) override {
    if (name == "mojo:connect_test_a")
      new ProvidedShellClient("A", std::move(request));
    else if (name == "mojo:connect_test_b")
      new ProvidedShellClient("B", std::move(request));
  }

  // test::mojom::ConnectTestService:
  void GetTitle(const GetTitleCallback& callback) override {
    callback.Run("ROOT");
  }

  void OnConnectionError() {
    if (bindings_.empty())
      base::MessageLoop::current()->QuitWhenIdle();
  }

  std::vector<scoped_ptr<ShellClient>> delegates_;
  BindingSet<mojom::ShellClientFactory> shell_client_factory_bindings_;
  BindingSet<test::mojom::ConnectTestService> bindings_;

  DISALLOW_COPY_AND_ASSIGN(ConnectTestShellClient);
};

}  // namespace shell
}  // namespace mojo


MojoResult MojoMain(MojoHandle shell_handle) {
  MojoResult rv = mojo::ApplicationRunner(
      new mojo::shell::ConnectTestShellClient).Run(shell_handle);
  return rv;
}
