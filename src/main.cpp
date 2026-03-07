#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/utils/web.hpp>

#include <random>
#include <fstream>
#include <filesystem>
#include <chrono>
#include <thread>
#include <vector>
#include <string>
#include <iterator>

using namespace geode::prelude;
namespace fs = std::filesystem;

class $modify(MyPlayLayer, PlayLayer) {
    struct Fields {
        std::vector<std::string> m_roasts;
        std::vector<std::string> m_congrats;
        bool m_loaded = false;

        async::TaskHolder<utils::web::WebResponse> m_task;

        std::mt19937 m_rng;

        int m_lastDeathPercent = -1;
        int m_deathsAtSamePercent = 0;
        std::chrono::steady_clock::time_point m_firstDeathAtPercentTime;

        std::string m_pendingStuckMessage;
    };

    bool init(GJGameLevel* level, bool useReplay, bool dontSave) {
        if (!PlayLayer::init(level, useReplay, dontSave)) return false;

        if (!m_fields->m_loaded) {
            std::random_device rd;
            m_fields->m_rng = std::mt19937(rd());

            auto roastFile = Mod::get()->getSaveDir() / "roasts.txt";
            m_fields->m_roasts = {
                "bro died at {}%... skill issue 💀 ()",
                "certified choking hazard at {}% on [] 🙏",
                "{}% and still trash lmao get gud ()",
                "bro really thought he had it but died at {}% 😭",
                "imagine getting to {}% just to choke like that 🙏",
                "{}%... my grandma plays better with one hand 💀",
                "another day, another {}% fail. consistency in being trash is crazy 🥂",
                "bro is allergic to 100%, currently stuck at {}% 💀",
                "{}%? yeah just delete the game at this point fr 🙏",
                "certified {}% moment. seek help 😭",
                "ok but who actually dies at {}%? oh wait, you do 💀",
                "bro's heartbeat peaked just to fail at {}%... tragic 🙏",
                "{}%... i'd be embarrassed to let the webhook even send this 🥂",
                "bro really saw {}% and decided to stop breathing 💀",
                "nice {}% fail bro, keep it up and you'll reach 100% by 2030 🙏",
                "i've seen better gameplay from a literal rock. {}%? embarrassing 😭",
                "{}%... is your monitor even turned on? 💀",
                "bro clicked 0.0001s too late at {}% and lost his soul 🙏",
                "invest in a better gaming chair if you're dying at {}% 🥂",
                "{}%? yeah i'm telling the whole server you're washed 💀",
                "{}%? my cat could do better and he doesn't even have thumbs 😭 ()",
                "bro really choked at {}% on []... just uninstall already 💀",
                "{}%... i've seen bot accounts with better consistency 🙏",
                "imagine dying at {}% in 2026 🥂",
                "{}% is crazy. seek professional help 💀",
                "bro's heart rate went to 200 just to fail at {}% 😭 ()",
                "{}%... i'd rather watch paint dry than this gameplay 💀",
                "bro really hit the pause button on life at {}% 🙏",
                "{}%? yeah that's going in the fail compilation 🥂",
                "certified {}% enjoyer. stay bad bro 😭 ()",
                "{}%... even the level is laughing at you 💀",
                "bro's gaming chair clearly isn't expensive enough for {}% 🙏",
                "{}% is the new 100% for people who can't play 🥂",
                "imagine being this consistent at failing at {}% 😭",
                "{}%... i'm deleting the webhook so i don't have to see this trash 💀",
                "bro really thought he was him until {}% happened 🙏 ()",
            };

            if (!fs::exists(roastFile)) {
                std::ofstream f(roastFile);
                for (auto& r : m_fields->m_roasts) f << r << "\n";
            } else {
                m_fields->m_roasts.clear();
                std::ifstream rf(roastFile);
                std::string l;
                while (std::getline(rf, l)) if (!l.empty()) m_fields->m_roasts.push_back(l);
            }

            auto congratsFile = Mod::get()->getSaveDir() / "congrats.txt";
            m_fields->m_congrats = {
                "GG WP! () beat []! 🥂",
                "massive W on [] after <> attempts! 😭"
            };

            if (!fs::exists(congratsFile)) {
                std::ofstream f(congratsFile);
                for (auto& c : m_fields->m_congrats) f << c << "\n";
            } else {
                m_fields->m_congrats.clear();
                std::ifstream cf(congratsFile);
                std::string l;
                while (std::getline(cf, l)) if (!l.empty()) m_fields->m_congrats.push_back(l);
            }

            m_fields->m_loaded = true;
        }

        return true;
    }

