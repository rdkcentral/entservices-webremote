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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <future>
#include <string>
#include <vector>

#include "WebRemote.h"

#include "ServiceMock.h"
#include "WorkerPoolImplementation.h"

using ::testing::_;
using ::testing::DoAll;
using ::testing::Eq;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::SaveArg;
using namespace WPEFramework;

namespace {

const char LINCHPIN_CALLSIGN[] = "org.rdk.LinchpinClientProxy";
const char TOPIC[] = "X12345";

// Builds the inner Linchpin payload (the "payload" member of the published message).
string Payload(const string& method, const string& dataType, const string& data)
{
    return "{\"method\":\"" + method + "\",\"requestUid\":\"webRemote-X12345\",\"dataType\":\"" + dataType
        + "\",\"data\":" + data + ",\"notificationUID\":\"1234\"}";
}

// Records routed messages instead of calling the other plugins.
class TestableWebRemote : public Plugin::WebRemote {
public:
    struct Key {
        int code;
        uint32_t modifier;
        uint32_t delay;
        uint32_t duration;
    };

    std::vector<Key> keys;
    std::vector<string> texts;

protected:
    bool SendKey(int code, uint32_t modifier, uint32_t delay, uint32_t duration) override
    {
        keys.push_back({ code, modifier, delay, duration });
        return true;
    }

    bool SendText(const string& text) override
    {
        texts.push_back(text);
        return true;
    }
};

class LinchpinMock : public Exchange::ILinchpinClientProxy {
public:
    MOCK_METHOD(Core::hresult, Register, (Exchange::ILinchpinClientProxy::INotification*), (override));
    MOCK_METHOD(Core::hresult, Unregister, (Exchange::ILinchpinClientProxy::INotification*), (override));
    MOCK_METHOD(Core::hresult, RegistrationCallbackOnSubscriptionStatusChanged, (const string&), (override));
    MOCK_METHOD(Core::hresult, RegistrationCallbackOnMessageReceived, (const string&), (override));
    MOCK_METHOD(Core::hresult, Publish, (const string&, const string&, const string&), (override));
    MOCK_METHOD(Core::hresult, PublishResponse, (const string&, const string&, const string&, const string&), (const, override));
    MOCK_METHOD(Core::hresult, GetSubscriptionStatus, (const string&, ITopicInfoIterator*&), (override));
    MOCK_METHOD(Core::hresult, Subscribe, (const string&, const string&), (override));
    MOCK_METHOD(Core::hresult, Unsubscribe, (const string&, const string&), (override));
    MOCK_METHOD(Core::hresult, SendMetrics, (const string&, const string&, const string&), (override));

    uint32_t AddRef() const override { return Core::ERROR_NONE; }
    uint32_t Release() const override { return Core::ERROR_NONE; }

    BEGIN_INTERFACE_MAP(LinchpinMock)
    INTERFACE_ENTRY(Exchange::ILinchpinClientProxy)
    END_INTERFACE_MAP
};

// Marker job: with a single worker thread, it runs after every job queued before it.
class SyncJob : public Core::IDispatch {
public:
    explicit SyncJob(std::promise<void>& done)
        : _done(done)
    {
    }

    void Dispatch() override
    {
        _done.set_value();
    }

private:
    std::promise<void>& _done;
};

} // namespace

class WebRemoteTest : public ::testing::Test {
protected:
    NiceMock<ServiceMock> service;
    NiceMock<LinchpinMock> linchpin;
    Core::ProxyType<WorkerPoolImplementation> workerPool;
    Core::ProxyType<TestableWebRemote> pluginProxy;
    TestableWebRemote& plugin;
    bool linchpinAvailable{ true };
    string config{ "{\"deviceid\":\"12345\"}" };

    WebRemoteTest()
        : pluginProxy(Core::ProxyType<TestableWebRemote>::Create())
        , plugin(*pluginProxy)
    {
    }

