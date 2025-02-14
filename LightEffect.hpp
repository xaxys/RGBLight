#ifndef __LIGHTEFFECT_HPP__
#define __LIGHTEFFECT_HPP__

#include <Arduino.h>
#include <LittleFS.h>
#include <FastLED.h>
#include <ArduinoJson.h>
#include <functional>

#include "Light.hpp"
#include "any.h"
#include "utils.h"

enum EffectType {
    CONSTANT,    // 常亮
    BLINK,       // 闪烁
    BREATH,      // 呼吸
    CHASE,       // 跑马灯
    RAINBOW,     // 彩虹
    STREAM,      // 流光
    ANIMATION,   // 动画
    MUSIC,       // 音乐律动
    CUSTOM,      // 上位机控制
    EFFECT_TYPE_COUNT
};

/**
 * @brief Get light effect enum from name
 * 
 * @param str effect name
 * @return EffectType effect enum
 */
EffectType str2effect(const char *str);

/**
 * @brief Get name from light effect enum
 * 
 * @param effect effect enum
 * @return const char* effect name
 */
const char* effect2str(EffectType effect);

extern const uint16_t &fps;

template <typename Light>
class Effect {
private:
    std::any _impl;
    std::function<EffectType()> _type;
    std::function<bool(Light &, uint32_t)> _update;
    std::function<void(JsonDocument &)> _writeToJSON;

public:
    Effect() noexcept {}

    template <typename T>
    Effect(T &&impl) : _impl(std::forward<T>(impl)) {
        _type = [this]() -> EffectType {
            return std::any_cast<T&>(_impl).type();
        };
        _update = [this](Light &light, uint32_t deltaTime) -> bool {
            return std::any_cast<T&>(_impl).update(light, deltaTime);
        };
        _writeToJSON = [this](JsonDocument &json) {
            std::any_cast<T&>(_impl).writeToJSON(json);
        };
    }

    template <typename T>
    Effect<Light>& operator=(T &&impl) {
        ::new(this) Effect<Light>(std::forward<T>(impl));
        return *this;
    }

    template <typename T>
    T& as() {
        return std::any_cast<T&>(_impl);
    }

    EffectType type() const {
        return _type();
    }

    bool update(Light &light, uint32_t deltaTime) {
        return _update(light, deltaTime);
    }

    void writeToJSON(JsonDocument &json) const {
        json["mode"] = type();
        _writeToJSON(json);
    }

    // defined at the end of the file
    static Effect<Light> readFromJSON(JsonDocument &json);
};

class ConstantEffect {
private:
    bool updated;
    CRGB currentColor;

public:
    ConstantEffect(uint32_t color) :
        updated(false), currentColor(color) {}

    EffectType type() const {
        return CONSTANT;
    }

    template <typename Light>
    bool update(Light &light, uint32_t deltaTime) {
        if (!updated) {
            fill_solid(light.data(), light.count(), currentColor);
            updated = true;
            return true;
        }
        return false;
    }

    void writeToJSON(JsonDocument &json) const {
        json["color"] = rgb2hex(currentColor.r, currentColor.g, currentColor.b);
    }

    static ConstantEffect readFromJSON(JsonDocument &json) {
        uint32_t color = json["color"];
        return ConstantEffect(color);
    }
};

class BlinkEffect {
private:
    uint16_t currentFrame;
    CRGB currentColor;
    float lastTime;
    float interval;

public:
    BlinkEffect(uint32_t color, float lastTime, float interval) :
        currentFrame(0), currentColor(color), lastTime(lastTime), interval(interval) {}

    EffectType type() const {
        return BLINK;
    }

    template <typename Light>
    bool update(Light &light, uint32_t deltaTime) {
        int lastTime = fps * this->lastTime;
        int interval = fps * this->interval;
        bool needUpdate = false;
        if (currentFrame == 0) {
            fill_solid(light.data(), light.count(), currentColor);
            needUpdate = true;
        } else if (currentFrame == lastTime) {
            fill_solid(light.data(), light.count(), CRGB::Black);
            needUpdate = true;
        }
        if (++currentFrame >= lastTime + interval) {
            currentFrame = 0;
        }
        return needUpdate;
    }