    void destroyPlayer(PlayerObject* player, GameObject* obj) {
        bool bad = m_isPracticeMode || m_isTestMode || m_level->isPlatformer();

        if (bad || Mod::get()->getSettingValue<bool>("disable_roasts")) {
            PlayLayer::destroyPlayer(player, obj);
            return;
        }

        int percent = getCurrentPercentInt();

        if (Mod::get()->getSettingValue<bool>("enable_stuck_messages")) {
            auto now = std::chrono::steady_clock::now();

            if (percent == m_fields->m_lastDeathPercent) {
                m_fields->m_deathsAtSamePercent++;
            } else {
                m_fields->m_lastDeathPercent = percent;
                m_fields->m_deathsAtSamePercent = 1;
                m_fields->m_firstDeathAtPercentTime = now;
            }

            m_fields->m_pendingStuckMessage.clear();

            if (m_fields->m_deathsAtSamePercent > 5) {
                auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                    now - m_fields->m_firstDeathAtPercentTime
                ).count();

                int h = elapsed / 3600;
                int m = (elapsed % 3600) / 60;

                std::string t =
                    h >= 1 ? std::to_string(h) + " hour" + (h > 1 ? "s" : "")
                           : std::to_string(m) + " minute" + (m != 1 ? "s" : "");

                m_fields->m_pendingStuckMessage =
                    "bro has been stuck at " + std::to_string(percent) +
                    "% for " + t + " straight someone call a therapist";
            }
        }

        auto minPercent = Mod::get()->getSettingValue<int64_t>("min_percent");
        bool newBest = percent > m_level->m_normalPercent;

        PlayLayer::destroyPlayer(player, obj);

