/*---------------------------------------------------------*\
| RippleWidget.cpp                                          |
\*---------------------------------------------------------*/

#include "RippleWidget.h"
#include "KeyboardHook.h"
#include "KeyMap.h"
#include "ResourceManagerInterface.h"
#include "RGBController.h"
#include "SettingsManager.h"

#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
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

static QSlider* MakeSlider(int min, int max, int value)
{
    QSlider* s = new QSlider(Qt::Horizontal);
    s->setRange(min, max);
    s->setValue(value);
    return s;
}

RippleWidget::RippleWidget(ResourceManagerInterface* rm, QWidget* parent)
    : QWidget(parent)
    , rm_(rm)
{
    hook_ = new KeyboardHook();
    BuildUi();
    LoadSettings();
    RebuildDevices();
    PushDirectMode();
    hook_->Start();

    timer_ = new QTimer(this);
    timer_->setInterval(16);
    connect(timer_, &QTimer::timeout, this, &RippleWidget::OnTick);
    timer_->start();
}

RippleWidget::~RippleWidget()
{
    Shutdown();
    if(hook_)
    {
        delete hook_;
        hook_ = nullptr;
    }
    SaveSettings();
}

void RippleWidget::InvalidateDevices()
{
    std::lock_guard<std::mutex> lock(device_mutex_);
    devices_live_ = false;
    mapped_.clear();
    for(DeviceOpt& d : devices_)
    {
        d.controller = nullptr;
    }
}

void RippleWidget::SuspendForDetection()
{
    if(timer_)
    {
        timer_->stop();
    }
    InvalidateDevices();
    if(status_)
    {
        status_->setText("Paused while OpenRGB rescans devices…");
    }
}

void RippleWidget::ResumeAfterDetection()
{
    RebuildDevices();
    PushDirectMode();
    if(timer_)
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
    if(hook_)
    {
        hook_->Stop();
    }
    InvalidateDevices();
}

void RippleWidget::BuildUi()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(16, 16, 16, 16);
    root->setSpacing(12);

    auto* title = new QLabel("Ripple 1.0.2");
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
    blend_box_->addItems({"Over", "Add"});
    blend_box_->setToolTip("Over replaces the background color. Add stacks the wave on top.");
    connect(blend_box_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &RippleWidget::OnUiChanged);
    grid->addWidget(blend_box_, row, 1, 1, 2);
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
    add_slider("Speed", speed_, speed_val_, 40, 280, 140);
    add_slider("Thickness", thickness_, thickness_val_, 35, 300, 115);
    add_slider("Lifetime", lifetime_, lifetime_val_, 35, 280, 115);
    add_slider("Fade", fade_, fade_val_, 60, 300, 135);
    add_slider("Echoes", echoes_, echoes_val_, 0, 4, 1);
    add_slider("Brightness", brightness_, brightness_val_, 15, 100, 100);
    root->addLayout(grid);
    UpdateSliderLabels();
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
    fade_val_->setText(QString::number(fade_->value() / 100.0, 'f', 2));
    echoes_val_->setText(QString::number(echoes_->value()));
    brightness_val_->setText(QString::number(brightness_->value()) + "%");
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
    RippleSettings s = engine_.GetSettings();
    s.brush      = static_cast<RippleBrush>(brush_box_->currentIndex());
    s.color_mode = color_box_->currentIndex() == 1 ? RippleColorMode::Solid
                 : color_box_->currentIndex() == 2 ? RippleColorMode::Random
                 : RippleColorMode::Rainbow;
    s.speed      = speed_->value() / 10.0f;
    s.thickness  = thickness_->value() / 100.0f;
    s.lifetime   = lifetime_->value() / 100.0f;
    s.fade_power = fade_->value() / 100.0f;
    s.echo_count = echoes_->value();
    s.brightness = brightness_->value() / 100.0f;
    s.impact_flash = impact_box_->isChecked();
    s.blend      = blend_box_->currentIndex() == 1 ? RippleBlend::Add : RippleBlend::Max;
    s.paint_idle = !idle_box_ || !idle_box_->isChecked();
    s.enabled    = enable_box_->isChecked();
    engine_.SetSettings(s);
    SaveSettings();
}