    void writeToJSON(JsonDocument &json) const {
        json["color"] = rgb2hex(currentColor.r, currentColor.g, currentColor.b);
        json["lastTime"] = lastTime;
        json["interval"] = interval;
    }

    static BlinkEffect readFromJSON(JsonDocument &json) {
        uint32_t color = json["color"];
        float lastTime = json["lastTime"];
        float interval = json["interval"];
        return BlinkEffect(color, lastTime, interval);
    }
};

class BreathEffect {
private:
    uint16_t currentFrame;
    CRGB currentColor;
    float lastTime;
    float interval;

public:
    BreathEffect(uint32_t color, float lastTime, float interval) :
        currentFrame(0), currentColor(color), lastTime(lastTime), interval(interval) {}

    EffectType type() const {
        return BREATH;
    }

    template <typename Light>
    bool update(Light &light, uint32_t deltaTime) {
        int lastTime = fps * this->lastTime;
        int interval = fps * this->interval;
        bool needUpdate = false;
        if (currentFrame <= lastTime) {
            CRGB rgb = currentColor;
            double x = (double) currentFrame / lastTime;
            int scale = -1010 * x * x + 1010 * x;
            rgb.nscale8(scale);
            fill_solid(light.data(), light.count(), rgb);
            needUpdate = true;
        }
        if (++currentFrame >= lastTime + interval) {
            currentFrame = 0;
        }
        return needUpdate;
    }

    void writeToJSON(JsonDocument &json) const {
        json["color"] = rgb2hex(currentColor.r, currentColor.g, currentColor.b);
        json["lastTime"] = lastTime;
        json["interval"] = interval;
    }

    static BreathEffect readFromJSON(JsonDocument &json) {
        uint32_t color = json["color"];
        float lastTime = json["lastTime"];
        float interval = json["interval"];
        return BreathEffect(color, lastTime, interval);
    }
};

class ChaseEffect {
private:
    uint16_t currentFrame;
    CRGB currentColor;
    uint8_t direction;
    float lastTime;

public:
    ChaseEffect(uint32_t color, uint8_t direction, float lastTime) :
        currentFrame(0), currentColor(color), direction(direction), lastTime(lastTime) {}

    EffectType type() const {
        return CHASE;
    }

    template <int COUNT, bool REVERSE>
    bool update(LightStrip<COUNT, REVERSE> &light, uint32_t deltaTime) {
        int lastTime = fps * this->lastTime;
        bool needUpdate = false;
        if (currentFrame % lastTime == 0) {
            fill_solid(light.data(), light.count(), CRGB::Black);
            int index = currentFrame / lastTime;
            if (index > light.l() * 2 - 1) {
                currentFrame = 0;
                index = 0;
            } else if (index > light.l() - 1) {
                index = light.l() * 2 - 1 - index;
            }
            light.at(index) = currentColor;
            needUpdate = true;
        }
        ++currentFrame;
        return needUpdate;
    }

    template <int ARRANGEMENT, int... COUNT_PER_RING>
    bool update(LightDisc<ARRANGEMENT, COUNT_PER_RING...> &light, uint32_t deltaTime) {
        int lastTime = fps * this->lastTime;
        bool needUpdate = false;
        if (currentFrame % lastTime == 0) {
            fill_solid(light.data(), light.count(), CRGB::Black);
            int index = currentFrame / lastTime;
            if (index > light.r() * 2 - 1) {
                currentFrame = 0;
                index = 0;
            } else if (index > light.r() - 1) {
                index = light.r() * 2 - 1 - index;
            }
            for (int j = 0; j < light.l(index); j++) {
                light.at(index, j) = currentColor;
            }
            needUpdate = true;
        }
        ++currentFrame;
        return needUpdate;
    }

    void writeToJSON(JsonDocument &json) const {
        json["color"] = rgb2hex(currentColor.r, currentColor.g, currentColor.b);
        json["direction"] = direction;
        json["lastTime"] = lastTime;
    }

    static ChaseEffect readFromJSON(JsonDocument &json) {
        uint32_t color = json["color"];
        uint8_t direction = json["direction"];
        float lastTime = json["lastTime"];
        return ChaseEffect(color, direction, lastTime);
    }
};

