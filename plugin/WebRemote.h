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

#pragma once

#include "Module.h"
#include <interfaces/ILinchpinClientProxy.h>
#include "UtilsLogging.h"

namespace WPEFramework {
namespace Plugin {

    // Interface-less plugin: relays Linchpin topic messages to key generation or voice control.
    class WebRemote : public PluginHost::IPlugin {
    public:
        WebRemote(const WebRemote&) = delete;
        WebRemote& operator=(const WebRemote&) = delete;

        WebRemote();
        ~WebRemote() override;

        BEGIN_INTERFACE_MAP(WebRemote)
        INTERFACE_ENTRY(PluginHost::IPlugin)
        END_INTERFACE_MAP

        const string Initialize(PluginHost::IShell* service) override;
        void Deinitialize(PluginHost::IShell* service) override;
        string Information() const override;

        // Entry point for every message received on the subscribed topic.
        void HandleMessage(const string& topic, const string& payloadType, const string& payload);

    protected:
        virtual bool SendKey(int code, uint32_t modifier, uint32_t delay, uint32_t duration);
        virtual bool SendText(const string& text);

    private:
        class Config : public Core::JSON::Container {
        public:
            Config(const Config&) = delete;
            Config& operator=(const Config&) = delete;
            Config()
                : Core::JSON::Container()
                , DeviceId()
            {
                Add(_T("deviceid"), &DeviceId);
            }
            ~Config() override = default;

            Core::JSON::String DeviceId;
        };

        class Notification : public Exchange::ILinchpinClientProxy::INotification,
                             public PluginHost::IPlugin::INotification {
        public:
            Notification() = delete;
            Notification(const Notification&) = delete;
            Notification& operator=(const Notification&) = delete;

            explicit Notification(WebRemote* parent)
                : _parent(*parent)
            {
                ASSERT(parent != nullptr);
            }
            ~Notification() override = default;

            BEGIN_INTERFACE_MAP(Notification)
            INTERFACE_ENTRY(Exchange::ILinchpinClientProxy::INotification)
            INTERFACE_ENTRY(PluginHost::IPlugin::INotification)
            END_INTERFACE_MAP

            void OnSubscriptionStatusChanged(const string& clientId, const string& topicList) override
            {
                LOGINFO("OnSubscriptionStatusChanged: clientId: %s, topicList: %s", clientId.c_str(), topicList.c_str());
            }

            void OnMessageReceived(const string& topic, const string& payloadType, const string& payload,
                const string& headers, const string& responseTopic) override
            {
                (void)headers;
                (void)responseTopic;
                _parent.HandleMessage(topic, payloadType, payload);
            }

            void Activated(const string& callsign, PluginHost::IShell*) override
            {
                _parent.OnPluginActivated(callsign);
            }

            void Deactivated(const string& callsign, PluginHost::IShell*) override
            {
                _parent.OnPluginDeactivated(callsign);
            }

            void Unavailable(const string&, PluginHost::IShell*) override
            {
            }

        private:
            WebRemote& _parent;
        };

        void OnPluginActivated(const string& callsign);
        void OnPluginDeactivated(const string& callsign);
        void SubscribeToLinchpin();
        void UnsubscribeFromLinchpin();

        PluginHost::IShell* _service;
        Exchange::ILinchpinClientProxy* _linchpin;
        string _topic;
        Core::CriticalSection _adminLock;
        Core::Sink<Notification> _notification;
    };

} // namespace Plugin
} // namespace WPEFramework
