/*---------------------------------------------------------*\
| OpenRGBRipplePlugin.h                                     |
|                                                           |
|   OpenRGB plugin — Artemis-style key-press ripple         |
|   Target: Plugin API 5 (OpenRGB 1.1)                      |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#pragma once

#include "OpenRGBPluginInterface.h"
#include "DeviceSession.h"

#include <QObject>
#include <QPointer>
#include <atomic>
#include <cstdint>
#include <vector>

class RippleWidget;
class RGBControllerInterface;

class OpenRGBRipplePlugin : public QObject, public OpenRGBPluginInterface
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID OpenRGBPluginInterface_IID FILE "OpenRGBRipplePlugin.json")
    Q_INTERFACES(OpenRGBPluginInterface)

public:
    ~OpenRGBRipplePlugin() override = default;

    OpenRGBPluginInfo GetPluginInfo() override;
    unsigned int      GetPluginAPIVersion() override;

    void     Load(OpenRGBPluginAPIInterface* plugin_api_ptr) override;
    QWidget* GetWidget() override;
    QMenu*   GetTrayMenu() override;
    void     Unload() override;

    void           OnProfileAboutToLoad() override {}
    void           OnProfileLoad(nlohmann::json) override {}
    nlohmann::json OnProfileSave() override { return nlohmann::json(); }
    unsigned char* OnSDKCommand(unsigned int, unsigned char*, unsigned int*) override { return nullptr; }

    void ProfileManagerUpdated(unsigned int) override {}
    void ResourceManagerUpdated(unsigned int update_reason) override;
    void SettingsManagerUpdated(unsigned int) override {}

public slots:
    void OnDetectionStart();
    void OnDeviceListChanged();
    void OnDetectionEnd();

private:
    void DetectionStarted();
    void DeviceListChanged();
    void DetectionEnded();

    std::vector<RGBControllerInterface*> CopyControllers() const;

    OpenRGBPluginAPIInterface* api_ = nullptr;
    DeviceSession             session_;
    QPointer<RippleWidget>    ui_;
    std::atomic<bool>         alive_{false};
    std::atomic<bool>         paused_{false};
    std::atomic<uint64_t>     suspend_epoch_{0};
    std::atomic<uint64_t>     queued_end_epoch_{0};
};
