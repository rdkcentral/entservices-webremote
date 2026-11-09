/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2026 RDK Management
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "WebRemote.h"

#include <interfaces/ITools.h>
#include <interfaces/IVoiceControl.h>

#include <cmath>
#include <cstdlib>
#include <vector>

#define API_VERSION_NUMBER_MAJOR 1
#define API_VERSION_NUMBER_MINOR 0
#define API_VERSION_NUMBER_PATCH 0

namespace WPEFramework {

namespace {

    static Plugin::Metadata<Plugin::WebRemote> metadata(
        // Version (Major, Minor, Patch)
        API_VERSION_NUMBER_MAJOR, API_VERSION_NUMBER_MINOR, API_VERSION_NUMBER_PATCH,
        // Preconditions
        {},
        // Terminations
        {},
        // Controls
        {});

    const char LINCHPIN_CALLSIGN[] = "org.rdk.LinchpinClientProxy";
    const char VOICECONTROL_CALLSIGN[] = "org.rdk.VoiceControl";
    const char KEY_TARGET_CALLSIGN[] = "org.rdk.Tools";

    const char LINCHPIN_CLIENT_ID[] = "WebRemote";
    const char TOPIC_PREFIX[] = "X";
    const char METHOD_GENERATE_KEY[] = "generateKey";
    const char DATA_TYPE_KEY_PRESS[] = "keypress";
    const char DATA_TYPE_TEXT[] = "text";
    const char VOICE_NAME[] = "webremote";

    // Tools takes whole seconds, so fractional values are rounded.
    uint32_t ToSeconds(const double value)
    {
        return (value > 0) ? static_cast<uint32_t>(std::lround(value)) : 0;
    }

} // namespace

namespace Plugin {

    SERVICE_REGISTRATION(WebRemote, API_VERSION_NUMBER_MAJOR, API_VERSION_NUMBER_MINOR, API_VERSION_NUMBER_PATCH);

    WebRemote::WebRemote()
        : _service(nullptr)
        , _linchpin(nullptr)
        , _topic()
        , _adminLock()
        , _notification(this)
    {
        SYSLOG(Logging::Startup, (_T("WebRemote Constructor")));
    }

    WebRemote::~WebRemote()
    {
        SYSLOG(Logging::Shutdown, (string(_T("WebRemote Destructor"))));
    }

    const string WebRemote::Initialize(PluginHost::IShell* service)
    {
        ASSERT(service != nullptr);
        ASSERT(_service == nullptr);
        ASSERT(_linchpin == nullptr);

        Config config;
        config.FromString(service->ConfigLine());

        if (config.DeviceId.Value().empty() == true) {
            return _T("WebRemote: 'deviceid' is not configured");
        }

        _topic = string(TOPIC_PREFIX) + config.DeviceId.Value();

        _service = service;
        _service->AddRef();
        _service->Register(&_notification);

        // Linchpin may already be running; otherwise OnPluginActivated() subscribes later.
        SubscribeToLinchpin();

        return string();
    }

    void WebRemote::Deinitialize(PluginHost::IShell* service)
    {
        ASSERT(_service == service);

        _service->Unregister(&_notification);
        UnsubscribeFromLinchpin();

        _service->Release();
        _service = nullptr;
        _topic.clear();

        SYSLOG(Logging::Shutdown, (string(_T("WebRemote de-initialised"))));
    }

    string WebRemote::Information() const
    {
        return string();
    }

    void WebRemote::OnPluginActivated(const string& callsign)
    {
        if (callsign == LINCHPIN_CALLSIGN) {
            SubscribeToLinchpin();
        }
    }

    void WebRemote::OnPluginDeactivated(const string& callsign)
    {
        if (callsign == LINCHPIN_CALLSIGN) {
            UnsubscribeFromLinchpin();
        }
    }