        if (newBest && percent >= minPercent) {
            this->getScheduler()->scheduleSelector(
                schedule_selector(MyPlayLayer::captureAndSendRoast),
                this, 0.1f, 0, 0.f, false
            );
        } else if (!m_fields->m_pendingStuckMessage.empty()) {
            this->getScheduler()->scheduleSelector(
                schedule_selector(MyPlayLayer::sendStuckMessageOnly),
                this, 0.1f, 0, 0.f, false
            );
        }
    }

    void levelComplete() {
        bool bad = m_isPracticeMode || m_isTestMode || m_level->isPlatformer();

        PlayLayer::levelComplete();

        if (!bad) {
            this->getScheduler()->scheduleSelector(
                schedule_selector(MyPlayLayer::captureAndSendCongrats),
                this, 0.1f, 0, 0.f, false
            );
        }
    }

    void captureAndSendRoast(float) { sendToDiscord(false); }
    void captureAndSendCongrats(float) { sendToDiscord(true); }

    std::string getResolutionString() {
        auto s = CCDirector::sharedDirector()->getWinSize();
        return std::to_string((int)s.width) + "x" + std::to_string((int)s.height);
    }

    std::string formatCustomMessage(std::string msg, int percent) {
        std::string user =
            GJAccountManager::sharedState()->m_username.empty()
                ? "Guest"
                : GJAccountManager::sharedState()->m_username;

        std::string level = m_level->m_levelName;
        std::string attempts = std::to_string(m_level->m_attempts);

        std::string timeStr = fmt::format(
            "{:02}:{:02}",
            (int)m_level->m_workingTime / 60,
            (int)m_level->m_workingTime % 60
        );

        auto replaceAll = [](std::string& s, const std::string& f, const std::string& t) {
            size_t p = 0;
            while ((p = s.find(f, p)) != std::string::npos) {
                s.replace(p, f.length(), t);
                p += t.length();
            }
        };

        replaceAll(msg, "()", user);
        replaceAll(msg, "[]", level);
        replaceAll(msg, "{}", std::to_string(percent));
        replaceAll(msg, "<>", attempts);
        replaceAll(msg, "!!", timeStr);
        replaceAll(msg, "~~", getResolutionString());

        return msg;
    }

    void sendStuckMessageOnly(float) {
        if (!Mod::get()->getSettingValue<bool>("enable_stuck_messages")) return;

        auto webhook = Mod::get()->getSettingValue<std::string>("webhook_url");
        if (webhook.empty() || m_fields->m_pendingStuckMessage.empty()) return;

        std::string msg = m_fields->m_pendingStuckMessage;
        m_fields->m_pendingStuckMessage.clear();

        utils::web::MultipartForm form;
        form.param("content", msg);

        auto req = utils::web::WebRequest()
            .bodyMultipart(form)
            .timeout(std::chrono::seconds(15))
            .post(webhook);

        m_fields->m_task.spawn(std::move(req), [](utils::web::WebResponse res) {
            if (!res.ok()) log::error("webhook failed: {} code {}", res.errorMessage(), res.code());
        });
    }

    void sendToDiscord(bool victory) {
        auto webhook = Mod::get()->getSettingValue<std::string>("webhook_url");
        if (webhook.empty()) return;

        int percent = victory ? 100 : getCurrentPercentInt();

        std::string raw;
        auto& rng = m_fields->m_rng;

        if (victory) {
            if (m_fields->m_congrats.empty()) raw = "GG! () just beat []! 🥂";
            else {
                std::uniform_int_distribution dis(0, (int)m_fields->m_congrats.size() - 1);
                raw = m_fields->m_congrats[dis(rng)];
            }
        } else {
            if (m_fields->m_roasts.empty()) raw = "died at {}% lol get good";
            else {
                std::uniform_int_distribution dis(0, (int)m_fields->m_roasts.size() - 1);
                raw = m_fields->m_roasts[dis(rng)];
            }
        }

        std::string finalMsg = formatCustomMessage(raw, percent);

        if (!m_fields->m_pendingStuckMessage.empty()) {
            finalMsg += "\n" + m_fields->m_pendingStuckMessage;
            m_fields->m_pendingStuckMessage.clear();
        }

        auto size = CCDirector::sharedDirector()->getWinSize();
        auto rt = CCRenderTexture::create((int)size.width, (int)size.height);
        if (!rt) return;

        rt->beginWithClear(0,0,0,0);
        this->visit();
        rt->end();

        auto img = rt->newCCImage(true);
        if (!img) return;

        auto tmp = (Mod::get()->getSaveDir() / "tmp_screenshot.png").string();

        std::thread([this, img, tmp, webhook, finalMsg]() {

            if (!img->saveToFile(tmp.c_str())) {
                Loader::get()->queueInMainThread([img]() { img->release(); });
                return;
            }

            Loader::get()->queueInMainThread([img]() { img->release(); });

            std::ifstream f(tmp, std::ios::binary);
            if (!f) {
                fs::remove(tmp);
                return;
            }

            std::vector<uint8_t> buf(
                (std::istreambuf_iterator<char>(f)),
                std::istreambuf_iterator<char>()
            );

            f.close();
            fs::remove(tmp);

            if (buf.empty()) return;

            Loader::get()->queueInMainThread([this, webhook, finalMsg, data = std::move(buf)]() {

                utils::web::MultipartForm form;
                form.param("content", finalMsg);
                form.file("file", data, "image.png", "image/png");

                auto req = utils::web::WebRequest()
                    .bodyMultipart(form)
                    .timeout(std::chrono::seconds(15))
                    .post(webhook);

                m_fields->m_task.spawn(std::move(req), [](utils::web::WebResponse res) {
                    if (!res.ok()) log::error("webhook failed: {} code {}", res.errorMessage(), res.code());
                });

            });

        }).detach();
    }
};
// hi whoever is reviewing this
// ok so the best way to make it work was to save send delete