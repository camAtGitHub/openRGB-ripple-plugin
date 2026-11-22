/*---------------------------------------------------------*\
| OpenRGBRipplePlugin.cpp                                   |
\*---------------------------------------------------------*/

#include "OpenRGBRipplePlugin.h"
#include "DetectionLifecycle.h"
#include "RippleWidget.h"
#include "ResourceManagerCallback.h"
#include "RGBControllerInterface.h"

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
    info.Version     = "1.1.0";
    info.Commit      = "release";
    info.URL         = "https://github.com/camAtGitHub/openRGB-ripple-plugin";
    info.Label       = "Ripple";
    info.Location    = OPENRGB_PLUGIN_LOCATION_TOP;
    info.Icon.load(":/OpenRGBRipplePlugin.png");
    return info;
}

unsigned int OpenRGBRipplePlugin::GetPluginAPIVersion()
{
    return OPENRGB_PLUGIN_API_VERSION; /* 5: OpenRGB 1.1 */
}

void OpenRGBRipplePlugin::Load(OpenRGBPluginAPIInterface* plugin_api_ptr)
{
    api_ = plugin_api_ptr;
    alive_ = false;
    paused_ = false;
    printf("[OpenRGBRipplePlugin] Loaded (Plugin API %u)\n", OPENRGB_PLUGIN_API_VERSION);
}

/* API 5 has no per-plugin RM callbacks; PluginManager forwards every
   ResourceManager signal here, on whichever thread raised it. Same order
   as the old callbacks: DETECTION_STARTED, then DEVICE_LIST_UPDATED
   (list already swapped, old controllers deleted only after we return),
   then DETECTION_COMPLETE. */
void OpenRGBRipplePlugin::ResourceManagerUpdated(unsigned int update_reason)
{
    switch(update_reason)
    {
        case RESOURCEMANAGER_UPDATE_REASON_DETECTION_STARTED:
            DetectionStarted();
            break;
        case RESOURCEMANAGER_UPDATE_REASON_DEVICE_LIST_UPDATED:
            DeviceListChanged();
            break;
        case RESOURCEMANAGER_UPDATE_REASON_DETECTION_COMPLETE:
            DetectionEnded();
            break;
        default:
            break;
    }
}

void OpenRGBRipplePlugin::DetectionStarted()
{
    if(!alive_.load())
    {
        return;
    }
    /* Bump before the GUI slot so a stale queued OnDetectionEnd cannot
       Rebuild/UpdateMode after we have decided controllers are dying. */
    suspend_epoch_.fetch_add(1);
    paused_.store(true);
    if(QThread::currentThread() == thread())
    {
        OnDetectionStart();
    }
    else
    {
        QMetaObject::invokeMethod(this, "OnDetectionStart",
                                  Qt::BlockingQueuedConnection);
    }
}

void OpenRGBRipplePlugin::DeviceListChanged()
{
    if(!alive_.load())
    {
        return;
    }
    if(QThread::currentThread() == thread())
    {
        OnDeviceListChanged();
    }
    else
    {
        /* Inside UpdateDeviceList the controller vector is stable.
           Block until GUI drops/rebuilds pointers before we return. */
        QMetaObject::invokeMethod(this, "OnDeviceListChanged",
                                  Qt::BlockingQueuedConnection);
    }
}

void OpenRGBRipplePlugin::DetectionEnded()
{
    if(!alive_.load())
    {
        return;
    }
    queued_end_epoch_.store(suspend_epoch_.load());
    QMetaObject::invokeMethod(this, "OnDetectionEnd", Qt::QueuedConnection);
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
    if(!alive_.load() || !api_)
    {
        return;
    }
    if(paused_.load() || api_->GetDetectionPercent() < 100)
    {
        session_.Invalidate();
        if(RippleWidget* w = ui_.data())
        {
            w->PausePainting();
        }
        return;
    }
    session_.Rebuild(CopyControllers());
    if(paused_.load())
    {
        session_.Invalidate();
        if(RippleWidget* w = ui_.data())
        {
            w->PausePainting();
        }
        return;
    }
    if(RippleWidget* w = ui_.data())
    {
        w->BindDevicesFromSession();
        session_.PushDirectMode();
        w->ResumePainting();
    }
}

void OpenRGBRipplePlugin::OnDetectionEnd()
{
    if(!alive_.load() || !api_)
    {
        return;
    }
    const uint64_t end_epoch = queued_end_epoch_.load();
    if(!DetectionEndIsCurrent(end_epoch, suspend_epoch_.load()))
    {
        return;
    }
    session_.Rebuild(CopyControllers());
    if(!DetectionEndIsCurrent(end_epoch, suspend_epoch_.load()))
    {
        /* A newer DetectionStart won the race; do not touch dying controllers. */
        session_.Invalidate();
        if(RippleWidget* w = ui_.data())
        {
            w->PausePainting();
        }
        return;
    }
    paused_.store(false);
    if(RippleWidget* w = ui_.data())
    {
        w->BindDevicesFromSession();
        session_.PushDirectMode();
        w->ResumePainting();
    }
}

std::vector<RGBControllerInterface*> OpenRGBRipplePlugin::CopyControllers() const
{
    return api_->GetRGBControllers(); /* already a copy in API 5 */
}

QWidget* OpenRGBRipplePlugin::GetWidget()
{
    if(!api_)
    {
        return nullptr;
    }
    if(RippleWidget* existing = ui_.data())
    {
        return existing;
    }
    /* No WaitForDetection() here: this runs on the GUI thread, and 1.1's
       detection thread blocks on the GUI for DEVICE_LIST_UPDATED, so
       waiting would deadlock. If detection is running, start paused and
       let DETECTION_COMPLETE rebuild and resume. */
    alive_ = true;                    /* BEFORE any controller ptr is cached */
    paused_ = api_->GetDetectionPercent() < 100;
    auto* w = new RippleWidget(api_, &session_);
    ui_ = w;
    if(paused_.load())
    {
        session_.Invalidate();
        w->BindDevicesFromSession();
        w->PausePainting();
    }
    else
    {
        session_.Rebuild(CopyControllers());
        w->BindDevicesFromSession();
        session_.PushDirectMode();
    }
    w->StartRuntime();                /* hook + timer */
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
    alive_ = false;                   /* ResourceManagerUpdated now no-ops */
    paused_ = true;
    QCoreApplication::removePostedEvents(this, QEvent::MetaCall);
    session_.Invalidate();
    if(RippleWidget* w = ui_.data())
    {
        w->Shutdown();
    }
    ui_.clear();
}
