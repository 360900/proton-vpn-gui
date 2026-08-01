// natPmpService.cpp
// See natPmpService.h.

#include "natPmpService.h"

#include "debug.h"

#include <QPointer>
#include <QRegularExpression>

namespace
{
constexpr int KEEPALIVE_INTERVAL_MS = 45'000; // keeps a 60 s NAT-PMP lease alive
constexpr int NATPMPC_TIMEOUT_MS    = 20'000;
constexpr int CHECK_TIMEOUT_MS      = 5'000;
constexpr int NO_PORT               = 0;
} // namespace

NatPmpService::NatPmpService(ProcessRunner* runner, QObject* parent)
    : QObject(parent)
    , m_runner(runner)
{
    m_timer.setInterval(KEEPALIVE_INTERVAL_MS);
    connect(&m_timer, &QTimer::timeout, this, &NatPmpService::runOnce);
}

void NatPmpService::setGateway(const QString& gateway)
{
    m_gateway = gateway;
}

void NatPmpService::checkInstalled(const std::function<void(bool)>& done) const
{
    // `command -v` through the runner resolves against the HOST PATH when
    // sandboxed - QStandardPaths::findExecutable would only see the sandbox.
    m_runner->run(QStringLiteral("/bin/sh"),
                  {QStringLiteral("-c"), QStringLiteral("command -v natpmpc")},
                  CHECK_TIMEOUT_MS,
                  [done](const ProcessRunner::Result& r)
                  {
                      done(r.ok());
                  });
}

void NatPmpService::start()
{
    if (m_timer.isActive())
    {
        return;
    }
    m_timer.start();
    runOnce(); // fire immediately so the port appears without a 45 s wait
}

void NatPmpService::refresh()
{
    if (m_timer.isActive() == false)
    {
        start();
        return;
    }
    if (m_active == false)
    {
        runOnce();
    }
}

void NatPmpService::stop()
{
    m_timer.stop();
    m_active        = false;
    m_forwardedPort = NO_PORT;
}

// static
int NatPmpService::parseMappedPort(const QString& output)
{
    static const QRegularExpression re(QStringLiteral(R"(Mapped public port (\d+))"));
    const QRegularExpressionMatch match = re.match(output);
    return match.hasMatch() ? match.captured(1).toInt() : NO_PORT;
}

void NatPmpService::runOnce()
{
    if (m_active)
    {
        return; // previous invocation still in flight
    }
    m_active = true;

    // Both UDP and TCP mappings are required by the ProtonVPN port-forwarding
    // spec; the allocated port number is the same for both and is parsed from
    // the UDP response.
    const QString command = QStringLiteral("natpmpc -a 1 0 udp 60 -g %1 "
                                           "&& natpmpc -a 1 0 tcp 60 -g %1").arg(m_gateway);

    QPointer<NatPmpService> self(this);
    m_runner->run(QStringLiteral("/bin/sh"),
                  {QStringLiteral("-c"), command},
                  NATPMPC_TIMEOUT_MS,
                  [self](const ProcessRunner::Result& r)
                  {
                      if (self.isNull())
                      {
                          return;
                      }
                      self->m_active = false;

                      if (r.ok())
                      {
                          const int port = parseMappedPort(r.stdOut + r.stdErr);
                          if (port != NO_PORT)
                          {
                              self->m_forwardedPort = port;
                              emit self->portAcquired(port);
                          }
                          return;
                      }

                      // Failure: find out whether natpmpc is even installed
                      // on the host before deciding what to report.
                      self->checkInstalled([self](const bool installed)
                      {
                          if (self.isNull())
                          {
                              return;
                          }
                          if (installed == false)
                          {
                              DBG_CLI(QStringLiteral("natpmpc is not installed on the host."));
                              if (self->m_forwardedPort != NO_PORT)
                              {
                                  self->m_forwardedPort = NO_PORT;
                                  emit self->portLost();
                              }
                              self->stop();
                              emit self->natpmpcMissing();
                          }
                          else if (self->m_forwardedPort != NO_PORT)
                          {
                              self->m_forwardedPort = NO_PORT;
                              emit self->portLost();
                          }
                      });
                  });
}