    void SetUp() override
    {
        // One worker thread, so jobs run in submission order (see DrainWorkerPool).
        workerPool = Core::ProxyType<WorkerPoolImplementation>::Create(2, Core::Thread::DefaultStackSize(), 16);
        Core::IWorkerPool::Assign(&(*workerPool));
        workerPool->Run();

        ON_CALL(service, ConfigLine()).WillByDefault(Invoke([this]() { return config; }));
        ON_CALL(service, QueryInterfaceByCallsign(_, _))
            .WillByDefault(Invoke([this](const uint32_t, const string& name) -> void* {
                if ((linchpinAvailable == true) && (name == LINCHPIN_CALLSIGN)) {
                    return static_cast<void*>(static_cast<Exchange::ILinchpinClientProxy*>(&linchpin));
                }
                return nullptr;
            }));
        ON_CALL(linchpin, Register(_)).WillByDefault(Return(Core::ERROR_NONE));
        ON_CALL(linchpin, Unregister(_)).WillByDefault(Return(Core::ERROR_NONE));
        ON_CALL(linchpin, Subscribe(_, _)).WillByDefault(Return(Core::ERROR_NONE));
        ON_CALL(linchpin, Unsubscribe(_, _)).WillByDefault(Return(Core::ERROR_NONE));
        ON_CALL(linchpin, RegistrationCallbackOnMessageReceived(_)).WillByDefault(Return(Core::ERROR_NONE));
    }

    void TearDown() override
    {
        DrainWorkerPool();
        Core::IWorkerPool::Assign(nullptr);
        workerPool->Stop();
        workerPool = Core::ProxyType<WorkerPoolImplementation>();
    }

    // Blocks until every job queued so far has run.
    void DrainWorkerPool()
    {
        std::promise<void> done;
        std::future<void> finished = done.get_future();
        Core::IWorkerPool::Instance().Submit(
            Core::ProxyType<Core::IDispatch>(Core::ProxyType<SyncJob>::Create(done)));
        ASSERT_EQ(std::future_status::ready, finished.wait_for(std::chrono::seconds(5)));
    }

    // Initializes the plugin and leaves it ready to route messages on TOPIC.
    void InitializePlugin()
    {
        EXPECT_EQ(string(), plugin.Initialize(&service));
    }
};

TEST_F(WebRemoteTest, Initialize_FailsWithoutDeviceId)
{
    config = "{}";
    EXPECT_NE(string(), plugin.Initialize(&service));
}

TEST_F(WebRemoteTest, Initialize_SubscribesToDeviceTopic)
{
    EXPECT_CALL(linchpin, Register(_)).Times(1);
    EXPECT_CALL(linchpin, Subscribe(Eq("WebRemote"), Eq(TOPIC))).WillOnce(Return(Core::ERROR_NONE));
    EXPECT_CALL(linchpin, RegistrationCallbackOnMessageReceived(Eq("WebRemote"))).Times(1);

    InitializePlugin();

    EXPECT_CALL(linchpin, Unsubscribe(Eq("WebRemote"), Eq(TOPIC))).Times(1);
    EXPECT_CALL(linchpin, Unregister(_)).Times(1);
    plugin.Deinitialize(&service);
}

TEST_F(WebRemoteTest, Initialize_SubscribesWhenLinchpinActivatesLater)
{
    PluginHost::IPlugin::INotification* shellSink = nullptr;
    ON_CALL(service, Register(::testing::An<PluginHost::IPlugin::INotification*>()))
        .WillByDefault(SaveArg<0>(&shellSink));

    linchpinAvailable = false;
    EXPECT_CALL(linchpin, Subscribe(_, _)).Times(0);
    InitializePlugin();
    ASSERT_NE(nullptr, shellSink);

    linchpinAvailable = true;
    EXPECT_CALL(linchpin, Subscribe(Eq("WebRemote"), Eq(TOPIC))).WillOnce(Return(Core::ERROR_NONE));
    shellSink->Activated(LINCHPIN_CALLSIGN, nullptr);
    DrainWorkerPool();

    plugin.Deinitialize(&service);
}

TEST_F(WebRemoteTest, NotificationMessageIsHandledOnWorkerPool)
{
    Exchange::ILinchpinClientProxy::INotification* sink = nullptr;
    EXPECT_CALL(linchpin, Register(_)).WillOnce(DoAll(SaveArg<0>(&sink), Return(Core::ERROR_NONE)));

    InitializePlugin();
    ASSERT_NE(nullptr, sink);

    sink->OnMessageReceived(TOPIC, "5", Payload("generateKey", "text", R"({"text":"Launch Netflix"})"), "", "");
    DrainWorkerPool();

    ASSERT_EQ(1u, plugin.texts.size());
    EXPECT_EQ("Launch Netflix", plugin.texts[0]);
    plugin.Deinitialize(&service);
}

