#include <Geode/Geode.hpp>
#include <Geode/loader/Event.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/utils/web.hpp>
#include <Geode/utils/async.hpp>
#include <random>
#include <fstream>
#include <filesystem>
#include <chrono>
using namespace geode::prelude;
namespace fs = std::filesystem;
class $modify(MyPlayLayer, PlayLayer) {
    struct Fields {
        std::vector<std::string> m_roasts;
        std::vector<std::string> m_congrats;
        bool m_loaded = false;
        async::TaskHolder<utils::web::WebResponse> m_task;
    };
    bool init(GJGameLevel* level, bool useReplay, bool dontSave) {
        if (!PlayLayer::init(level, useReplay, dontSave)) return false;
        if (!m_fields->m_loaded) {
            auto roastFile = Mod::get()->getSaveDir() / "roasts.txt";
            if (!fs::exists(roastFile)) {
                std::ofstream file(roastFile);
                file << "bro died at {}%... skill issue 💀\n";
                file << "certified choking hazard at {}% 🙏\n";
                file << "{}% and still trash lmao get gud\n";
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
        if (webhook.empty()) {
            log::error("no webhook url set idiot");
            return;
        }
        std::string message;
        std::random_device rd;
        std::mt19937 gen(rd());
        if (isVictory) {
            if (m_fields->m_congrats.empty()) message = "GG! you actually won?";
            else {
                std::uniform_int_distribution<> dis(0, (int)m_fields->m_congrats.size() - 1);
                message = fmt::format(fmt::runtime(m_fields->m_congrats[dis(gen)]), m_level->m_levelName.c_str());
            }
        } else {
            if (m_fields->m_roasts.empty()) message = "died lol skill issue";
            else {
                std::uniform_int_distribution<> dis(0, (int)m_fields->m_roasts.size() - 1);
                message = fmt::format(fmt::runtime(m_fields->m_roasts[dis(gen)]), this->getCurrentPercentInt());
            }
        }
        auto winSize = CCDirector::sharedDirector()->getWinSize();
        auto renderer = CCRenderTexture::create(winSize.width, winSize.height);
        renderer->begin();
        this->visit();
        renderer->end();
        auto img = renderer->newCCImage();
        if (!img) {
            log::error("no image from renderer wtf");
            return;
        }
        auto path = Mod::get()->getSaveDir() / "ss.png";
        bool saved = img->saveToFile(path.string().c_str());
        img->release();
        log::info("screenshot saved? {}", saved ? "yes" : "no fuck");
        if (!saved) return;
        utils::web::MultipartForm form;
        form.param("content", message);
        auto fileRes = form.file("file", path, "image/png");
        log::info("multipart file added? {}", fileRes.isOk() ? "yes" : "no error");
        if (fileRes.isErr()) {
            log::error("file add failed lol");
            return;
        }
        log::info("sending photo + text to {}", webhook);
        auto req = utils::web::WebRequest()
            .bodyMultipart(form)
            .certVerification(false)
            .timeout(std::chrono::seconds(8))
            .userAgent("EchoChoke/1.0 GD")
            .post(webhook);
        m_fields->m_task.spawn(std::move(req), [](utils::web::WebResponse res) {
            if (res.ok()) {
                log::info("SENT PHOTO + TEXT BITCH check discord");
            } else {
                log::error("failed hard: {} code {}", res.errorMessage(), res.code());
            }
        });
    }
}; // MEOWWWW, thanks jukebox for borrowing me code