    void WebRemote::SubscribeToLinchpin()
    {
        _adminLock.Lock();

        if (_linchpin == nullptr) {
            _linchpin = _service->QueryInterfaceByCallsign<Exchange::ILinchpinClientProxy>(LINCHPIN_CALLSIGN);
        }

        if (_linchpin != nullptr) {
            _linchpin->Register(&_notification);

            if (_linchpin->Subscribe(LINCHPIN_CLIENT_ID, _topic) == Core::ERROR_NONE) {
                // Delivers a message already pending on the topic, if any.
                _linchpin->RegistrationCallbackOnMessageReceived(LINCHPIN_CLIENT_ID);
                LOGINFO("Subscribed to topic %s", _topic.c_str());
            } else {
                LOGERR("Failed to subscribe to topic %s", _topic.c_str());
                _linchpin->Unregister(&_notification);
                _linchpin->Release();
                _linchpin = nullptr;
            }
        } else {
            LOGWARN("%s is not available, waiting for activation", LINCHPIN_CALLSIGN);
        }

        _adminLock.Unlock();
    }

    void WebRemote::UnsubscribeFromLinchpin()
    {
        _adminLock.Lock();

        if (_linchpin != nullptr) {
            _linchpin->Unsubscribe(LINCHPIN_CLIENT_ID, _topic);
            _linchpin->Unregister(&_notification);
            _linchpin->Release();
            _linchpin = nullptr;
        }

        _adminLock.Unlock();
    }

    void WebRemote::HandleMessage(const string& topic, const string& payloadType, const string& payload)
    {
        if (topic != _topic) {
            return;
        }

        LOGINFO("Message on topic %s, payloadType %s", topic.c_str(), payloadType.c_str());

        Core::JSON::VariantContainer message;
        if (message.FromString(payload) == false) {
            LOGWARN("Payload is not a JSON object");
            return;
        }

        if (message["method"].String() != METHOD_GENERATE_KEY) {
            LOGWARN("Unsupported method %s", message["method"].String().c_str());
            return;
        }

        const string dataType = message["dataType"].String();
        const Core::JSON::VariantContainer data = message["data"].Object();

        if (dataType == DATA_TYPE_KEY_PRESS) {
            const string code = data["code"].String();
            char* end = nullptr;
            const long keyCode = std::strtol(code.c_str(), &end, 0);

            if ((code.empty() == false) && (*end == '\0') && (keyCode >= 0)) {
                SendKey(static_cast<int>(keyCode), static_cast<uint32_t>(data["modifier"].Number()),
                    ToSeconds(data["delay"].Double()), ToSeconds(data["duration"].Double()));
            } else if (code.empty() == false) {
                // A non numeric code is a custom key (e.g. "dog videos"), handled as a voice command.
                SendText(code);
            } else {
                LOGWARN("Key press without a code");
            }
        } else if (dataType == DATA_TYPE_TEXT) {
            const string text = data["text"].String();

            if (text.empty() == false) {
                SendText(text);
            } else {
                LOGWARN("Text message without text");
            }
        } else {
            LOGWARN("Unsupported dataType %s", dataType.c_str());
        }
    }

    bool WebRemote::SendKey(int code, uint32_t modifier, uint32_t delay, uint32_t duration)
    {
        bool success = false;
        Exchange::ITools* tools = _service->QueryInterfaceByCallsign<Exchange::ITools>(KEY_TARGET_CALLSIGN);

        if (tools != nullptr) {
            Exchange::ToolsKey key;
            key.code = code;
            key.modifier = static_cast<Exchange::Modifier>(modifier);
            key.delay = delay;
            key.duration = duration;

            bool generated = false;
            success = (tools->GenerateKeys(std::vector<Exchange::ToolsKey>(1, key), generated) == Core::ERROR_NONE) && generated;
            tools->Release();
        }

        if (success == false) {
            LOGERR("Failed to generate key %d", code);
        }
        return success;
    }

    bool WebRemote::SendText(const string& text)
    {
        bool success = false;
        Exchange::IVoiceControl* voice = _service->QueryInterfaceByCallsign<Exchange::IVoiceControl>(VOICECONTROL_CALLSIGN);

        if (voice != nullptr) {
            bool accepted = false;
            Core::OptionalType<string> sessionId;

            const Core::hresult result = voice->VoiceSessionRequest(Exchange::VoiceSessionRequestType::MIC_TRANSCRIPTION,
                Core::OptionalType<string>(text), Core::OptionalType<string>(), Core::OptionalType<string>(),
                Core::OptionalType<string>(string(VOICE_NAME)), accepted, sessionId);

            success = ((result == Core::ERROR_NONE) && accepted);
            voice->Release();
        }

        if (success == false) {
            LOGERR("Failed to send voice text");
        }
        return success;
    }

} // namespace Plugin
} // namespace WPEFramework
