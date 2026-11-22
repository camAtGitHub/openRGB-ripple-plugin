/*---------------------------------------------------------*\
| RippleWidget.cpp                                          |
\*---------------------------------------------------------*/

#include "RippleWidget.h"
#include "DeviceSession.h"
#include "KeyboardHook.h"
#include "KeyMap.h"
#include "RippleSettingsIO.h"
#include "ResourceManagerInterface.h"
#include "RGBController.h"
#include "SettingsManager.h"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QShowEvent>
#include <QSlider>
#include <QSpinBox>
#include <QTimer>
#include <QVBoxLayout>
#include <algorithm>
#include <chrono>
#include <unordered_set>

static double NowSeconds()
{
    using clock = std::chrono::steady_clock;
    static const auto t0 = clock::now();
    return std::chrono::duration<double>(clock::now() - t0).count();
}

static unsigned int ClampByte(float v)
{
    if(v < 0.0f) return 0;
    if(v > 255.0f) return 255;
    return static_cast<unsigned int>(v);
}

static unsigned int ToRgb(const RippleRGB& c)
{
    return ToRGBColor(ClampByte(c.r), ClampByte(c.g), ClampByte(c.b));
}

/* Effects plugin tray actions (same lookup the Scheduler plugin used).
   Returns false when Effects is not loaded, so callers can skip. */
static const char* const EFFECTS_START_ALL = "OpenRGBEffectsPlugin::Action::StartAll";
static const char* const EFFECTS_STOP_ALL  = "OpenRGBEffectsPlugin::Action::StopAll";

static QAction* FindEffectsAction(const char* object_name)
{
    for(QWidget* w : QApplication::topLevelWidgets())
    {
        if(QAction* action = w->findChild<QAction*>(object_name))
        {
            return action;
        }
    }
    return nullptr;
}

static bool TriggerEffectsAction(const char* object_name)
{
    QAction* action = FindEffectsAction(object_name);
    if(action)
    {
        action->trigger();
    }
    return action != nullptr;
}

static QSlider* MakeSlider(int min, int max, int value)
{
    QSlider* s = new QSlider(Qt::Horizontal);
    s->setRange(min, max);
    s->setValue(value);
    return s;
}

RippleWidget::RippleWidget(ResourceManagerInterface* rm, DeviceSession* session, QWidget* parent)
    : QWidget(parent), rm_(rm), session_(session)
{
    hook_ = new KeyboardHook();
    BuildUi();
    LoadSettings();
    /* Do NOT RebuildDevices / PushDirectMode / hook Start / timer start here. */
    timer_ = new QTimer(this);
    timer_->setInterval(16);
    connect(timer_, &QTimer::timeout, this, &RippleWidget::OnTick);
    save_timer_ = new QTimer(this);
    save_timer_->setSingleShot(true);
    save_timer_->setInterval(400);
    connect(save_timer_, &QTimer::timeout, this, &RippleWidget::FlushSettings);
    /* Gap between Stop All Effects and going dark, so Effects' last
       frame cannot land on top of the black write. */
    lights_off_timer_ = new QTimer(this);
    lights_off_timer_->setSingleShot(true);
    lights_off_timer_->setInterval(500);
    connect(lights_off_timer_, &QTimer::timeout, this, &RippleWidget::PaintLightsOff);
}

RippleWidget::~RippleWidget()
{
    Shutdown();
    if(save_timer_)
    {
        save_timer_->stop();
    }
    if(hook_)
    {
        delete hook_;
        hook_ = nullptr;
    }
    FlushSettings();
}

void RippleWidget::PausePainting()
{
    if(timer_)
    {
        timer_->stop();
    }
    if(status_)
    {
        status_->setText("Paused while OpenRGB rescans devices…");
    }
}

void RippleWidget::ResumePainting()
{
    if(timer_ && session_ && session_->IsLive())
    {
        timer_->start();
    }
}

void RippleWidget::StartRuntime()
{
    last_key_at_ = NowSeconds();
    if(hook_)
    {
        hook_->Start();
    }
    if(timer_ && session_ && session_->IsLive())
    {
        timer_->start();
    }
}

void RippleWidget::Shutdown()
{
    if(timer_)
    {
        timer_->stop();
    }
    if(save_timer_)
    {
        save_timer_->stop();
    }
    if(lights_off_timer_)
    {
        lights_off_timer_->stop();
    }
    if(hook_)
    {
        hook_->Stop();
    }
    session_ = nullptr;
}

