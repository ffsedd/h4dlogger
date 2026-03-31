#include <LittleFS.h>

void startWeb()
{
    if (!LittleFS.begin())
    {
        Serial.println("LittleFS mount failed");
    }

    server.serveStatic("/", LittleFS, "/")
        .setDefaultFile("index.html");

    server.on("/data", HTTP_GET,
              [](AsyncWebServerRequest *req)
              {
                  req->send(200, "application/json", jsonData());
              });

    server.begin();
}