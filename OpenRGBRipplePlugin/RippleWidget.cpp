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
#include <QHBoxLayout>
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
    hook_->Start();

    timer_ = new QTimer(this);
    timer_->setInterval(16);
    connect(timer_, &QTimer::timeout, this, &RippleWidget::OnTick);
    timer_->start();
}

RippleWidget::~RippleWidget()
{
    if(hook_)
    {
        hook_->Stop();
        delete hook_;
        hook_ = nullptr;
    }
    SaveSettings();
}

void RippleWidget::BuildUi()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(16, 16, 16, 16);
    root->setSpacing(12);

    auto* title = new QLabel("Ripple");
    QFont f = title->font();
    f.setPointSize(16);
    f.setBold(true);
    title->setFont(f);
    root->addWidget(title);

    auto* blurb = new QLabel(
        "Artemis-style key press wave. A ring expands from the LED that matches "
        "the physical key you press.");
    blurb->setWordWrap(true);
    root->addWidget(blurb);

    enable_box_ = new QCheckBox("Enable effect");
    enable_box_->setChecked(true);
    connect(enable_box_, &QCheckBox::toggled, this, &RippleWidget::SetEnabled);
    root->addWidget(enable_box_);

    auto* brush_row = new QHBoxLayout();
    brush_row->addWidget(new QLabel("Brush"));
    brush_box_ = new QComboBox();
    brush_box_->addItems({"Ring (Key Wave)", "Fill (Key Wave Filled)", "Soft (glow)"});
    connect(brush_box_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &RippleWidget::OnUiChanged);
    brush_row->addWidget(brush_box_, 1);
    root->addLayout(brush_row);

    auto* color_row = new QHBoxLayout();
    color_row->addWidget(new QLabel("Color"));
    color_box_ = new QComboBox();
    color_box_->addItems({"Rainbow", "Solid", "Random"});
    color_box_->setCurrentIndex(0);
    connect(color_box_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &RippleWidget::OnUiChanged);
    color_row->addWidget(color_box_, 1);
    auto* pick = new QPushButton("Pick");
    connect(pick, &QPushButton::clicked, this, &RippleWidget::OnPickColor);
    color_row->addWidget(pick);
    color_swatch_ = new QLabel("    ");
    color_swatch_->setAutoFillBackground(true);
    color_row->addWidget(color_swatch_);
    root->addLayout(color_row);

    auto* idle_row = new QHBoxLayout();
    idle_row->addWidget(new QLabel("Idle"));
    auto* idle_pick = new QPushButton("Background");
    connect(idle_pick, &QPushButton::clicked, this, &RippleWidget::OnPickIdle);
    idle_row->addWidget(idle_pick);
    idle_swatch_ = new QLabel("    ");
    idle_swatch_->setAutoFillBackground(true);
    idle_row->addWidget(idle_swatch_);
    idle_row->addWidget(new QLabel("Blend"));
    blend_box_ = new QComboBox();
    blend_box_->addItems({"Over (ripple on top)", "Add (glow)"});
    connect(blend_box_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &RippleWidget::OnUiChanged);
    idle_row->addWidget(blend_box_, 1);
    root->addLayout(idle_row);

    auto* grid = new QGridLayout();
    int row = 0;
    auto add_slider = [&](const char* name, QSlider*& slot, QLabel*& value,
                          int min, int max, int val)
    {
        grid->addWidget(new QLabel(name), row, 0);
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

    impact_box_ = new QCheckBox("Impact flash on the pressed key");
    impact_box_->setChecked(true);
    connect(impact_box_, &QCheckBox::toggled, this, &RippleWidget::OnUiChanged);
    root->addWidget(impact_box_);

    idle_box_ = new QCheckBox("Paint idle background");
    idle_box_->setChecked(true);
    idle_box_->setToolTip(
        "Off: leave keys the ripple is not touching alone, so an Effects "
        "plugin shader / pattern can show through.");
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
    QColor c = QColorDialog::getColor(start, this, "Ripple color");
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
    QPalette p = color_swatch_->palette();
    p.setColor(QPalette::Window, c);
    color_swatch_->setPalette(p);
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

void RippleWidget::OnPickIdle()
{
    RippleSettings s = engine_.GetSettings();
    QColor start(static_cast<int>(s.idle.r),
                 static_cast<int>(s.idle.g),
                 static_cast<int>(s.idle.b));
    QColor c = QColorDialog::getColor(start, this, "Idle / background color");
    if(!c.isValid())
    {
        return;
    }
    s.idle = {static_cast<float>(c.red()),
              static_cast<float>(c.green()),
              static_cast<float>(c.blue())};
    engine_.SetSettings(s);
    QPalette p = idle_swatch_->palette();
    p.setColor(QPalette::Window, c);
    idle_swatch_->setPalette(p);
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
    s.paint_idle = !idle_box_ || idle_box_->isChecked();
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
        idle_box_->setChecked(s.paint_idle);
    }
    if(blend_box_)
    {
        blend_box_->setCurrentIndex(s.blend == RippleBlend::Add ? 1 : 0);
    }
    QPalette p = color_swatch_->palette();
    p.setColor(QPalette::Window, QColor(static_cast<int>(s.solid.r),
                                        static_cast<int>(s.solid.g),
                                        static_cast<int>(s.solid.b)));
    color_swatch_->setPalette(p);
    if(idle_swatch_)
    {
        QPalette ip = idle_swatch_->palette();
        ip.setColor(QPalette::Window, QColor(static_cast<int>(s.idle.r),
                                             static_cast<int>(s.idle.g),
                                             static_cast<int>(s.idle.b)));
        idle_swatch_->setPalette(ip);
    }
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
    controller->SetCustomMode();
    controller->UpdateMode();
}

bool RippleWidget::DeviceSelected(RGBController* controller) const
{
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
    if(!rm_)
    {
        return;
    }

    QLayout* list = device_list_->layout();
    QLayoutItem* child;
    while((child = list->takeAt(0)) != nullptr)
    {
        delete child->widget();
        delete child;
    }
    devices_.clear();
    mapped_.clear();

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

    status_->setText(QString("Mapped %1 LEDs on %2 keyboard(s). Hook %3.")
                         .arg(mapped_.size())
                         .arg(devices_.size())
                         .arg(hook_ && hook_->IsRunning() ? "active" : "unavailable"));
}

void RippleWidget::ConsumeKeys()
{
    if(!hook_)
    {
        return;
    }
    const std::vector<KeyEvent> events = hook_->Drain();
    if(events.empty() || mapped_.empty())
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
            if(!DeviceSelected(led.controller))
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
                if(DeviceSelected(led.controller))
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
        if(!DeviceSelected(led.controller))
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
    ConsumeKeys();
    Paint();
    if(status_ && hook_)
    {
        status_->setText(QString("Mapped %1 LEDs · %2 waves · hook %3")
                             .arg(mapped_.size())
                             .arg(engine_.ActiveCount())
                             .arg(hook_->IsRunning() ? "active" : "off"));
    }
}