class RainbowEffect {
private:
    uint8_t currentHue;
    int8_t delta;

public:
    RainbowEffect(int8_t delta) :
        currentHue(0), delta(delta) {}

    EffectType type() const {
        return RAINBOW;
    }

    template <typename Light>
    bool update(Light &light, uint32_t deltaTime) {
        CHSV hsv(currentHue, 255, 240);
        CRGB rgb;
        hsv2rgb_rainbow(hsv, rgb);
        fill_solid(light.data(), light.count(), rgb);
        currentHue += delta;
        return true;
    }

    void writeToJSON(JsonDocument &json) const {
        json["delta"] = delta;
    }

    static RainbowEffect readFromJSON(JsonDocument &json) {
        uint8_t delta = json["delta"];
        return RainbowEffect(delta);
    }
};

class StreamEffect {
private:
    uint8_t currentHue;
    uint8_t direction;
    int8_t delta;

public:
    StreamEffect(uint8_t direction, int8_t delta) :
        currentHue(0), direction(direction), delta(delta) {}

    EffectType type() const {
        return STREAM;
    }

    template <int COUNT, bool REVERSE>
    bool update(LightStrip<COUNT, REVERSE> &light, uint32_t deltaTime) {
        fill_rainbow(light.data(), light.count(), currentHue);
        currentHue += delta;
        return true;
    }

    template <int ARRANGEMENT, int... COUNT_PER_RING>
    bool update(LightDisc<ARRANGEMENT, COUNT_PER_RING...> &light, uint32_t deltaTime) {
        CRGB rgb[light.r()];
        fill_rainbow(rgb, light.r(), currentHue);
        for (int i = 0; i < light.r(); i++) {
            for (int j = 0; j < light.l(i); j++) {
                light.at(i, j) = rgb[i];
            }
        }
        currentHue += delta;
        return true;
    }

    void writeToJSON(JsonDocument &json) const {
        json["direction"] = direction;
        json["delta"] = delta;
    }

    static StreamEffect readFromJSON(JsonDocument &json) {
        uint8_t direction = json["direction"];
        uint8_t delta = json["delta"];
        return StreamEffect(direction, delta);
    }
};

class AnimationEffect {
private:
    String animName;
    File file;
    uint16_t currentFrame;

public:
    AnimationEffect(const char *animName) :
        animName(animName), currentFrame(0) {
        if (strlen(animName) > 0) {
            String path = String("/animations/") + animName;
            file = LittleFS.open(path, "r");
            if (!file.isFile()) {
                file.close();
            }
        }
        if (file) {
            Serial.print(F("Start to play animation: "));
        } else {
            Serial.print(F("Failed to open animation: "));
        }
        Serial.println(animName);
    }

    ~AnimationEffect() {
        if (file) {
            file.close();
            Serial.println(F("Stop playing animation"));
        }
    }

    EffectType type() const {
        return ANIMATION;
    }

    template <typename Light>
    bool update(Light &light, uint32_t deltaTime) {
        if (!file) {
            return false;
        }
#ifdef ENABLE_DEBUG
        Serial.printf_P(PSTR("Playing anim frame: %d\n"), currentFrame);
#endif
        char buffer[8] = "";
        int bufLen = 0;
        int index = 0;
        while (true) {
            char c;
            // read a byte
            if (file.read((uint8_t *) &c, 1) != 1) {
                Serial.println(F("End of animation, replay"));
                file.seek(0);
                currentFrame = 0;
                break;
            }
            // parse data
            if (c == ',' || c == '\n') {
                buffer[bufLen] = '\0';
                if (buffer[0] == '#' && bufLen == 7) {
                    buffer[bufLen] = '\0';
                    light.data()[index++] = str2hex(buffer);
                    bufLen = 0;
                } else {
                    Serial.printf_P(PSTR("Invalid anim element: %s\n"), buffer);
                }
                if (c == '\n') {
                    currentFrame++;
                    break;
                }
            } else if (c == '\r') {
                continue;
            } else {
                if (bufLen < 7) {
                    buffer[bufLen++] = c;
                }
            }
        }
        return true;
    }

    void writeToJSON(JsonDocument &json) const {
        json["animName"] = animName;
    }

