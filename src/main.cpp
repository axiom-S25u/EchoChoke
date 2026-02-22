#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/utils/web.hpp>
#include <cocos2d.h>
#include <fmt/core.h>
#include <fstream>
#include <vector>
#include <random>
#include <filesystem>

using namespace geode::prelude;
namespace fs = std::filesystem;

class MyPlayLayer : public geode::Modify<MyPlayLayer, PlayLayer> {
private:
    std::vector<std::string> roasts;
    bool roastsLoaded = false;

    void loadRoasts() {
        if (roastsLoaded) return;

        auto roastFile = Mod::get()->getSaveDir() / "roasts.txt";
        
        if (!fs::exists(roastFile)) {
            std::ofstream file(roastFile);
            file << "bro died at {}% on {}, skill issue 💀\n";
            file << "certified choking hazard at {}% 🙏\n";
            file << "imagine dying at {}%... couldn't be me 😭\n";
            file << "LMAOOO {} at {}%, stay humble buddy\n";
            file << "bruhhh you had one job at {}%\n";
            file.close();
        }

        std::ifstream file(roastFile);
        std::string line;
        while (std::getline(file, line)) {
            if (!line.empty() && line[0] != '#') {
                roasts.push_back(line);
            }
        }
        file.close();

        if (roasts.empty()) {
            roasts.push_back("bro died at {}% on {}, skill issue 💀");
        }

        roastsLoaded = true;
    }

    std::string getRandomRoast(int percent, const std::string& levelName) {
        loadRoasts();
        
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, roasts.size() - 1);
        
        std::string selected = roasts[dis(gen)];
        
        return fmt::format(selected, percent, levelName);
    }

    void captureAndSend(int currentPercent, const std::string& levelName) {
        auto director = CCDirector::sharedDirector();
        if (!director) return;

        auto scene = director->getRunningScene();
        if (!scene) return;

        auto winSize = director->getWinSize();
        auto renderTexture = CCRenderTexture::create(winSize.width, winSize.height, kCCTexture2DPixelFormat_RGBA8888);
        
        if (!renderTexture) return;

        renderTexture->begin();
        scene->visit();
        renderTexture->end();

        auto savePath = Mod::get()->getSaveDir() / "last_death.png";
        renderTexture->saveToFile(savePath.string().c_str(), kCCImageFormatPNG);

        std::string roast = getRandomRoast(currentPercent, levelName);
        sendToDiscord(savePath.string(), roast);
    }

    void sendToDiscord(const std::string& imagePath, const std::string& content) {
        auto webhookUrl = Mod::get()->getSettingValue<std::string>("webhook_url");
        
        if (webhookUrl.empty()) {
            return;
        }

        auto task = geode::utils::web::AsyncWebRequest();
        task->setURL(webhookUrl);
        task->setMethod(geode::utils::web::RequestMethod::Post);

        std::string boundary = "----FormBoundary7MA4YWxkTrZu0gW";
        std::string contentType = fmt::format("multipart/form-data; boundary={}", boundary);
        task->addHeader("Content-Type", contentType);

        std::string body;
        
        body += fmt::format("--{}\r\n", boundary);
        body += "Content-Disposition: form-data; name=\"content\"\r\n\r\n";
        body += content + "\r\n";

        std::ifstream fileStream(imagePath, std::ios::binary);
        if (fileStream.is_open()) {
            std::vector<char> fileData((std::istreambuf_iterator<char>(fileStream)), std::istreambuf_iterator<char>());
            fileStream.close();

            body += fmt::format("--{}\r\n", boundary);
            body += "Content-Disposition: form-data; name=\"file\"; filename=\"death.png\"\r\n";
            body += "Content-Type: image/png\r\n\r\n";
            
            body.insert(body.end(), fileData.begin(), fileData.end());
            body += "\r\n";
        }

        body += fmt::format("--{}--\r\n", boundary);

        task->setBody(body);

        task->send([](std::string response) {
            log::info("Discord webhook sent successfully");
        });

        task->setErrorHandler([](std::string error) {
            log::error("Failed to send to Discord: {}", error);
        });
    }

public:
    void destroyPlayer(PlayerObject* p0, GameObject* p1) {
        int currentPercent = this->getCurrentPercentInt();
        int minPercent = Mod::get()->getSettingValue<int64_t>("min_percent");
        bool isPractice = this->m_isPracticeMode;

        if (currentPercent >= minPercent && !isPractice) {
            std::string levelName = this->m_level->m_levelName;
            captureAndSend(currentPercent, levelName);
        }

        return MyPlayLayer::destroyPlayer(p0, p1);
    }
};
// meow meow meow meow meow meow meow meow meow meow meow meow meow meow meow meow meow meow meow meow meow meow meow meow, yes meow