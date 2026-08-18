/*---------------------------------------------------------*\
| OpenRGBRipplePlugin.h                                     |
|                                                           |
|   OpenRGB plugin — Artemis-style key-press ripple         |
|   Target: Plugin API 4 (OpenRGB 1.0rc2 / 1.0rc3)          |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#pragma once

#include "OpenRGBPluginInterface.h"
#include "DeviceSession.h"

#include <QObject>
#include <QPointer>
#include <atomic>
#include <vector>

class RippleWidget;
class RGBController;

class OpenRGBRipplePlugin : public QObject, public OpenRGBPluginInterface
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID OpenRGBPluginInterface_IID FILE "OpenRGBRipplePlugin.json")
    Q_INTERFACES(OpenRGBPluginInterface)

public:
    ~OpenRGBRipplePlugin() override = default;

    OpenRGBPluginInfo GetPluginInfo() override;
    unsigned int      GetPluginAPIVersion() override;

    void     Load(ResourceManagerInterface* resource_manager_ptr) override;
    QWidget* GetWidget() override;
    QMenu*   GetTrayMenu() override;
    void     Unload() override;

public slots:
    void OnDetectionStart();
    void OnDeviceListChanged();
    void OnDetectionEnd();

private:
    static void DetectionStartCallback(void* arg);
    static void DeviceListChangedCallback(void* arg);
    static void DetectionEndCallback(void* arg);

    void RegisterHostCallbacks();
    void UnregisterHostCallbacks();
    std::vector<RGBController*> CopyControllers() const;

    ResourceManagerInterface* rm_ = nullptr;
    DeviceSession             session_;
    QPointer<RippleWidget>    ui_;
    std::atomic<bool>         alive_{false};
    bool                      callbacks_registered_ = false;
};
