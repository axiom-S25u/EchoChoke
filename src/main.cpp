#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/utils/web.hpp>
#include <random>
#include <fstream>
#include <filesystem>

using namespace geode::prelude;
namespace fs = std::filesystem;

class $modify(MyPlayLayer, PlayLayer) {
    struct Fields {
        std::vector<std::string> m_roasts;
        std::vector<std::string> m_congrats;
        bool m_loaded = false;
    };

    bool init(GJGameLevel* level, bool useReplay, bool dontSave) {
        if (!PlayLayer::init(level, useReplay, dontSave)) return false;

        if (!m_fields->m_loaded) {
            auto roastFile = Mod::get()->getSaveDir() / "roasts.txt";
            if (!fs::exists(roastFile)) {
                std::ofstream file(roastFile);
                file << "bro died at {}%... skill issue 💀\n";
                file << "certified choking hazard at {}% 🙏\n";
                file.close();
            }
            std::ifstream rFile(roastFile);
            std::string rLine;
            while (std::getline(rFile, rLine)) if (!rLine.empty()) m_fields->m_roasts.push_back(rLine);

            auto congratsFile = Mod::get()->getSaveDir() / "congrats.txt";
            if (!fs::exists(congratsFile)) {
                std::ofstream file(congratsFile);
                file << "GG WP! You beat {}! 🥂\n";
                file.close();
            }
            std::ifstream cFile(congratsFile);
            std::string cLine;
            while (std::getline(cFile, cLine)) if (!cLine.empty()) m_fields->m_congrats.push_back(cLine);

            m_fields->m_loaded = true;
        }
        return true;
    }

    void destroyPlayer(PlayerObject* player, GameObject* obj) {
        PlayLayer::destroyPlayer(player, obj);

        if (m_isPracticeMode) return;

        int percent = this->getCurrentPercentInt();
        int minPercent = Mod::get()->getSettingValue<int64_t>("min_percent");

        if (percent >= minPercent) {
            this->getScheduler()->scheduleSelector(
                schedule_selector(MyPlayLayer::captureAndSendRoast),
                this, 0.1f, 0, 0.0f, false
            );
        }
    }

    void levelComplete() {
        PlayLayer::levelComplete();
        
        this->getScheduler()->scheduleSelector(
            schedule_selector(MyPlayLayer::captureAndSendCongrats),
            this, 0.1f, 0, 0.0f, false
        );
    }

    void captureAndSendRoast(float dt) {
        sendToDiscord(false);
    }

    void captureAndSendCongrats(float dt) {
        sendToDiscord(true);
    }

    void sendToDiscord(bool isVictory) {
        auto webhook = Mod::get()->getSettingValue<std::string>("webhook_url");
        if (webhook.empty()) return;

        auto winSize = CCDirector::sharedDirector()->getWinSize();
        auto renderer = CCRenderTexture::create(winSize.width, winSize.height);
        renderer->begin();
        this->visit();
        renderer->end();

        auto img = renderer->newCCImage();
        if (!img) return;

        auto path = Mod::get()->getSaveDir() / "capture.png";
        img->saveToFile(path.string().c_str());
        img->release();

        std::string message;
        std::random_device rd;
        std::mt19937 gen(rd());
        
        if (isVictory) {
            if (m_fields->m_congrats.empty()) message = "GG!";
            else {
                std::uniform_int_distribution<> dis(0, m_fields->m_congrats.size() - 1);
                message = fmt::format(m_fields->m_congrats[dis(gen)], m_level->m_levelName);
            }
        } else {
            if (m_fields->m_roasts.empty()) message = "died lol";
            else {
                std::uniform_int_distribution<> dis(0, m_fields->m_roasts.size() - 1);
                message = fmt::format(m_fields->m_roasts[dis(gen)], this->getCurrentPercentInt());
            }
        }

        std::string boundary = "GeodeBoundary" + std::to_string(time(nullptr));
        std::string body = "--" + boundary + "\r\n";
        body += "Content-Disposition: form-data; name=\"content\"\r\n\r\n";
        body += message + "\r\n";
        body += "--" + boundary + "\r\n";
        body += "Content-Disposition: form-data; name=\"file\"; filename=\"ss.png\"\r\n";
        body += "Content-Type: image/png\r\n\r\n";

        std::ifstream file(path, std::ios::binary);
        std::string fileContent((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        body += fileContent + "\r\n";
        body += "--" + boundary + "--\r\n";

        utils::web::WebRequest()
            .bodyString(body)
            .header("Content-Type", "multipart/form-data; boundary=" + boundary)
            .post(webhook);
    }
};

// meow
