#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/utils/web.hpp>
#include <random>
#include <fstream>
#include <filesystem>
#include <chrono>
#include <thread>
#include <regex>

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
                file << "bro died at {}%... skill issue 💀 ()\n";
                file << "certified choking hazard at {}% on [] 🙏\n";
                file << "{}% and still trash lmao get gud ()\n";
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
                file << "{}%? my cat could do better and he doesn't even have thumbs 😭 ()\n";
                file << "bro really choked at {}% on []... just uninstall already 💀\n";
                file << "{}%... i've seen bot accounts with better consistency 🙏\n";
                file << "imagine dying at {}% in 2026 🥂\n";
                file << "{}% is crazy. seek professional help 💀\n";
                file << "bro's heart rate went to 200 just to fail at {}% 😭 ()\n";
                file << "{}%... i'd rather watch paint dry than this gameplay 💀\n";
                file << "bro really hit the pause button on life at {}% 🙏\n";
                file << "{}%? yeah that's going in the fail compilation 🥂\n";
                file << "certified {}% enjoyer. stay bad bro 😭 ()\n";
                file << "{}%... even the level is laughing at you 💀\n";
                file << "bro's gaming chair clearly isn't expensive enough for {}% 🙏\n";
                file << "{}% is the new 100% for people who can't play 🥂\n";
                file << "imagine being this consistent at failing at {}% 😭\n";
                file << "{}%... i'm deleting the webhook so i don't have to see this trash 💀\n";
                file << "bro really thought he was him until {}% happened 🙏 ()\n";
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
                file << "GG WP! () beat []! 🥂\n";
                file << "massive W on [] after <> attempts! 😭\n";
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
        bool isInvalid = m_isPracticeMode || m_isTestMode || m_level->isPlatformer();
        if (isInvalid || Mod::get()->getSettingValue<bool>("disable_roasts")) {
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
        bool isInvalid = m_isPracticeMode || m_isTestMode || m_level->isPlatformer();
        PlayLayer::levelComplete();
        if (isInvalid) return;

        this->getScheduler()->scheduleSelector(
            schedule_selector(MyPlayLayer::captureAndSendCongrats),
            this, 0.1f, 0, 0.0f, false
        );
    }

    void captureAndSendRoast(float dt) { this->sendToDiscord(false); }
    void captureAndSendCongrats(float dt) { this->sendToDiscord(true); }

    std::string getResolutionString() {
        auto size = CCDirector::sharedDirector()->getOpenGLView()->getFrameSize();
        return std::to_string((int)size.width) + "x" + std::to_string((int)size.height);
    }

    std::string formatCustomMessage(std::string msg, int percent) {
        std::string userName = GJAccountManager::sharedState()->m_username;
        if (userName.empty()) userName = "Guest";

        std::string levelName = m_level->m_levelName;
        std::string attempts = std::to_string(m_level->m_attempts);
        
        int totalSeconds = static_cast<int>(m_level->m_workingTime);
        int mins = totalSeconds / 60;
        int secs = totalSeconds % 60;
        std::string timeStr = fmt::format("{:02}:{:02}", mins, secs);

        msg = std::regex_replace(msg, std::regex("\\(\\)"), userName);
        msg = std::regex_replace(msg, std::regex("\\[\\]"), levelName);
        msg = std::regex_replace(msg, std::regex("\\{\\}"), std::to_string(percent));
        msg = std::regex_replace(msg, std::regex("<>"), attempts);
        msg = std::regex_replace(msg, std::regex("!!"), timeStr);
        msg = std::regex_replace(msg, std::regex("~~"), getResolutionString());

        return msg;
    }

    void sendToDiscord(bool isVictory) {
        auto webhook = Mod::get()->getSettingValue<std::string>("webhook_url");
        if (webhook.empty()) return;

        std::string rawMessage;
        int percent = isVictory ? 100 : this->getCurrentPercentInt();
        auto& rng = m_fields->m_rng;

        if (isVictory) {
            if (m_fields->m_congrats.empty()) {
                rawMessage = "GG! () just beat []! 🥂";
            } else {
                std::uniform_int_distribution dis(0, (int)m_fields->m_congrats.size() - 1);
                rawMessage = m_fields->m_congrats[dis(rng)];
            }
        } else {
            if (m_fields->m_roasts.empty()) {
                rawMessage = "died at {}% lol get good";
            } else {
                std::uniform_int_distribution dis(0, (int)m_fields->m_roasts.size() - 1);
                rawMessage = m_fields->m_roasts[dis(rng)];
            }
            
            bool shouldPing = Mod::get()->getSettingValue<bool>("enable_ping");
            auto threshold = Mod::get()->getSettingValue<int64_t>("ping_threshold");
            std::string roleId = Mod::get()->getSettingValue<std::string>("role_id");
            if (shouldPing && percent >= threshold && !roleId.empty()) {
                rawMessage = "<@&" + roleId + "> " + rawMessage;
            }
        }

        std::string finalMessage = formatCustomMessage(rawMessage, percent);

        auto winSize = CCDirector::sharedDirector()->getWinSize();
        auto renderer = CCRenderTexture::create((int)winSize.width, (int)winSize.height);
        if (!renderer) return;

        renderer->beginWithClear(0, 0, 0, 0);
        this->visit();
        renderer->end();

        auto img = renderer->newCCImage(true);
        if (!img) return;

        auto path = Mod::get()->getSaveDir() / "ss.png";
        
        std::thread([this, path, finalMessage, webhook, img]() {
            bool ok = img->saveToFile(path.string().c_str());
            img->release();
            if (!ok) return;

            Loader::get()->queueInMainThread([this, path, finalMessage, webhook]() {
                utils::web::MultipartForm form;
                form.param("content", finalMessage);
                auto file = form.file("file", path, "image/png");
                
                if (file.isErr()) return;

                auto req = utils::web::WebRequest()
                    .bodyMultipart(form)
                    .timeout(std::chrono::seconds(15))
                    .post(webhook);

                m_fields->m_task.spawn(std::move(req), [](utils::web::WebResponse res) {
                    if (res.ok()) {
                        log::info("sent to discord");
                    } else {
                        log::error("webhook failed: {} code {}", res.errorMessage(), res.code());
                    }
                });
            });
        }).detach();
    }
};