void RippleWidget::BuildUi()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(16, 16, 16, 16);
    root->setSpacing(12);

    auto* title = new QLabel("Ripple 1.1.0");
    QFont f = title->font();
    f.setPointSize(16);
    f.setBold(true);
    title->setFont(f);
    root->addWidget(title);

    auto* blurb = new QLabel("A wave expands from each key you press.");
    blurb->setWordWrap(true);
    root->addWidget(blurb);

    enable_box_ = new QCheckBox("Enabled");
    enable_box_->setChecked(true);
    connect(enable_box_, &QCheckBox::toggled, this, &RippleWidget::SetEnabled);
    root->addWidget(enable_box_);

    auto* grid = new QGridLayout();
    grid->setHorizontalSpacing(10);
    grid->setVerticalSpacing(8);
    int row = 0;

    auto add_label = [&](const char* name) -> QLabel*
    {
        auto* l = new QLabel(name);
        l->setMinimumWidth(80);
        grid->addWidget(l, row, 0);
        return l;
    };

    add_label("Brush");
    brush_box_ = new QComboBox();
    brush_box_->addItems({"Ring", "Fill", "Soft"});
    connect(brush_box_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &RippleWidget::OnUiChanged);
    grid->addWidget(brush_box_, row, 1, 1, 2);
    row++;

    add_label("Shape");
    shape_box_ = new QComboBox();
    shape_box_->addItems({"Circle", "Square", "Row/Col", "Sweep", "Dart"});
    connect(shape_box_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &RippleWidget::OnUiChanged);
    grid->addWidget(shape_box_, row, 1, 1, 2);
    row++;

    add_label("Color");
    color_box_ = new QComboBox();
    color_box_->addItems({"Rainbow", "Solid", "Random"});
    color_box_->setCurrentIndex(0);
    connect(color_box_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &RippleWidget::OnUiChanged);
    grid->addWidget(color_box_, row, 1, 1, 2);
    row++;

    QLabel* wave_lbl = add_label("Wave");
    color_btn_ = new QPushButton();
    color_btn_->setCursor(Qt::PointingHandCursor);
    color_btn_->setAutoDefault(false);
    color_btn_->setAccessibleName("Wave color");
    color_btn_->setToolTip("Color of the wave when Color is Solid. Click to change.");
    connect(color_btn_, &QPushButton::clicked, this, &RippleWidget::OnPickColor);
    wave_lbl->setBuddy(color_btn_);
    grid->addWidget(color_btn_, row, 1, 1, 2);
    row++;

    QLabel* idle_lbl = add_label("Background");
    idle_btn_ = new QPushButton();
    idle_btn_->setCursor(Qt::PointingHandCursor);
    idle_btn_->setAutoDefault(false);
    idle_btn_->setAccessibleName("Background color");
    idle_btn_->setToolTip("Color of keys the wave is not on. Click to change.");
    connect(idle_btn_, &QPushButton::clicked, this, &RippleWidget::OnPickIdle);
    idle_lbl->setBuddy(idle_btn_);
    grid->addWidget(idle_btn_, row, 1, 1, 2);
    row++;

    add_label("Blend");
    blend_box_ = new QComboBox();
    blend_box_->addItems({
        "Over", "Add", "XOR", "Screen", "Overlay",
        "Color Dodge", "Color Burn", "Exclusion"
    });
    blend_box_->setToolTip(
        "Over: wave hue on top. XOR: complementary (|wave-idle|). "
        "Add / Screen / Color Dodge all lighten (look similar on bright waves). "
        "Overlay lights. Color Burn punches a dark hole. Exclusion is a soft invert.");
    connect(blend_box_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &RippleWidget::OnUiChanged);
    grid->addWidget(blend_box_, row, 1, 1, 2);
    row++;

    jitter_lbl_ = add_label("Jitter");
    axis_jitter_ = MakeSlider(0, 100, 18);
    connect(axis_jitter_, &QSlider::valueChanged, this, &RippleWidget::OnUiChanged);
    grid->addWidget(axis_jitter_, row, 1);
    jitter_val_ = new QLabel();
    jitter_val_->setMinimumWidth(48);
    jitter_val_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    grid->addWidget(jitter_val_, row, 2);
    row++;

    span_lbl_ = add_label("Span");
    sweep_span_ = MakeSlider(0, 100, 100);
    connect(sweep_span_, &QSlider::valueChanged, this, &RippleWidget::OnUiChanged);
    grid->addWidget(sweep_span_, row, 1);
    span_val_ = new QLabel();
    span_val_->setMinimumWidth(48);
    span_val_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    grid->addWidget(span_val_, row, 2);
    row++;

    trail_lbl_ = add_label("Trail");
    trail_length_ = MakeSlider(0, 160, 25);
    connect(trail_length_, &QSlider::valueChanged, this, &RippleWidget::OnUiChanged);
    grid->addWidget(trail_length_, row, 1);
    trail_val_ = new QLabel();
    trail_val_->setMinimumWidth(48);
    trail_val_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    grid->addWidget(trail_val_, row, 2);
    row++;

    blast_shape_lbl_ = add_label("Explosion");
    blast_shape_box_ = new QComboBox();
    blast_shape_box_->addItems({"Circle", "Square"});
    connect(blast_shape_box_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &RippleWidget::OnUiChanged);
    grid->addWidget(blast_shape_box_, row, 1, 1, 2);
    row++;

    blast_size_lbl_ = add_label("Blast size");
    blast_size_ = MakeSlider(5, 120, 35);
    connect(blast_size_, &QSlider::valueChanged, this, &RippleWidget::OnUiChanged);
    grid->addWidget(blast_size_, row, 1);
    blast_size_val_ = new QLabel();
    blast_size_val_->setMinimumWidth(48);
    blast_size_val_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    grid->addWidget(blast_size_val_, row, 2);
    row++;

    auto add_slider = [&](const char* name, QSlider*& slot, QLabel*& value,
                          int min, int max, int val)
    {
        add_label(name);
        slot = MakeSlider(min, max, val);
        connect(slot, &QSlider::valueChanged, this, &RippleWidget::OnUiChanged);
        grid->addWidget(slot, row, 1);
        value = new QLabel();
        value->setMinimumWidth(48);
        value->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        grid->addWidget(value, row, 2);
        row++;
    };
    add_slider("Speed", speed_, speed_val_, 40, 500, 140);
    add_slider("Thickness", thickness_, thickness_val_, 35, 300, 115);
    add_slider("Lifetime", lifetime_, lifetime_val_, 5, 280, 115);
    lifetime_->setToolTip(
        "Max expand time. Sweep / Row-Col travel at Speed and may finish sooner. "
        "Dart: idle after the last key, then the boom expands for this long.");
    add_slider("Retract", fade_, fade_val_, 0, 300, 100);
    fade_->setToolTip(
        "Seconds to pull the front back to the key. 0 snaps off. "
        "Dart: how long the boom shrinks back.");
    add_slider("Echoes", echoes_, echoes_val_, 0, 4, 1);
    add_slider("Brightness", brightness_, brightness_val_, 15, 100, 100);
    root->addLayout(grid);
    UpdateSliderLabels();
    ShowShapeExtras();
    SetColorButton(color_btn_, engine_.GetSettings().solid);
    SetColorButton(idle_btn_, engine_.GetSettings().idle);

    impact_box_ = new QCheckBox("Flash pressed key");
    impact_box_->setChecked(true);
    connect(impact_box_, &QCheckBox::toggled, this, &RippleWidget::OnUiChanged);
    root->addWidget(impact_box_);

    idle_box_ = new QCheckBox("Disable background");
    idle_box_->setChecked(false);
    idle_box_->setToolTip(
        "Leave keys the wave is not touching alone, so another "
        "effect can show through.");
    connect(idle_box_, &QCheckBox::toggled, this, &RippleWidget::OnUiChanged);
    root->addWidget(idle_box_);

    auto* timeouts_box = new QGroupBox("Idle");
    auto* timeouts = new QGridLayout(timeouts_box);
    timeouts->setHorizontalSpacing(10);
    auto make_minutes = [&](int row_i, const char* name, int max, const char* tip,
                            QSpinBox*& spin, QLabel*& l)
    {
        l = new QLabel(name);
        l->setMinimumWidth(80);
        timeouts->addWidget(l, row_i, 0);
        spin = new QSpinBox();
        spin->setRange(0, max);
        spin->setSpecialValueText("Disabled");
        spin->setSuffix(" min");
        spin->setKeyboardTracking(false); /* no clamping mid-typing */
        spin->setToolTip(tip);
        l->setBuddy(spin);
        connect(spin, QOverload<int>::of(&QSpinBox::valueChanged),
                this, &RippleWidget::OnUiChanged);
        timeouts->addWidget(spin, row_i, 1);
    };
    QLabel* lights_off_lbl = nullptr;
    make_minutes(0, "Idle timeout", 60,
        "Minutes without a keypress before Ripple hands over to the Effects "
        "plugin (Start All Effects). A keypress stops Effects and brings "
        "Ripple back.",
        idle_timeout_, idle_timeout_lbl_);
    make_minutes(1, "Lights off", 120,
        "Minutes without a keypress before keyboards go dark (stops all "
        "effects first). Must be longer than Idle timeout; setting it at or "
        "below Idle timeout turns it off.",
        lights_off_, lights_off_lbl);
    /* Hidden until showEvent finds the Effects plugin. */
    idle_timeout_lbl_->setVisible(false);
    idle_timeout_->setVisible(false);
    root->addWidget(timeouts_box);

    auto* devices_box = new QGroupBox("Keyboards");
    auto* devices_lay = new QVBoxLayout(devices_box);
    device_list_ = new QWidget();
    device_list_->setLayout(new QVBoxLayout());
    static_cast<QVBoxLayout*>(device_list_->layout())->setContentsMargins(0, 0, 0, 0);
    devices_lay->addWidget(device_list_);
    root->addWidget(devices_box);

    status_ = new QLabel();
    status_->setWordWrap(true);
    root->addWidget(status_);
    root->addStretch(1);
}

