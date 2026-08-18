/*---------------------------------------------------------*\
| OpenRGBRipplePlugin.cpp                                   |
\*---------------------------------------------------------*/

#include "OpenRGBRipplePlugin.h"
#include "RippleWidget.h"
#include "ResourceManagerInterface.h"
#include "RGBController.h"

#include <QAction>
#include <QCoreApplication>
#include <QEvent>
#include <QMenu>
#include <QThread>
#include <cstdio>

OpenRGBPluginInfo OpenRGBRipplePlugin::GetPluginInfo()
{
    OpenRGBPluginInfo info;
    info.Name        = "OpenRGB Ripple Plugin";
    info.Description = "Artemis-style key-press ripple for RGB keyboards";
    info.Version     = "1.0.2";
    info.Commit      = "release";
    info.URL         = "https://github.com/camAtGitHub/openRGB-ripple-plugin";
    info.Label       = "Ripple";
    info.Location    = OPENRGB_PLUGIN_LOCATION_TOP;
    info.Icon.load(":/OpenRGBRipplePlugin.png");
    return info;
}

unsigned int OpenRGBRipplePlugin::GetPluginAPIVersion()
{
    return 4; /* OpenRGB 1.0rc2 / 1.0rc3 */
}

void OpenRGBRipplePlugin::Load(ResourceManagerInterface* resource_manager_ptr)
{
    rm_ = resource_manager_ptr;
    alive_ = false;
    printf("[OpenRGBRipplePlugin] Loaded (Plugin API %u)\n", OPENRGB_PLUGIN_API_VERSION);
}

void OpenRGBRipplePlugin::DetectionStartCallback(void* arg)
{
    auto* self = static_cast<OpenRGBRipplePlugin*>(arg);
    if(!self || !self->alive_.load())
    {
        return;
    }
    if(QThread::currentThread() == self->thread())
    {
        self->OnDetectionStart();
    }
    else
    {
        QMetaObject::invokeMethod(self, "OnDetectionStart",
                                  Qt::BlockingQueuedConnection);
    }
}

void OpenRGBRipplePlugin::DeviceListChangedCallback(void* arg)
{
    auto* self = static_cast<OpenRGBRipplePlugin*>(arg);
    if(!self || !self->alive_.load())
    {
        return;
    }
    if(QThread::currentThread() == self->thread())
    {
        self->OnDeviceListChanged();
    }
    else
    {
        /* Inside UpdateDeviceList the controller vector is stable.
           Block until GUI drops/rebuilds pointers before we return. */
        QMetaObject::invokeMethod(self, "OnDeviceListChanged",
                                  Qt::BlockingQueuedConnection);
    }
}

void OpenRGBRipplePlugin::DetectionEndCallback(void* arg)
{
    auto* self = static_cast<OpenRGBRipplePlugin*>(arg);
    if(!self || !self->alive_.load())
    {
        return;
    }
    QMetaObject::invokeMethod(self, "OnDetectionEnd", Qt::QueuedConnection);
}

void OpenRGBRipplePlugin::OnDetectionStart()
{
    if(!alive_.load())
    {
        return;
    }
    session_.Invalidate();
    if(RippleWidget* w = ui_.data())
    {
        w->PausePainting();
    }
}

void OpenRGBRipplePlugin::OnDeviceListChanged()
{
    if(!alive_.load() || !rm_)
    {
        return;
    }
    if(rm_->GetDetectionPercent() < 100)
    {
        session_.Invalidate();
        if(RippleWidget* w = ui_.data())
        {
            w->PausePainting();
        }
        return;
    }
    session_.Rebuild(CopyControllers());
    if(RippleWidget* w = ui_.data())
    {
        w->BindDevicesFromSession();
        session_.PushDirectMode();
        w->ResumePainting();
    }
}

void OpenRGBRipplePlugin::OnDetectionEnd()
{
    if(!alive_.load() || !rm_)
    {
        return;
    }
    session_.Rebuild(CopyControllers());
    if(RippleWidget* w = ui_.data())
    {
        w->BindDevicesFromSession();
        session_.PushDirectMode();
        w->ResumePainting();
    }
}

std::vector<RGBController*> OpenRGBRipplePlugin::CopyControllers() const
{
    std::vector<RGBController*>& live = rm_->GetRGBControllers();
    return std::vector<RGBController*>(live.begin(), live.end());
}

void OpenRGBRipplePlugin::RegisterHostCallbacks()
{
    if(!rm_ || callbacks_registered_)
    {
        return;
    }
    rm_->RegisterDetectionStartCallback(&OpenRGBRipplePlugin::DetectionStartCallback, this);
    rm_->RegisterDeviceListChangeCallback(&OpenRGBRipplePlugin::DeviceListChangedCallback, this);
    rm_->RegisterDetectionEndCallback(&OpenRGBRipplePlugin::DetectionEndCallback, this);
    callbacks_registered_ = true;
}

void OpenRGBRipplePlugin::UnregisterHostCallbacks()
{
    if(!rm_ || !callbacks_registered_)
    {
        return;
    }
    rm_->UnregisterDetectionStartCallback(&OpenRGBRipplePlugin::DetectionStartCallback, this);
    rm_->UnregisterDeviceListChangeCallback(&OpenRGBRipplePlugin::DeviceListChangedCallback, this);
    rm_->UnregisterDetectionEndCallback(&OpenRGBRipplePlugin::DetectionEndCallback, this);
    callbacks_registered_ = false;
}

QWidget* OpenRGBRipplePlugin::GetWidget()
{
    if(!rm_)
    {
        return nullptr;
    }
    if(RippleWidget* existing = ui_.data())
    {
        return existing;
    }
    rm_->WaitForDeviceDetection();
    alive_ = true;
    RegisterHostCallbacks();          /* BEFORE any RGBController* is cached */
    auto* w = new RippleWidget(rm_, &session_);
    ui_ = w;
    session_.Rebuild(CopyControllers());
    w->BindDevicesFromSession();
    session_.PushDirectMode();
    w->StartRuntime();                /* hook + timer, after callbacks exist */
    return w;
}

QMenu* OpenRGBRipplePlugin::GetTrayMenu()
{
    QMenu* menu = new QMenu("Ripple", ui_.data());
    QAction* enable = menu->addAction("Enable");
    QAction* disable = menu->addAction("Disable");
    QObject::connect(enable, &QAction::triggered, [this]()
    {
        if(RippleWidget* w = ui_.data()) w->SetEnabled(true);
    });
    QObject::connect(disable, &QAction::triggered, [this]()
    {
        if(RippleWidget* w = ui_.data()) w->SetEnabled(false);
    });
    return menu;
}

void OpenRGBRipplePlugin::Unload()
{
    printf("[OpenRGBRipplePlugin] Unloading\n");
    alive_ = false;
    UnregisterHostCallbacks();
    QCoreApplication::removePostedEvents(this, QEvent::MetaCall);
    session_.Invalidate();
    if(RippleWidget* w = ui_.data())
    {
        w->Shutdown();
    }
    ui_.clear();
}
