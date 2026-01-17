#include "bluetooth_uart.h"
#include "core/config.h"
#include "core/display.h"
#include "core/netserver.h"
#include "core/telnet.h"
#include <string.h>
#include <ctype.h>

static void trim(char *s)
{
    // trim leading
    char *p = s;
    while (*p && isspace((unsigned char)*p))
        p++;
    if (p != s)
        memmove(s, p, strlen(p) + 1);
    // trim trailing
    int l = strlen(s);
    while (l > 0 && isspace((unsigned char)s[l - 1]))
    {
        s[l - 1] = '\0';
        l--;
    }
}

void bluetooth_handle_line(const char *line)
{
    if (!line)
        return;
    if (strncmp(line, "BT:", 3) != 0)
        return;
    // Update lastSeen timestamp for heartbeat detection and clear any probe state
    if (btMetaMutex)
        xSemaphoreTake(btMetaMutex, pdMS_TO_TICKS(100));
    btMeta.lastSeen = millis();
    btMeta.probeSent = false;
    btMeta.probeDeadline = 0;
    if (btMetaMutex)
        xSemaphoreGive(btMetaMutex);
    char cmd[32] = {0};
    char value[256] = {0};
    const char *p = line + 3;
    const char *colon = strchr(p, ':');
    if (!colon)
    {
        strncpy(cmd, p, sizeof(cmd) - 1);
        trim(cmd);
    }
    else
    {
        size_t cmdlen = colon - p;
        if (cmdlen >= sizeof(cmd))
            cmdlen = sizeof(cmd) - 1;
        strncpy(cmd, p, cmdlen);
        cmd[cmdlen] = '\0';
        strncpy(value, colon + 1, sizeof(value) - 1);
        trim(value);
        trim(cmd);
    }

    if (strcmp(cmd, "CONNECTED") == 0)
    {
        if (btMetaMutex)
            xSemaphoreTake(btMetaMutex, pdMS_TO_TICKS(100));
        btMeta.connected = true;
        memset(btMeta.artist, 0, sizeof(btMeta.artist));
        memset(btMeta.title, 0, sizeof(btMeta.title));
        // clear any probe state and ensure artist empty on connect
        btMeta.probeSent = false;
        btMeta.probeDeadline = 0;
        if (btMetaMutex)
            xSemaphoreGive(btMetaMutex);
        Serial.println("BT: Connected set to true");
        if (config.getMode() == PM_BLUETOOTH)
            display.putRequest(NEWTITLE);
        return;
    }
    if (strcmp(cmd, "STATUS") == 0)
    {
        // response to probe: mark seen and clear probe state
        if (btMetaMutex)
            xSemaphoreTake(btMetaMutex, pdMS_TO_TICKS(100));
        btMeta.connected = true;
        btMeta.lastSeen = millis();
        btMeta.probeSent = false;
        btMeta.probeDeadline = 0;
        if (btMetaMutex)
            xSemaphoreGive(btMetaMutex);
        if (config.getMode() == PM_BLUETOOTH)
            display.putRequest(NEWTITLE);
        return;
    }
    if (strcmp(cmd, "DISCONNECTED") == 0)
    {
        if (btMetaMutex)
            xSemaphoreTake(btMetaMutex, pdMS_TO_TICKS(100));
        btMeta.connected = false;
        memset(btMeta.deviceName, 0, sizeof(btMeta.deviceName));
        memset(btMeta.deviceMAC, 0, sizeof(btMeta.deviceMAC));
        // set artist to Polish 'Brak połączenia' when no device connected
        strlcpy(btMeta.artist, "Brak połaczenia", sizeof(btMeta.artist));
        memset(btMeta.title, 0, sizeof(btMeta.title));
        if (btMetaMutex)
            xSemaphoreGive(btMetaMutex);
        if (config.getMode() == PM_BLUETOOTH)
            display.putRequest(NEWTITLE);
        return;
    }
    if (strcmp(cmd, "NAME") == 0)
    {
        if (btMetaMutex)
            xSemaphoreTake(btMetaMutex, pdMS_TO_TICKS(100));
        if (!btMeta.connected)
        {
            btMeta.connected = true;
            Serial.println("BT: Connected set to true from NAME");
        }
        strlcpy(btMeta.deviceName, value, sizeof(btMeta.deviceName));
        if (btMetaMutex)
            xSemaphoreGive(btMetaMutex);
        if (config.getMode() == PM_BLUETOOTH)
            display.putRequest(NEWTITLE);
        return;
    }
    if (strcmp(cmd, "MAC") == 0)
    {
        if (btMetaMutex)
            xSemaphoreTake(btMetaMutex, pdMS_TO_TICKS(100));
        strlcpy(btMeta.deviceMAC, value, sizeof(btMeta.deviceMAC));
        if (btMetaMutex)
            xSemaphoreGive(btMetaMutex);
        return;
    }
    if (strcmp(cmd, "ARTIST") == 0)
    {
        if (btMetaMutex)
            xSemaphoreTake(btMetaMutex, pdMS_TO_TICKS(100));
        if (!btMeta.connected)
        {
            btMeta.connected = true;
            Serial.println("BT: Connected set to true from ARTIST");
        }
        strlcpy(btMeta.artist, value, sizeof(btMeta.artist));
        if (btMetaMutex)
            xSemaphoreGive(btMetaMutex);
        if (config.getMode() == PM_BLUETOOTH)
            display.putRequest(NEWTITLE);
        return;
    }
    if (strcmp(cmd, "TITLE") == 0)
    {
        if (btMetaMutex)
            xSemaphoreTake(btMetaMutex, pdMS_TO_TICKS(100));
        if (!btMeta.connected)
        {
            btMeta.connected = true;
            Serial.println("BT: Connected set to true from TITLE");
        }
        strlcpy(btMeta.title, value, sizeof(btMeta.title));
        if (btMetaMutex)
            xSemaphoreGive(btMetaMutex);
        // snapshot updated metadata (bt_meta_snapshot takes the mutex internally)
        bt_metadata_t local;
        bt_meta_snapshot(&local);
        if (config.getMode() == PM_BLUETOOTH)
        {
            display.putRequest(NEWTITLE);
            if (strlen(local.artist) > 0 && strlen(local.title) > 0)
            {
                char meta[256];
                snprintf(meta, sizeof(meta), "%s - %s", local.artist, local.title);
                strlcpy(config.station.title, meta, sizeof(config.station.title));
                netserver.requestOnChange(TITLE, 0);
                telnet.printf("##CLI.META#: %s\r\n", meta);
            }
        }
        return;
    }
    if (strcmp(cmd, "PLAYING") == 0)
    {
        if (btMetaMutex)
            xSemaphoreTake(btMetaMutex, pdMS_TO_TICKS(100));
        btMeta.playing = true;
        // if we were awaiting ack for play, clear it when device confirms
        if (btMeta.awaitingAck && btMeta.expectedPlaying == true)
        {
            btMeta.awaitingAck = false;
            btMeta.ackDeadline = 0;
            btMeta.ackRetries = 0;
        }
        if (btMetaMutex)
            xSemaphoreGive(btMetaMutex);
        if (config.getMode() == PM_BLUETOOTH)
        {
            // update both bitrate widget (icon) and title lines
            display.putRequest(DBITRATE);
            display.putRequest(NEWTITLE);
        }
        return;
    }
    if (strcmp(cmd, "STOPPED") == 0)
    {
        if (btMetaMutex)
            xSemaphoreTake(btMetaMutex, pdMS_TO_TICKS(100));
        btMeta.playing = false;
        // clear ack if we were expecting stopped
        if (btMeta.awaitingAck && btMeta.expectedPlaying == false)
        {
            btMeta.awaitingAck = false;
            btMeta.ackDeadline = 0;
            btMeta.ackRetries = 0;
        }
        if (btMetaMutex)
            xSemaphoreGive(btMetaMutex);
        if (config.getMode() == PM_BLUETOOTH)
        {
            // when playback stops (pause), clear displayed meta and update title
            memset(config.station.title, 0, sizeof(config.station.title));
            netserver.requestOnChange(TITLE, 0);
            telnet.printf("##CLI.META#: \r\n");
            display.putRequest(DBITRATE);
            display.putRequest(NEWTITLE);
        }
        return;
    }
}