/* Effects may load after Ripple, so check each time the tab is shown. */
void RippleWidget::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    const bool have_effects = FindEffectsAction(EFFECTS_START_ALL) != nullptr;
    if(idle_timeout_lbl_) idle_timeout_lbl_->setVisible(have_effects);
    if(idle_timeout_) idle_timeout_->setVisible(have_effects);
    if(have_effects != effects_present_)
    {
        effects_present_ = have_effects;
        OnUiChanged(); /* re-apply the Lights off > Idle timeout rule */
    }
}

void RippleWidget::SetEnabled(bool on)
{
    RippleSettings s = engine_.GetSettings();
    s.enabled = on;
    engine_.SetSettings(s);
    if(enable_box_ && enable_box_->isChecked() != on)
    {
        enable_box_->blockSignals(true);
        enable_box_->setChecked(on);
        enable_box_->blockSignals(false);
    }
    SaveSettings();
}

void RippleWidget::OnPickColor()
{
    RippleSettings s = engine_.GetSettings();
    QColor start(static_cast<int>(s.solid.r),
                 static_cast<int>(s.solid.g),
                 static_cast<int>(s.solid.b));
    QColor c = QColorDialog::getColor(start, this, "Wave color");
    if(!c.isValid())
    {
        return;
    }
    s.solid = {static_cast<float>(c.red()),
               static_cast<float>(c.green()),
               static_cast<float>(c.blue())};
    s.color_mode = RippleColorMode::Solid;
    engine_.SetSettings(s);
    suppress_ui_ = true;
    color_box_->setCurrentIndex(1);
    suppress_ui_ = false;
    SetColorButton(color_btn_, s.solid);
    SaveSettings();
}

