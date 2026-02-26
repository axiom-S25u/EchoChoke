#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/binding/PlayLayer.hpp>
#include <Geode/utils/web.hpp>
#include <cocos2d.h>
#include <fmt/core.h>
#include <fstream>
#include <vector>
#include <random>
#include <filesystem>

using namespace geode::prelude;
namespace fs = std::filesystem;
namespace web = geode::utils::web;

class $modify(MyPlayLayer, PlayLayer) {
    struct Fields {
        std::vector<std::string> m_roasts;
        std::vector<std::string> m_congratulations;
        bool m_roastsLoaded = false;
        bool m_congratsLoaded = false;
    };
    bool init(GJGameLevel* level, bool useReplay, bool dontSave) {
        if (!PlayLayer::init(level, useReplay, dontSave)) {
            return false;
        }
        
        loadRoasts();
        loadCongrats();
        return true;
    }

    void loadRoasts() {
        if (m_fields->m_roastsLoaded) return;
        auto roastFile = Mod::get()->getSaveDir() / "roasts.txt";
        if (!fs::exists(roastFile)) {
            std::ofstream file(roastFile);
            file << "bro died at {}% on {}, skill issue 💀\n";
            file << "certified choking hazard at {}% 🙏\n";
            file << "imagine dying at {}%... couldn't be me 😭\n";
            file.close();
        }

        std::ifstream file(roastFile);
        std::string line;
        while (std::getline(file, line)) {
            if (!line.empty()) m_fields->m_roasts.push_back(line);
        }
        m_fields->m_roastsLoaded = true;
    }

    void loadCongrats() {
        if (m_fields->m_congratsLoaded) return;
        auto congratsFile = Mod::get()->getSaveDir() / "congrats.txt";
        if (!fs::exists(congratsFile)) {
            std::ofstream file(congratsFile);
            file << "GG WP! You beat {}! 🥂\n";
            file.close();
        }

        std::ifstream file(congratsFile);
        std::string line;
        while (std::getline(file, line)) {
            if (!line.empty()) m_fields->m_congratulations.push_back(line);
        }
        m_fields->m_congratsLoaded = true;
    }

    std::string getRandomRoast(int percent, const std::string& levelName) {
        if (m_fields->m_roasts.empty()) return "skill issue 💀";
        static std::random_device rd;
        static std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, m_fields->m_roasts.size() - 1);
        return fmt::format(m_fields->m_roasts[dis(gen)], percent, levelName);
    }

    void trySendScreenshotRoast(float dt) {
        auto webhookUrl = Mod::get()->getSettingValue<std::string>("webhook_url");
        if (webhookUrl.empty()) return;

        int percent = this->getCurrentPercentInt();
        std::string levelName = m_level->m_levelName;
        std::string message = getRandomRoast(percent, levelName);

        auto winSize = CCDirector::sharedDirector()->getWinSize();
        auto renderer = CCRenderTexture::create(winSize.width, winSize.height);
        renderer->begin();
        this->visit();
        renderer->end();

        auto img = renderer->newCCImage();
        if (!img) return;

        auto path = Mod::get()->getSaveDir() / "temp_death.png";
        img->saveToFile(path.string().c_str());
        img->release();

        std::string boundary = "----GeodeBoundary" + std::to_string(time(nullptr));
        std::string body = "--" + boundary + "\r\n";
        body += "Content-Disposition: form-data; name=\"content\"\r\n\r\n";
        body += message + "\r\n";
        body += "--" + boundary + "\r\n";
        body += "Content-Disposition: form-data; name=\"file\"; filename=\"death.png\"\r\n";
        body += "Content-Type: image/png\r\n\r\n";

        std::ifstream imageFile(path, std::ios::binary);
        std::string imageContent((std::istreambuf_iterator<char>(imageFile)), std::istreambuf_iterator<char>());
        body += imageContent + "\r\n";
        body += "--" + boundary + "--\r\n";

        web::WebRequest()
            .bodyString(body)
            .header("Content-Type", "multipart/form-data; boundary=" + boundary)
            .post(webhookUrl);
    }

    void destroyPlayer(PlayerObject* p0, GameObject* p1) {
        PlayLayer::destroyPlayer(p0, p1);
        if (m_isPracticeMode) return;

        int percent = this->getCurrentPercentInt();
        int min = Mod::get()->getSettingValue<int64_t>("min_percent");
        
        if (percent >= min) {
            this->getScheduler()->scheduleSelector(
                schedule_selector(MyPlayLayer::trySendScreenshotRoast), 
                this, 0.1f, 0, 0.0f, false
            );
        }
    }
};
// meow meow meow meow meow meow meow meow meow meow meow meow meow meow meow meow meow meow meow meow meow meow meow meow, yes meow
