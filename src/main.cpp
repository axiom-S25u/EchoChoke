#include <Geode/Geode.hpp>
#include <Geode/loader/Event.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/utils/web.hpp>
#include <Geode/utils/async.hpp>
#include <random>
#include <fstream>
#include <filesystem>
#include <chrono>
#include <thread>
#include <algorithm>

using namespace geode::prelude;
namespace fs = std::filesystem;

class $modify(MyPlayLayer, PlayLayer) {
    struct Fields {
        std::vector<std::string> m_roasts;
        std::vector<std::string> m_congrats;
        bool m_loaded = false;
        async::TaskHolder<utils::web::WebResponse> m_task;
        std::mt19937 m_rng;
    };

    bool init(GJGameLevel* level, bool useReplay, bool dontSave) {
        if (!PlayLayer::init(level, useReplay, dontSave)) return false;
        if (!m_fields->m_loaded) {
            std::random_device rd;
            m_fields->m_rng = std::mt19937(rd());
            auto roastFile = Mod::get()->getSaveDir() / "roasts.txt";
            if (!fs::exists(roastFile)) {
                std::ofstream file(roastFile);
                file << "bro died at {}%... skill issue 💀\n";
                file << "certified choking hazard at {}% 🙏\n";
                file << "{}% and still trash lmao get gud\n";
                file << "bro really thought he had it but died at {}% 😭\n";
                file << "imagine getting to {}% just to choke like that 🙏\n";
                file << "{}%... my grandma plays better with one hand 💀\n";
                file << "another day, another {}% fail. consistency in being trash is crazy 🥂\n";
                file << "bro is allergic to 100%, currently stuck at {}% 💀\n";
                file << "{}%? yeah just delete the game at this point fr 🙏\n";
                file << "certified {}% moment. seek help 😭\n";
                file << "ok but who actually dies at {}%? oh wait, you do 💀\n";
                file << "bro's heartbeat peaked just to fail at {}%... tragic 🙏\n";
                file << "{}%... i'd be embarrassed to let the webhook even send this 🥂\n";
                file << "bro really saw {}% and decided to stop breathing 💀\n";
                file << "nice {}% fail bro, keep it up and you'll reach 100% by 2030 🙏\n";
                file << "i've seen better gameplay from a literal rock. {}%? embarrassing 😭\n";
                file << "{}%... is your monitor even turned on? 💀\n";
                file << "bro clicked 0.0001s too late at {}% and lost his soul 🙏\n";
                file << "invest in a better gaming chair if you're dying at {}% 🥂\n";
                file << "{}%? yeah i'm telling the whole server you're washed 💀\n";
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
                file << "Wait... you actually finished {}? hackers smh 💀\n";
                file << "bro finally finished {} after 5 years 🙏\n";
                file << "massive W on {}! now go touch grass 😭\n";
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
        bool isInvalid = m_isPracticeMode || m_isTestMode || m_level->isPlatformer() || m_startPercent > 0;
        if (isInvalid) {
            PlayLayer::destroyPlayer(player, obj);
            return;
        }
        int percent = this->getCurrentPercentInt();
        auto minPercent = Mod::get()->getSettingValue<int64_t>("min_percent");
        bool isNewBest = percent > m_level->m_normalPercent;
        PlayLayer::destroyPlayer(player, obj);
        if (isNewBest && percent >= minPercent) {
            this->getScheduler()->scheduleSelector(
                schedule_selector(MyPlayLayer::captureAndSendRoast),
                this, 0.1f, 0, 0.0f, false
            );
        }
    }

    void levelComplete() {
        bool isInvalid = m_isPracticeMode || m_isTestMode || m_level->isPlatformer() || m_startPercent > 0;
        if (isInvalid) {
            PlayLayer::levelComplete();
            return;
        }
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
        std::string message;
        int percent = this->getCurrentPercentInt();
        auto& rng = m_fields->m_rng;
        if (isVictory) {
            if (m_fields->m_congrats.empty()) {
                message = fmt::format("GG! you actually won on {}? sus", m_level->m_levelName);
            } else {
                std::uniform_int_distribution dis(0, (int)m_fields->m_congrats.size() - 1);
                message = fmt::format(fmt::runtime(m_fields->m_congrats[dis(rng)]), m_level->m_levelName.c_str());
            }
        } else {
            if (m_fields->m_roasts.empty()) {
                message = fmt::format("died at {}% lol get good", percent);
            } else {
                std::uniform_int_distribution dis(0, (int)m_fields->m_roasts.size() - 1);
                message = fmt::format(fmt::runtime(m_fields->m_roasts[dis(rng)]), percent);
            }
            bool shouldPing = Mod::get()->getSettingValue<bool>("enable_ping");
            auto threshold = Mod::get()->getSettingValue<int64_t>("ping_threshold");
            std::string roleId = Mod::get()->getSettingValue<std::string>("role_id");
            if (shouldPing && percent >= threshold && !roleId.empty()) {
                message = "<@&" + roleId + "> " + message;
            }
        }
        auto winSize = CCDirector::sharedDirector()->getWinSize();
        auto renderer = CCRenderTexture::create((int)winSize.width, (int)winSize.height);
        if (!renderer) return;
        renderer->beginWithClear(0, 0, 0, 0);
        this->visit();
        renderer->end();
        auto img = renderer->newCCImage(true);
        if (!img) return;
        auto path = Mod::get()->getSaveDir() / "ss.png";
        std::thread([path, message, webhook, img]() {
            bool ok = img->saveToFile(path.string().c_str());
            img->release();
            if (!ok) return;
            Loader::get()->queueInMainThread([path, message, webhook]() {
                utils::web::MultipartForm form;
                form.param("content", message);
                auto file = form.file("file", path, "image/png");
                if (file.isErr()) return;
                auto req = utils::web::WebRequest()
                    .bodyMultipart(form)
                    .timeout(std::chrono::seconds(12))
                    .post(webhook);
                m_fields->m_task.spawn(std::move(req), [](utils::web::WebResponse res) {
                    if (res.ok()) {
                        log::info("sent, check discord");
                    } else {
                        log::error("failed hard: {} code {}", res.errorMessage(), res.code());
                    }
                });
            });
        }).detach();
    }
};