void RippleWidget::UpdateSliderLabels()
{
    if(!speed_val_) return;
    speed_val_->setText(QString::number(speed_->value() / 10.0, 'f', 1));
    thickness_val_->setText(QString::number(thickness_->value() / 100.0, 'f', 2));
    lifetime_val_->setText(QString::number(lifetime_->value() / 100.0, 'f', 2) + "s");
    fade_val_->setText(QString::number(fade_->value() / 100.0, 'f', 2) + "s");
    echoes_val_->setText(QString::number(echoes_->value()));
    brightness_val_->setText(QString::number(brightness_->value()) + "%");
    if(jitter_val_ && axis_jitter_)
    {
        jitter_val_->setText(QString::number(axis_jitter_->value()) + "%");
    }
    if(span_val_ && sweep_span_)
    {
        span_val_->setText(QString::number(sweep_span_->value()) + "%");
    }
    if(trail_val_ && trail_length_)
    {
        trail_val_->setText(QString::number(trail_length_->value() / 10.0, 'f', 1));
    }
    if(blast_size_val_ && blast_size_)
    {
        blast_size_val_->setText(QString::number(blast_size_->value() / 10.0, 'f', 1));
    }
}

void RippleWidget::ShowShapeExtras()
{
    const int shape = shape_box_ ? shape_box_->currentIndex() : 0;
    const bool axis = shape == static_cast<int>(RippleShape::Axis)
                   || shape == static_cast<int>(RippleShape::Sweep);
    const bool sweep = shape == static_cast<int>(RippleShape::Sweep);
    const bool jump = shape == static_cast<int>(RippleShape::Jump);
    if(jitter_lbl_) jitter_lbl_->setVisible(axis);
    if(axis_jitter_) axis_jitter_->setVisible(axis);
    if(jitter_val_) jitter_val_->setVisible(axis);
    if(span_lbl_) span_lbl_->setVisible(sweep);
    if(sweep_span_) sweep_span_->setVisible(sweep);
    if(span_val_) span_val_->setVisible(sweep);
    if(trail_lbl_) trail_lbl_->setVisible(jump);
    if(trail_length_) trail_length_->setVisible(jump);
    if(trail_val_) trail_val_->setVisible(jump);
    if(blast_shape_lbl_) blast_shape_lbl_->setVisible(jump);
    if(blast_shape_box_) blast_shape_box_->setVisible(jump);
    if(blast_size_lbl_) blast_size_lbl_->setVisible(jump);
    if(blast_size_) blast_size_->setVisible(jump);
    if(blast_size_val_) blast_size_val_->setVisible(jump);
}

