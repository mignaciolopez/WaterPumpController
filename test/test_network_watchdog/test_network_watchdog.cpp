#include <gtest/gtest.h>

#include "../support/Fakes.h"
#include "core/NetworkWatchdog.h"

using namespace CE;
using namespace CE::Core;

namespace
{
    class NetworkWatchdogTest : public ::testing::Test
    {
    protected:
        Fakes::FakeNetwork network;
        Fakes::FakeRestarter restarter;
        Fakes::FakeLog log;
        NetworkWatchdog watchdog{network, restarter, log, NetworkWatchdogConfig{3, 2}};
    };

    TEST_F(NetworkWatchdogTest, HealthyNetworkDoesNothing)
    {
        for (int i = 0; i < 10; ++i) EXPECT_TRUE(watchdog.Tick());

        EXPECT_EQ(network.reconnectCount, 0);
        EXPECT_EQ(restarter.restartCount, 0);
    }

    TEST_F(NetworkWatchdogTest, ReconnectsAfterConsecutiveFailures)
    {
        network.pingOk = false;

        EXPECT_FALSE(watchdog.Tick());
        EXPECT_FALSE(watchdog.Tick());
        EXPECT_EQ(network.reconnectCount, 0);

        EXPECT_FALSE(watchdog.Tick());
        EXPECT_EQ(network.reconnectCount, 1);
        EXPECT_EQ(restarter.restartCount, 0);
    }

    TEST_F(NetworkWatchdogTest, TheLogSaysWhetherTheLinkOrThePingFailed)
    {
        network.pingOk = false;
        watchdog.Tick();
        network.pingOk = true;
        network.connected = false;
        watchdog.Tick();

        ASSERT_GE(log.messages.size(), 2u);
        EXPECT_EQ(log.messages[0], "Network check failed: the gateway did not answer a ping");
        EXPECT_EQ(log.messages[1], "Network check failed: the WiFi link is down");
    }

    TEST_F(NetworkWatchdogTest, LinkDownCountsAsFailure)
    {
        network.connected = false;

        for (int i = 0; i < 3; ++i) watchdog.Tick();

        EXPECT_EQ(network.reconnectCount, 1);
    }

    TEST_F(NetworkWatchdogTest, RestartsWhenReconnectsKeepFailing)
    {
        network.pingOk = false;

        for (int i = 0; i < 5; ++i) watchdog.Tick();
        EXPECT_EQ(restarter.restartCount, 0);

        watchdog.Tick(); // 6th failure = 2nd reconnect round
        EXPECT_EQ(network.reconnectCount, 2);
        EXPECT_EQ(restarter.restartCount, 1);
    }

    TEST_F(NetworkWatchdogTest, AGoodCheckResetsTheEscalation)
    {
        network.pingOk = false;
        for (int i = 0; i < 3; ++i) watchdog.Tick(); // 1 reconnect
        network.pingOk = true;
        EXPECT_TRUE(watchdog.Tick());

        network.pingOk = false;
        for (int i = 0; i < 5; ++i) watchdog.Tick(); // 1 more reconnect, not yet a restart

        EXPECT_EQ(network.reconnectCount, 2);
        EXPECT_EQ(restarter.restartCount, 0);
    }

    TEST_F(NetworkWatchdogTest, ConfigComesFromSettings)
    {
        Settings settings;
        settings.maxReconnects = 4;

        const auto config = NetworkWatchdogConfigFrom(settings);

        EXPECT_EQ(config.reconnectsBeforeRestart, 4);
        EXPECT_EQ(config.failuresBeforeReconnect, 3);
    }
}

#include "../support/Main.h"
