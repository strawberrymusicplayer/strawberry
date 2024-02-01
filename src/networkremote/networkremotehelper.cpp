#include "networkremote/networkremotehelper.h"

#include "core/application.h"
#include "core/logging.h"
#include "networkremote/networkremote.h"

NetworkRemoteHelper* NetworkRemoteHelper::sInstance = nullptr;

NetworkRemoteHelper::NetworkRemoteHelper(Application* app, QObject* parent)
  : QObject(parent),
    app_(app)
{
  app_ = app;
  sInstance = this;

  QObject::connect(this,&NetworkRemoteHelper::ReloadSettingsSig, &*app->network_remote(), &NetworkRemote::Init,Qt::QueuedConnection);
}

NetworkRemoteHelper::~NetworkRemoteHelper()
{}

void NetworkRemoteHelper::ReloadSettings()
{
  qLog(Debug) << "NetworkRemoteHelper called ----------------------";
  emit ReloadSettingsSig();
}

NetworkRemoteHelper* NetworkRemoteHelper::Instance() {
  if (!sInstance) {
    // Error
    return nullptr;
  }
  return sInstance;
}