void RippleWidget::SyncUiFromSettings(const RippleSettings& s)
{
    suppress_ui_ = true;
    enable_box_->setChecked(s.enabled);
    brush_box_->setCurrentIndex(static_cast<int>(s.brush));
    color_box_->setCurrentIndex(s.color_mode == RippleColorMode::Solid ? 1
                              : s.color_mode == RippleColorMode::Random ? 2 : 0);
    speed_->setValue(static_cast<int>(s.speed * 10));
    thickness_->setValue(static_cast<int>(s.thickness * 100));
    lifetime_->setValue(static_cast<int>(s.lifetime * 100));
    fade_->setValue(static_cast<int>(s.fade_power * 100));
    echoes_->setValue(s.echo_count);
    brightness_->setValue(static_cast<int>(s.brightness * 100));
    impact_box_->setChecked(s.impact_flash);
    if(idle_box_)
    {
        idle_box_->setChecked(!s.paint_idle);
    }
    if(blend_box_)
    {
        blend_box_->setCurrentIndex(s.blend == RippleBlend::Add ? 1 : 0);
    }
    SetColorButton(color_btn_, s.solid);
    SetColorButton(idle_btn_, s.idle);
    UpdateSliderLabels();
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
    json j = rm_->GetSettingsManager()->GetSettings("OpenRGBRipplePlugin");
    RippleSettings s = engine_.GetSettings();
    if(j.contains("brush"))      s.brush = static_cast<RippleBrush>(j["brush"].get<int>());
    if(j.contains("speed"))      s.speed = j["speed"].get<float>();
    if(j.contains("thickness"))  s.thickness = j["thickness"].get<float>();
    if(j.contains("lifetime"))   s.lifetime = j["lifetime"].get<float>();
    if(j.contains("fade_power")) s.fade_power = j["fade_power"].get<float>();
    if(j.contains("echo_count")) s.echo_count = j["echo_count"].get<int>();
    if(j.contains("echo_delay")) s.echo_delay = j["echo_delay"].get<float>();
    if(j.contains("brightness")) s.brightness = j["brightness"].get<float>();
    if(j.contains("color_mode")) s.color_mode = static_cast<RippleColorMode>(j["color_mode"].get<int>());
    if(j.contains("impact_flash")) s.impact_flash = j["impact_flash"].get<bool>();
    if(j.contains("enabled"))    s.enabled = j["enabled"].get<bool>();
    if(j.contains("solid_r"))    s.solid.r = j["solid_r"].get<float>();
    if(j.contains("solid_g"))    s.solid.g = j["solid_g"].get<float>();
    if(j.contains("solid_b"))    s.solid.b = j["solid_b"].get<float>();
    if(j.contains("idle_r"))     s.idle.r = j["idle_r"].get<float>();
    if(j.contains("idle_g"))     s.idle.g = j["idle_g"].get<float>();
    if(j.contains("idle_b"))     s.idle.b = j["idle_b"].get<float>();
    if(j.contains("blend"))      s.blend = static_cast<RippleBlend>(j["blend"].get<int>());
    if(j.contains("paint_idle")) s.paint_idle = j["paint_idle"].get<bool>();
    engine_.SetSettings(s);
    SyncUiFromSettings(s);
}

void RippleWidget::SaveSettings()
{
    if(!rm_ || !rm_->GetSettingsManager())
    {
        return;
    }
    const RippleSettings s = engine_.GetSettings();
    json j;
    j["brush"]        = static_cast<int>(s.brush);
    j["speed"]        = s.speed;
    j["thickness"]    = s.thickness;
    j["lifetime"]     = s.lifetime;
    j["fade_power"]   = s.fade_power;
    j["echo_count"]   = s.echo_count;
    j["echo_delay"]   = s.echo_delay;
    j["brightness"]   = s.brightness;
    j["color_mode"]   = static_cast<int>(s.color_mode);
    j["impact_flash"] = s.impact_flash;
    j["enabled"]      = s.enabled;
    j["solid_r"]      = s.solid.r;
    j["solid_g"]      = s.solid.g;
    j["solid_b"]      = s.solid.b;
    j["idle_r"]       = s.idle.r;
    j["idle_g"]       = s.idle.g;
    j["idle_b"]       = s.idle.b;
    j["blend"]        = static_cast<int>(s.blend);
    j["paint_idle"]   = s.paint_idle;
    rm_->GetSettingsManager()->SetSettings("OpenRGBRipplePlugin", j);
    rm_->GetSettingsManager()->SaveSettings();
}

void RippleWidget::EnsureDirectMode(RGBController* controller)
{
    if(!controller)
    {
        return;
    }
    /* SetCustomMode only picks the Direct/Custom mode index.
       Do not call UpdateMode() here — that queues a USB write on the
       controller's worker thread and races with Rescan's destructor. */
    controller->SetCustomMode();
}

void RippleWidget::PushDirectMode()
{
    std::lock_guard<std::mutex> lock(device_mutex_);
    if(!devices_live_)
    {
        return;
    }
    for(const DeviceOpt& d : devices_)
    {
        if(d.controller)
        {
            d.controller->UpdateMode();
        }
    }
}

bool RippleWidget::DeviceSelected(RGBController* controller) const
{
    if(!controller)
    {
        return false;
    }
    bool any = false;
    for(const DeviceOpt& d : devices_)
    {
        if(d.box && d.box->isChecked())
        {
            any = true;
            if(d.controller == controller)
            {
                return true;
            }
        }
    }
    return !any;
}