TEST_F(WebRemoteTest, KeyPress_HexCodeRoutedAsKey)
{
    InitializePlugin();
    plugin.HandleMessage(TOPIC, "5", Payload("generateKey", "keypress",
        R"({"code":"0x161","modifier":5,"delay":1,"duration":0.5})"));

    ASSERT_EQ(1u, plugin.keys.size());
    EXPECT_EQ(0x161, plugin.keys[0].code);
    EXPECT_EQ(5u, plugin.keys[0].modifier);
    EXPECT_EQ(1u, plugin.keys[0].delay);
    EXPECT_EQ(1u, plugin.keys[0].duration);
    EXPECT_TRUE(plugin.texts.empty());
    plugin.Deinitialize(&service);
}

TEST_F(WebRemoteTest, KeyPress_NumericCodeWithoutOptionalFields)
{
    InitializePlugin();
    plugin.HandleMessage(TOPIC, "5", Payload("generateKey", "keypress", R"({"code":28})"));

    ASSERT_EQ(1u, plugin.keys.size());
    EXPECT_EQ(28, plugin.keys[0].code);
    EXPECT_EQ(0u, plugin.keys[0].modifier);
    EXPECT_EQ(0u, plugin.keys[0].delay);
    EXPECT_EQ(0u, plugin.keys[0].duration);
    plugin.Deinitialize(&service);
}

TEST_F(WebRemoteTest, KeyPress_CustomKeyRoutedAsText)
{
    InitializePlugin();
    plugin.HandleMessage(TOPIC, "5", Payload("generateKey", "keypress", R"({"code":"dog videos"})"));

    EXPECT_TRUE(plugin.keys.empty());
    ASSERT_EQ(1u, plugin.texts.size());
    EXPECT_EQ("dog videos", plugin.texts[0]);
    plugin.Deinitialize(&service);
}

TEST_F(WebRemoteTest, Text_RoutedAsText)
{
    InitializePlugin();
    plugin.HandleMessage(TOPIC, "5", Payload("generateKey", "text",
        R"({"text":"Launch Netflix","source":"webremote"})"));

    EXPECT_TRUE(plugin.keys.empty());
    ASSERT_EQ(1u, plugin.texts.size());
    EXPECT_EQ("Launch Netflix", plugin.texts[0]);
    plugin.Deinitialize(&service);
}

TEST_F(WebRemoteTest, OtherTopicIsIgnored)
{
    InitializePlugin();
    plugin.HandleMessage("Xother", "5", Payload("generateKey", "text", R"({"text":"hello"})"));

    EXPECT_TRUE(plugin.keys.empty());
    EXPECT_TRUE(plugin.texts.empty());
    plugin.Deinitialize(&service);
}

TEST_F(WebRemoteTest, UnsupportedMethodIsIgnored)
{
    InitializePlugin();
    plugin.HandleMessage(TOPIC, "5", Payload("other", "text", R"({"text":"hello"})"));

    EXPECT_TRUE(plugin.keys.empty());
    EXPECT_TRUE(plugin.texts.empty());
    plugin.Deinitialize(&service);
}

TEST_F(WebRemoteTest, UnsupportedDataTypeIsIgnored)
{
    InitializePlugin();
    plugin.HandleMessage(TOPIC, "5", Payload("generateKey", "unknown", R"({"text":"hello"})"));

    EXPECT_TRUE(plugin.keys.empty());
    EXPECT_TRUE(plugin.texts.empty());
    plugin.Deinitialize(&service);
}

TEST_F(WebRemoteTest, InvalidPayloadIsIgnored)
{
    InitializePlugin();
    plugin.HandleMessage(TOPIC, "5", "not json");

    EXPECT_TRUE(plugin.keys.empty());
    EXPECT_TRUE(plugin.texts.empty());
    plugin.Deinitialize(&service);
}