void RippleWidget::SetColorButton(QPushButton* btn, const RippleRGB& c)
{
    if(!btn)
    {
        return;
    }
    const QColor qc(static_cast<int>(c.r),
                    static_cast<int>(c.g),
                    static_cast<int>(c.b));
    const QString fg = qc.lightness() > 140 ? QString("#1a1a1a") : QString("#f2f2f2");
    btn->setText(qc.name().toUpper());
    btn->setStyleSheet(
        QString("QPushButton { background-color: %1; color: %2; }")
            .arg(qc.name(), fg));
    btn->setToolTip(qc.name().toUpper() + " — click to change");
}

void RippleWidget::OnPickIdle()
{
    RippleSettings s = engine_.GetSettings();
    QColor start(static_cast<int>(s.idle.r),
                 static_cast<int>(s.idle.g),
                 static_cast<int>(s.idle.b));
    QColor c = QColorDialog::getColor(start, this, "Background color");
    if(!c.isValid())
    {
        return;
    }
    s.idle = {static_cast<float>(c.red()),
              static_cast<float>(c.green()),
              static_cast<float>(c.blue())};
    engine_.SetSettings(s);
    SetColorButton(idle_btn_, s.idle);
    SaveSettings();
}

void RippleWidget::OnUiChanged()
{
    if(suppress_ui_)
    {
        return;
    }
    UpdateSliderLabels();
    ShowShapeExtras();
    RippleSettings s = engine_.GetSettings();
    s.brush      = static_cast<RippleBrush>(brush_box_->currentIndex());
    s.shape      = static_cast<RippleShape>(shape_box_->currentIndex());
    s.color_mode = color_box_->currentIndex() == 1 ? RippleColorMode::Solid
                 : color_box_->currentIndex() == 2 ? RippleColorMode::Random
                 : RippleColorMode::Rainbow;
    s.speed      = speed_->value() / 10.0f;
    s.thickness  = thickness_->value() / 100.0f;
    s.lifetime   = lifetime_->value() / 100.0f;
    s.fade       = fade_->value() / 100.0f;
    s.echo_count = echoes_->value();
    s.brightness = brightness_->value() / 100.0f;
    s.axis_jitter = axis_jitter_->value() / 100.0f;
    s.sweep_span  = sweep_span_->value() / 100.0f;
    if(trail_length_)
    {
        s.trail_length = trail_length_->value() / 10.0f;
    }
    if(blast_size_)
    {
        s.blast_size = blast_size_->value() / 10.0f;
    }
    if(blast_shape_box_)
    {
        s.blast_shape = blast_shape_box_->currentIndex() == 1
            ? RippleBlastShape::Square
            : RippleBlastShape::Circle;
    }
    s.impact_flash = impact_box_->isChecked();
    s.blend      = static_cast<RippleBlend>(blend_box_->currentIndex());
    s.paint_idle = !idle_box_ || !idle_box_->isChecked();
    s.enabled    = enable_box_->isChecked();
    if(idle_timeout_ && lights_off_)
    {
        s.idle_timeout_min = idle_timeout_->value();
        s.lights_off_min   = lights_off_->value();
        /* Stepping Lights off down to or past Idle timeout turns it off;
           raising Idle timeout past Lights off pushes Lights off up. */
        if(effects_present_ && sender() == lights_off_ && s.idle_timeout_min > 0
           && s.lights_off_min > 0 && s.lights_off_min <= s.idle_timeout_min)
        {
            s.lights_off_min = 0;
        }
        ClampIdleTimeouts(s, effects_present_);
        if(lights_off_->value() != s.lights_off_min)
        {
            suppress_ui_ = true;
            lights_off_->setValue(s.lights_off_min);
            suppress_ui_ = false;
        }
    }
    engine_.SetSettings(s);
    SaveSettings();
}

