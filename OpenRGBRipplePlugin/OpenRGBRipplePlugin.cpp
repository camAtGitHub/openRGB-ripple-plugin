/*---------------------------------------------------------*\
| OpenRGBRipplePlugin.cpp                                   |
\*---------------------------------------------------------*/

#include "OpenRGBRipplePlugin.h"
#include "RippleWidget.h"
#include "ResourceManagerInterface.h"

#include <QAction>
#include <QMenu>
#include <cstdio>

OpenRGBPluginInfo OpenRGBRipplePlugin::GetPluginInfo()
{
    OpenRGBPluginInfo info;
    info.Name        = "OpenRGB Ripple Plugin";
    info.Description = "Artemis-style key-press ripple for RGB keyboards";
    info.Version     = "1.0.0";
    info.Commit      = "release";
    info.URL         = "https://openrgb.org";
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
    printf("[OpenRGBRipplePlugin] Loaded (Plugin API %u)\n", OPENRGB_PLUGIN_API_VERSION);
}

void OpenRGBRipplePlugin::DeviceListChangedCallback(void* arg)
{
    RippleWidget* ui = static_cast<RippleWidget*>(arg);
    if(ui)
    {
        QMetaObject::invokeMethod(ui, "RebuildDevicesSlot", Qt::QueuedConnection);
    }
}

QWidget* OpenRGBRipplePlugin::GetWidget()
{
    if(!rm_)
    {
        return nullptr;
    }
    rm_->WaitForDeviceDetection();
    ui_ = new RippleWidget(rm_);
    rm_->RegisterDeviceListChangeCallback(&OpenRGBRipplePlugin::DeviceListChangedCallback, ui_);
    rm_->RegisterDetectionProgressCallback(&OpenRGBRipplePlugin::DeviceListChangedCallback, ui_);
    return ui_;
}

QMenu* OpenRGBRipplePlugin::GetTrayMenu()
{
    QMenu* menu = new QMenu("Ripple", ui_);
    QAction* enable = menu->addAction("Enable");
    QAction* disable = menu->addAction("Disable");
    QObject::connect(enable, &QAction::triggered, [this]()
    {
        if(ui_) ui_->SetEnabled(true);
    });
    QObject::connect(disable, &QAction::triggered, [this]()
    {
        if(ui_) ui_->SetEnabled(false);
    });
    return menu;
}

void OpenRGBRipplePlugin::Unload()
{
    printf("[OpenRGBRipplePlugin] Unloading\n");
    if(rm_ && ui_)
    {
        rm_->UnregisterDeviceListChangeCallback(&OpenRGBRipplePlugin::DeviceListChangedCallback, ui_);
        rm_->UnregisterDetectionProgressCallback(&OpenRGBRipplePlugin::DeviceListChangedCallback, ui_);
    }
    ui_ = nullptr;
}
