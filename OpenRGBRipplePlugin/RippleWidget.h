/*---------------------------------------------------------*\
| RippleWidget.h                                            |
\*---------------------------------------------------------*/

#pragma once

#include <QWidget>
#include <vector>
#include "RippleEngine.h"

class QCheckBox;
class QComboBox;
class QLabel;
class QSlider;
class ResourceManagerInterface;
class RGBController;

class RippleWidget : public QWidget
{
    Q_OBJECT

public:
    explicit RippleWidget(ResourceManagerInterface* rm, QWidget* parent = nullptr);
    ~RippleWidget() override;

    void RebuildDevices();
    void ApplySettings(const RippleSettings& s);
    RippleSettings CurrentSettings() const { return engine_.GetSettings(); }

public slots:
    void SetEnabled(bool on);
    void RebuildDevicesSlot() { RebuildDevices(); }

private slots:
    void OnUiChanged();
    void OnTick();
    void OnPickColor();
    void OnPickIdle();

private:
    struct MappedLed
    {
        RGBController* controller = nullptr;
        unsigned int   led_index  = 0;
        float          x          = 0;
        float          y          = 0;
    };

    struct DeviceOpt
    {
        RGBController* controller = nullptr;
        QCheckBox*     box        = nullptr;
    };

    void BuildUi();
    void LoadSettings();
    void SaveSettings();
    void SyncUiFromSettings(const RippleSettings& s);
    void ConsumeKeys();
    void Paint();
    void UpdateSliderLabels();
    void EnsureDirectMode(RGBController* controller);
    bool DeviceSelected(RGBController* controller) const;

    ResourceManagerInterface* rm_ = nullptr;
    RippleEngine              engine_;
    class KeyboardHook*       hook_ = nullptr;

    QCheckBox*  enable_box_   = nullptr;
    QComboBox*  brush_box_    = nullptr;
    QComboBox*  color_box_    = nullptr;
    QComboBox*  blend_box_    = nullptr;
    QSlider*    speed_        = nullptr;
    QSlider*    thickness_    = nullptr;
    QSlider*    lifetime_     = nullptr;
    QSlider*    fade_         = nullptr;
    QSlider*    echoes_       = nullptr;
    QSlider*    brightness_   = nullptr;
    QLabel*     speed_val_    = nullptr;
    QLabel*     thickness_val_= nullptr;
    QLabel*     lifetime_val_ = nullptr;
    QLabel*     fade_val_     = nullptr;
    QLabel*     echoes_val_   = nullptr;
    QLabel*     brightness_val_ = nullptr;
    QCheckBox*  impact_box_   = nullptr;
    QCheckBox*  idle_box_     = nullptr;
    QLabel*     status_       = nullptr;
    QLabel*     color_swatch_ = nullptr;
    QLabel*     idle_swatch_  = nullptr;
    QWidget*    device_list_  = nullptr;

    std::vector<DeviceOpt> devices_;
    std::vector<MappedLed> mapped_;
    uint32_t               seed_ = 1;
    bool                   suppress_ui_ = false;
    class QTimer*          timer_ = nullptr;
};