    static AnimationEffect readFromJSON(JsonDocument &json) {
        const char *animName = json["animName"];
        return AnimationEffect(animName);
    }
};

class MusicEffect {
private:
    uint8_t soundMode; // 0-电平模式 1-频谱模式
    uint8_t currentHue;
    double currentVolume; // Must be 0~1

public:
    MusicEffect(uint8_t mode) :
        soundMode(mode), currentHue(0), currentVolume(0.0) {}

    void setVolume(double volume) {
        currentVolume = volume;
    }

    EffectType type() const {
        return MUSIC;
    }

    template <int COUNT, bool REVERSE>
    bool update(LightStrip<COUNT, REVERSE> &light, uint32_t deltaTime) {
        if (soundMode == 0) {
            int count = light.l() * currentVolume;
            fill_solid(light.data(), light.count(), CRGB::Black);
            if (count > 0) {
                fill_solid(light.data(), count - 1, CRGB::Green);
                light.at(count - 1) = CRGB::Red;
            }
        } else {
            int count = light.l() * currentVolume;
            CHSV hsv(currentHue++, 255, 240);
            CRGB rgb;
            hsv2rgb_rainbow(hsv, rgb);
            fill_solid(light.data(), light.count(), CRGB::Black);
            fill_solid(light.data() + (light.count() - count) / 2, count, rgb);
        }
        return true;
    }

    template <int ARRANGEMENT, int... COUNT_PER_RING>
    bool update(LightDisc<ARRANGEMENT, COUNT_PER_RING...> &light, uint32_t deltaTime) {
        if (soundMode == 0) {
            int r = light.r() * currentVolume;
            fill_solid(light.data(), light.count(), CRGB::Black);
            if (r > 0) {
                for (int i = 0; i < r; i++) {
                    for (int j = 0; j < light.l(i); j++) {
                        light.at(i, j) = CRGB::Green;
                    }
                }
                int l = light.l(r - 1);
                for (int j = 0; j < l; j++) {
                    light.at(r - 1, j) = CRGB::Red;
                }
            }
        } else {
            int r = ceil(light.r() * currentVolume);
            CHSV hsv(currentHue++, 255, 240);
            CRGB rgb;
            hsv2rgb_rainbow(hsv, rgb);
            fill_solid(light.data(), light.count(), CRGB::Black);
            for (int i = light.r() - r; i < light.r(); i++) {
                for (int j = 0; j < light.l(i); j++) {
                    light.at(i, j) = rgb;
                }
            }
        }
        return true;
    }

    void writeToJSON(JsonDocument &json) const {
        json["soundMode"] = soundMode;
    }

    static MusicEffect readFromJSON(JsonDocument &json) {
        uint8_t soundMode = json["soundMode"];
        return MusicEffect(soundMode);
    }
};

class CustomEffect {
private:
    int index;

public:
    CustomEffect() : index(0) {}

    int& getIndex() {
        return index;
    }

    EffectType type() const {
        return CUSTOM;
    }

    template <typename Light>
    bool update(Light &light, uint32_t deltaTime) {
        return true;
    }

    void writeToJSON(JsonDocument &json) const {
    }

    static CustomEffect readFromJSON(JsonDocument &json) {
        return CustomEffect();
    }
};

template <typename Light>
Effect<Light> Effect<Light>::readFromJSON(JsonDocument &json) {
    if (json.containsKey("mode")) {
        EffectType mode = json["mode"].as<EffectType>();
        switch (mode) {
            case CONSTANT:
                return ConstantEffect::readFromJSON(json);
            case BLINK:
                return BlinkEffect::readFromJSON(json);
            case BREATH:
                return BreathEffect::readFromJSON(json);
            case CHASE:
                return ChaseEffect::readFromJSON(json);
            case RAINBOW:
                return RainbowEffect::readFromJSON(json);
            case STREAM:
                return StreamEffect::readFromJSON(json);
            case ANIMATION:
                return AnimationEffect::readFromJSON(json);
            case MUSIC:
                return MusicEffect::readFromJSON(json);
            case CUSTOM:
                return CustomEffect::readFromJSON(json);
        }
    }
    return ConstantEffect(DEFAULT_COLOR); // 默认为常亮
}

#endif // __LIGHTEFFECT_HPP__