void RippleWidget::SyncUiFromSettings(const RippleSettings& s)
{
    suppress_ui_ = true;
    enable_box_->setChecked(s.enabled);
    brush_box_->setCurrentIndex(static_cast<int>(s.brush));
    {
        int si = static_cast<int>(s.shape);
        if(si < 0 || si > 4) si = 0;
        shape_box_->setCurrentIndex(si);
    }
    color_box_->setCurrentIndex(s.color_mode == RippleColorMode::Solid ? 1
                              : s.color_mode == RippleColorMode::Random ? 2 : 0);
    speed_->setValue(static_cast<int>(s.speed * 10));
    thickness_->setValue(static_cast<int>(s.thickness * 100));
    lifetime_->setValue(static_cast<int>(s.lifetime * 100));
    fade_->setValue(static_cast<int>(s.fade * 100));
    echoes_->setValue(s.echo_count);
    brightness_->setValue(static_cast<int>(s.brightness * 100));
    if(axis_jitter_)
    {
        axis_jitter_->setValue(static_cast<int>(s.axis_jitter * 100));
    }
    if(sweep_span_)
    {
        sweep_span_->setValue(static_cast<int>(s.sweep_span * 100));
    }
    if(trail_length_)
    {
        trail_length_->setValue(static_cast<int>(s.trail_length * 10));
    }
    if(blast_size_)
    {
        blast_size_->setValue(static_cast<int>(s.blast_size * 10));
    }
    if(blast_shape_box_)
    {
        blast_shape_box_->setCurrentIndex(
            s.blast_shape == RippleBlastShape::Square ? 1 : 0);
    }
    impact_box_->setChecked(s.impact_flash);
    if(idle_box_)
    {
        idle_box_->setChecked(!s.paint_idle);
    }
    if(blend_box_)
    {
        blend_box_->setCurrentIndex(static_cast<int>(s.blend));
    }
    if(idle_timeout_)
    {
        idle_timeout_->setValue(s.idle_timeout_min);
    }
    if(lights_off_)
    {
        lights_off_->setValue(s.lights_off_min);
    }
    SetColorButton(color_btn_, s.solid);
    SetColorButton(idle_btn_, s.idle);
    UpdateSliderLabels();
    ShowShapeExtras();
    suppress_ui_ = false;
}

void RippleWidget::ApplySettings(const RippleSettings& s)
{
    engine_.SetSettings(s);
    SyncUiFromSettings(s);
}

void RippleWidget::LoadSettings()
{
    if(!rm_ || !rm_->GetSettingsManager())
    {
        return;
    }
    try
    {
        json j = rm_->GetSettingsManager()->GetSettings("OpenRGBRipplePlugin");
        engine_.SetSettings(SettingsFromJson(j, engine_.GetSettings()));
    }
    catch(...)
    {
        /* keep engine defaults */
    }
    SyncUiFromSettings(engine_.GetSettings());
}

void RippleWidget::SaveSettings()
{
    if(save_timer_)
    {
        save_timer_->start(); /* restart 400 ms */
    }
}

void RippleWidget::FlushSettings()
{
    if(!rm_ || !rm_->GetSettingsManager())
    {
        return;
    }
    try
    {
        json j = SettingsToJson(engine_.GetSettings());
        rm_->GetSettingsManager()->SetSettings("OpenRGBRipplePlugin", j);
        rm_->GetSettingsManager()->SaveSettings();
    }
    catch(...)
    {
    }
}

