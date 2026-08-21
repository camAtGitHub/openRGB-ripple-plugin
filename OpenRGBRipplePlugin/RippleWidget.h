/*---------------------------------------------------------*\
| RippleWidget.h                                            |
\*---------------------------------------------------------*/

#pragma once

#include <QWidget>
#include "RippleEngine.h"

class QCheckBox;
class QComboBox;
class QLabel;
class QPushButton;
class QSlider;
class QTimer;
class ResourceManagerInterface;
class DeviceSession;

class RippleWidget : public QWidget
{
    Q_OBJECT

public:
    explicit RippleWidget(ResourceManagerInterface* rm, DeviceSession* session, QWidget* parent = nullptr);
    ~RippleWidget() override;

    void ApplySettings(const RippleSettings& s);
    RippleSettings CurrentSettings() const { return engine_.GetSettings(); }

    void PausePainting();
    void ResumePainting();
    void BindDevicesFromSession();
    void StartRuntime();
    void Shutdown();

public slots:
    void SetEnabled(bool on);

private slots:
    void OnUiChanged();
    void OnTick();
    void OnPickColor();
    void OnPickIdle();
    void FlushSettings();

private:
    void BuildUi();
    void LoadSettings();
    void SaveSettings();
    void SyncUiFromSettings(const RippleSettings& s);
    void ConsumeKeys();
    void Paint();
    void UpdateSliderLabels();
    void ShowShapeExtras();
    void SetColorButton(QPushButton* btn, const RippleRGB& c);

    ResourceManagerInterface* rm_ = nullptr;
    DeviceSession*            session_ = nullptr;
    RippleEngine              engine_;
    class KeyboardHook*       hook_ = nullptr;

    QCheckBox*  enable_box_   = nullptr;
    QComboBox*  brush_box_    = nullptr;
    QComboBox*  shape_box_    = nullptr;
    QComboBox*  color_box_    = nullptr;
    QComboBox*  blend_box_    = nullptr;
    QSlider*    speed_        = nullptr;
    QSlider*    thickness_    = nullptr;
    QSlider*    lifetime_     = nullptr;
    QSlider*    fade_         = nullptr;
    QSlider*    echoes_       = nullptr;
    QSlider*    brightness_   = nullptr;
    QSlider*    axis_jitter_  = nullptr;
    QSlider*    sweep_span_   = nullptr;
    QSlider*    trail_length_ = nullptr;
    QSlider*    blast_size_   = nullptr;
    QComboBox*  blast_shape_box_ = nullptr;
    QLabel*     speed_val_    = nullptr;
    QLabel*     thickness_val_= nullptr;
    QLabel*     lifetime_val_ = nullptr;
    QLabel*     fade_val_     = nullptr;
    QLabel*     echoes_val_   = nullptr;
    QLabel*     brightness_val_ = nullptr;
    QLabel*     jitter_lbl_   = nullptr;
    QLabel*     jitter_val_   = nullptr;
    QLabel*     span_lbl_     = nullptr;
    QLabel*     span_val_     = nullptr;
    QLabel*     trail_lbl_    = nullptr;
    QLabel*     trail_val_    = nullptr;
    QLabel*     blast_shape_lbl_ = nullptr;
    QLabel*     blast_size_lbl_  = nullptr;
    QLabel*     blast_size_val_  = nullptr;
    QCheckBox*  impact_box_   = nullptr;
    QCheckBox*  idle_box_     = nullptr;
    QLabel*     status_       = nullptr;
    QPushButton* color_btn_   = nullptr;
    QPushButton* idle_btn_    = nullptr;
    QWidget*    device_list_  = nullptr;

    uint32_t               seed_ = 1;
    bool                   have_last_ = false;
    float                  last_x_ = 0;
    float                  last_y_ = 0;
    double                 lastPressAt_ = 0;
    bool                   pending_blast_ = false;
    bool                   suppress_ui_ = false;
    QTimer*                timer_ = nullptr;
    QTimer*                save_timer_ = nullptr;
};
