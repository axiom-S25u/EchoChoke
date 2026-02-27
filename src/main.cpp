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
        EventListener<utils::web::WebResponseEvent> m_listener;
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
            while (std::getline(rFile, rLine)) {
                if (!rLine.empty()) m_fields->m_roasts.push_back(rLine);
            }

            auto congratsFile = Mod::get()->getSaveDir() / "congrats.txt";
            if (!fs::exists(congratsFile)) {
                std::ofstream file(congratsFile);
                file << "GG WP! You beat {}! 🥂\n";
                file.close();
            }
            
            std::ifstream cFile(congratsFile);
            std::string cLine;
            while (std::getline(cFile, cLine)) {
                if (!cLine.empty()) m_fields->m_congrats.push_back(cLine);
            }

            m_fields->m_loaded = true;
        }
        return true;
    }

    void destroyPlayer(PlayerObject* player, GameObject* obj) {
        PlayLayer::destroyPlayer(player, obj);
        if (m_isPracticeMode) return;

        int percent = this->getCurrentPercentInt();
        auto minPercent = Mod::get()->getSettingValue<int64_t>("min_percent");

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

    void captureAndSendRoast(float dt) { this->sendToDiscord(false); }
    void captureAndSendCongrats(float dt) { this->sendToDiscord(true); }

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

        auto path = Mod::get()->getSaveDir() / "ss.png";
        bool saved = img->saveToFile(path.string().c_str());
        img->release();
        if (!saved) return;

        std::string message;
        std::random_device rd;
        std::mt19937 gen(rd());
        
        if (isVictory) {
            if (m_fields->m_congrats.empty()) {
                message = "GG!";
            } else {
                std::uniform_int_distribution<> dis(0, (int)m_fields->m_congrats.size() - 1);
                message = fmt::format(fmt::runtime(m_fields->m_congrats[dis(gen)]), m_level->m_levelName.c_str());
            }
        } else {
            if (m_fields->m_roasts.empty()) {
                message = "died lol";
            } else {
                std::uniform_int_distribution<> dis(0, (int)m_fields->m_roasts.size() - 1);
                message = fmt::format(fmt::runtime(m_fields->m_roasts[dis(gen)]), this->getCurrentPercentInt());
            }
        }

        m_fields->m_listener.bind([](utils::web::WebResponseEvent* e) {
            if (auto res = e->getResponse()) {
                if (res->ok()) {
                    log::info("Sent to Discord successfully!");
                } else {
                    log::error("Discord error: {}", res->code());
                }
            }
        });

        utils::web::MultipartForm form;
        form.add("content", message);
        form.addFile("file", path);

        m_fields->m_listener.setFilter(
            utils::web::WebRequest()
                .bodyMultipart(form)
                .post(webhook, Mod::get())
        );
    }
};