void RippleWidget::BindDevicesFromSession()
{
    if(!device_list_)
    {
        return;
    }

    QLayout* list = device_list_->layout();
    if(list)
    {
        QLayoutItem* child;
        while((child = list->takeAt(0)) != nullptr)
        {
            delete child->widget();
            delete child;
        }
    }

    if(!session_ || !list)
    {
        return;
    }

    const std::vector<DeviceSession::DeviceOpt> devices = session_->Devices();
    for(const DeviceSession::DeviceOpt& d : devices)
    {
        auto* box = new QCheckBox(QString::fromStdString(d.name));
        box->setChecked(d.selected);
        connect(box, &QCheckBox::toggled, this, [this, box](bool on)
        {
            if(session_)
            {
                session_->SetSelectedByName(box->text().toStdString(), on);
            }
        });
        list->addWidget(box);
    }

    if(status_)
    {
        status_->setText(QString("Mapped %1 LEDs on %2 keyboard(s). Hook %3.")
                             .arg(session_->Mapped().size())
                             .arg(devices.size())
                             .arg(hook_ && hook_->IsRunning() ? "active" : "unavailable"));
    }
}

/* Returns true when at least one key was pressed since the last call. */
bool RippleWidget::ConsumeKeys()
{
    if(!hook_ || !session_)
    {
        return false;
    }
    const std::vector<KeyEvent> events = hook_->Drain();
    if(events.empty())
    {
        return false;
    }

    session_->WithLive([&](const auto&, const auto& mapped)
    {
        if(mapped.empty())
        {
            return;
        }

        const double now = NowSeconds();
        bool have_bounds = false;
        LayoutBounds bounds;
        for(const auto& led : mapped)
        {
            if(!led.controller || !session_->DeviceSelected(led.controller))
            {
                continue;
            }
            if(!have_bounds)
            {
                bounds.minX = bounds.maxX = led.x;
                bounds.minY = bounds.maxY = led.y;
                have_bounds = true;
            }
            else
            {
                bounds.minX = std::min(bounds.minX, led.x);
                bounds.maxX = std::max(bounds.maxX, led.x);
                bounds.minY = std::min(bounds.minY, led.y);
                bounds.maxY = std::max(bounds.maxY, led.y);
            }
        }
        if(!have_bounds)
        {
            return;
        }

        const RippleSettings s = engine_.GetSettings();
        const bool jump = s.shape == RippleShape::Jump;
        auto spawn_at = [&](float x, float y)
        {
            pending_blast_ = jump;
            if(jump)
            {
                engine_.DropBlasts();
            }
            engine_.Spawn(x, y, now, seed_++, bounds, have_last_, last_x_, last_y_);
            last_x_ = x;
            last_y_ = y;
            lastPressAt_ = now;
            have_last_ = true;
        };

        for(const KeyEvent& ev : events)
        {
            const std::vector<std::string> names =
                KeyMap::NamesForVirtualKey(ev.vk, ev.scan, ev.extended);
            if(names.empty())
            {
                continue;
            }

            bool spawned = false;
            for(const auto& led : mapped)
            {
                if(!led.controller || !session_->DeviceSelected(led.controller))
                {
                    continue;
                }
                if(led.led_index >= led.controller->leds.size())
                {
                    continue;
                }
                const std::string& led_name = led.controller->leds[led.led_index].name;
                if(KeyMap::NameMatches(led_name, names))
                {
                    spawn_at(led.x, led.y);
                    spawned = true;
                    break;
                }
            }
            if(!spawned)
            {
                /* Fallback: spawn from the first selected keyboard origin. */
                for(const auto& led : mapped)
                {
                    if(led.controller && session_->DeviceSelected(led.controller))
                    {
                        spawn_at(led.x, led.y);
                        break;
                    }
                }
            }
        }
    });
    return true;
}

static RippleRGB FromRgb(RGBColor c)
{
    return {
        static_cast<float>(RGBGetRValue(c)),
        static_cast<float>(RGBGetGValue(c)),
        static_cast<float>(RGBGetBValue(c))
    };
}