void RippleWidget::RebuildDevices()
{
    if(!rm_ || !device_list_)
    {
        return;
    }

    const bool ready = rm_->GetDetectionPercent() >= 100;

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

    std::lock_guard<std::mutex> lock(device_mutex_);
    devices_.clear();
    mapped_.clear();
    devices_live_ = false;

    if(!ready)
    {
        if(status_)
        {
            status_->setText("Waiting for device detection…");
        }
        return;
    }

    std::vector<RGBController*>& controllers = rm_->GetRGBControllers();
    for(RGBController* controller : controllers)
    {
        if(!controller)
        {
            continue;
        }
        const bool is_kb =
            controller->type == DEVICE_TYPE_KEYBOARD ||
            controller->type == DEVICE_TYPE_KEYPAD ||
            controller->type == DEVICE_TYPE_LAPTOP;
        if(!is_kb)
        {
            continue;
        }

        auto* box = new QCheckBox(QString::fromStdString(controller->name));
        box->setChecked(true);
        list->addWidget(box);
        devices_.push_back({controller, box});
        EnsureDirectMode(controller);

        for(size_t z = 0; z < controller->zones.size(); z++)
        {
            zone& zn = controller->zones[z];
            if(zn.type == ZONE_TYPE_MATRIX && zn.matrix_map && zn.matrix_map->map)
            {
                const unsigned int h = zn.matrix_map->height;
                const unsigned int w = zn.matrix_map->width;
                for(unsigned int y = 0; y < h; y++)
                {
                    for(unsigned int x = 0; x < w; x++)
                    {
                        const unsigned int idx = zn.matrix_map->map[y * w + x];
                        if(idx == 0xFFFFFFFFu)
                        {
                            continue;
                        }
                        MappedLed led;
                        led.controller = controller;
                        led.led_index  = zn.start_idx + idx;
                        led.x          = static_cast<float>(x);
                        led.y          = static_cast<float>(y);
                        mapped_.push_back(led);
                    }
                }
            }
            else if(zn.type == ZONE_TYPE_LINEAR || zn.type == ZONE_TYPE_SINGLE)
            {
                for(unsigned int i = 0; i < zn.leds_count; i++)
                {
                    MappedLed led;
                    led.controller = controller;
                    led.led_index  = zn.start_idx + i;
                    led.x          = static_cast<float>(i);
                    led.y          = 0.0f;
                    mapped_.push_back(led);
                }
            }
        }
    }

    devices_live_ = true;
    if(status_)
    {
        status_->setText(QString("Mapped %1 LEDs on %2 keyboard(s). Hook %3.")
                             .arg(mapped_.size())
                             .arg(devices_.size())
                             .arg(hook_ && hook_->IsRunning() ? "active" : "unavailable"));
    }
}

void RippleWidget::ConsumeKeys()
{
    if(!hook_)
    {
        return;
    }
    const std::vector<KeyEvent> events = hook_->Drain();
    std::lock_guard<std::mutex> lock(device_mutex_);
    if(!devices_live_ || events.empty() || mapped_.empty())
    {
        return;
    }

    const double now = NowSeconds();
    for(const KeyEvent& ev : events)
    {
        const std::vector<std::string> names =
            KeyMap::NamesForVirtualKey(ev.vk, ev.scan, ev.extended);
        if(names.empty())
        {
            continue;
        }

        bool spawned = false;
        for(const MappedLed& led : mapped_)
        {
            if(!led.controller || !DeviceSelected(led.controller))
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
                engine_.Spawn(led.x, led.y, now, seed_++);
                spawned = true;
                break;
            }
        }
        if(!spawned)
        {
            /* Fallback: spawn from the first selected keyboard origin. */
            for(const MappedLed& led : mapped_)
            {
                if(led.controller && DeviceSelected(led.controller))
                {
                    engine_.Spawn(led.x, led.y, now, seed_++);
                    break;
                }
            }
        }
    }
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
    if(!s.enabled)
    {
        return;
    }

    /* Passthrough: do not touch the device unless a wave is live, so an
       Effects / GL shader plugin keeps ownership of idle keys. */
    if(!s.paint_idle && engine_.ActiveCount() == 0)
    {
        return;
    }

    std::lock_guard<std::mutex> lock(device_mutex_);
    if(!devices_live_)
    {
        return;
    }

    std::unordered_set<RGBController*> dirty;
    if(s.paint_idle)
    {
        for(const DeviceOpt& d : devices_)
        {
            if(!d.controller || !DeviceSelected(d.controller))
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

    for(const MappedLed& led : mapped_)
    {
        if(!led.controller || !DeviceSelected(led.controller))
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
    for(RGBController* controller : dirty)
    {
        controller->UpdateLEDs();
    }
}

void RippleWidget::OnTick()
{
    if(!devices_live_.load())
    {
        return;
    }
    ConsumeKeys();
    Paint();
    if(!status_ || !hook_)
    {
        return;
    }
    size_t mapped = 0;
    bool live = false;
    {
        std::lock_guard<std::mutex> lock(device_mutex_);
        live = devices_live_;
        mapped = mapped_.size();
    }
    if(!live)
    {
        return;
    }
    status_->setText(QString("Mapped %1 LEDs · %2 waves · hook %3")
                         .arg(mapped)
                         .arg(engine_.ActiveCount())
                         .arg(hook_->IsRunning() ? "active" : "off"));
}