void RippleWidget::Paint()
{
    const double now = NowSeconds();
    engine_.Prune(now);
    const RippleSettings s = engine_.GetSettings();
    if(!s.enabled || suspended_)
    {
        return;
    }

    /* Passthrough: do not touch the device unless a wave is live, so an
       Effects / GL shader plugin keeps ownership of idle keys. */
    if(!s.paint_idle && engine_.ActiveCount() == 0)
    {
        return;
    }

    if(!session_)
    {
        return;
    }
    session_->WithLive([&](const auto& devices, const auto& mapped)
    {
        std::unordered_set<RGBController*> dirty;
        if(s.paint_idle)
        {
            for(const auto& d : devices)
            {
                if(!d.controller || !session_->DeviceSelected(d.controller))
                {
                    continue;
                }
                const unsigned int idle = ToRgb(s.idle);
                for(RGBColor& c : d.controller->colors)
                {
                    c = idle;
                }
                dirty.insert(d.controller);
            }
        }

        for(const auto& led : mapped)
        {
            if(!led.controller || !session_->DeviceSelected(led.controller))
            {
                continue;
            }
            if(led.led_index >= led.controller->colors.size())
            {
                continue;
            }
            const RippleRGB base = s.paint_idle
                ? s.idle
                : FromRgb(led.controller->colors[led.led_index]);
            const RippleRGB c = engine_.SampleOver(led.x, led.y, now, base);
            led.controller->colors[led.led_index] = ToRgb(c);
            dirty.insert(led.controller);
        }
        /* DeviceUpdateLEDs writes HID on this thread. UpdateLEDs() only
           sets a flag for DeviceCallThread; Cleanup() destroys the
           derived controller (closes HID) before that worker is joined. */
        for(RGBController* controller : dirty)
        {
            controller->DeviceUpdateLEDs();
        }
    });
}

void RippleWidget::CheckIdle(double now)
{
    const RippleSettings s = engine_.GetSettings();
    const double idle_seconds = now - last_key_at_;

    if(!idle_fired_ && s.idle_timeout_min > 0
       && idle_seconds >= s.idle_timeout_min * 60.0)
    {
        idle_fired_ = true;
        /* Ripple only steps aside when Effects is there to take over. */
        if(TriggerEffectsAction(EFFECTS_START_ALL))
        {
            effects_started_ = true;
            suspended_ = true;
        }
    }

    if(!lights_off_fired_ && s.lights_off_min > 0
       && idle_seconds >= s.lights_off_min * 60.0)
    {
        lights_off_fired_ = true;
        suspended_ = true;
        TriggerEffectsAction(EFFECTS_STOP_ALL);
        effects_started_ = false;
        lights_off_timer_->start();
    }
}

void RippleWidget::WakeFromIdle()
{
    lights_off_timer_->stop();
    if(effects_started_)
    {
        TriggerEffectsAction(EFFECTS_STOP_ALL);
        effects_started_ = false;
    }
    idle_fired_ = false;
    lights_off_fired_ = false;
    suspended_ = false;
}

void RippleWidget::PaintLightsOff()
{
    if(!session_ || !lights_off_fired_)
    {
        return;
    }
    session_->WithLive([&](const auto& devices, const auto&)
    {
        for(const auto& d : devices)
        {
            if(!d.controller)
            {
                continue;
            }
            for(RGBColor& c : d.controller->colors)
            {
                c = ToRGBColor(0, 0, 0);
            }
            d.controller->DeviceUpdateLEDs();
        }
    });
}

void RippleWidget::OnTick()
{
    if(!session_ || !session_->IsLive())
    {
        return;
    }
    const double tick_now = NowSeconds();
    if(ConsumeKeys())
    {
        last_key_at_ = tick_now;
        if(idle_fired_ || lights_off_fired_)
        {
            WakeFromIdle();
        }
    }
    CheckIdle(tick_now);
    {
        const RippleSettings s = engine_.GetSettings();
        const double now = NowSeconds();
        if(s.shape == RippleShape::Jump && pending_blast_ && have_last_
           && now - lastPressAt_ >= static_cast<double>(s.lifetime))
        {
            Ripple lastDart;
            const bool haveDart = engine_.LastNonBlast(lastDart);
            engine_.KeepOnlyBlasts();
            engine_.SpawnBlast(last_x_, last_y_, now, seed_++,
                               haveDart ? &lastDart : nullptr);
            pending_blast_ = false;
        }
    }
    Paint();
    if(!status_ || !hook_)
    {
        return;
    }
    size_t mapped = 0;
    bool live = false;
    session_->WithLive([&](const auto&, const auto& mapped_leds)
    {
        live = true;
        mapped = mapped_leds.size();
    });
    if(!live)
    {
        return;
    }
    const QString idle_state = lights_off_fired_ ? " · lights off"
                             : suspended_        ? " · idle (Effects running)"
                             : "";
    status_->setText(QString("Mapped %1 LEDs · %2 waves · hook %3%4")
                         .arg(mapped)
                         .arg(engine_.ActiveCount())
                         .arg(hook_->IsRunning() ? "active" : "off")
                         .arg(idle_state));
}
