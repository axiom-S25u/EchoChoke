#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/MenuLayer.hpp>
#include <Geode/utils/web.hpp>
#include <Geode/ui/Popup.hpp>
#include <Geode/ui/Button.hpp>

using namespace geode::prelude;
namespace fs = std::filesystem;

static bool inDestroyPlayer = false;
// i dont get why ppl dont read the description, i had to add this..
// ok this might help i guess
class EchoChokeToast : public CCNode {
protected:
    bool init() {
        if (!CCNode::init()) return false;

        auto bg = CCScale9Sprite::create("GJ_square01.png");
        bg->setContentSize({130, 65});
        bg->setAnchorPoint({0, 0});
        this->addChild(bg);

        auto title = CCLabelBMFont::create("ECHOCHOKE", "goldFont.fnt");
        title->setScale(0.35f);
        title->setPosition({65, 52});
        this->addChild(title);

        auto msg = CCLabelBMFont::create("IT WONT WORK\nUNLESS ITS A\nNEW BEST.", "bigFont.fnt");
        msg->setScale(0.22f);
        msg->setAlignment(kCCTextAlignmentCenter);
        msg->setPosition({65, 22});
        this->addChild(msg);

        this->setPosition({5, 160});
        this->setScale(0.1f);

        this->runAction(CCSequence::create(
            CCEaseBackOut::create(CCScaleTo::create(0.3f, 1.0f)),
            CCDelayTime::create(5.0f),
            CCEaseBackIn::create(CCScaleTo::create(0.3f, 0.0f)),
            CCCallFunc::create(this, callfunc_selector(EchoChokeToast::removeSelf)),
            nullptr
        ));

        return true;
    }

    void removeSelf() {
        this->removeFromParent();
    }

public:
    static EchoChokeToast* create() {
        auto ret = new EchoChokeToast();
        if (ret->init()) {
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }
};

class $modify(EchoChokeMenuLayer, MenuLayer) {
    bool init() {
        if (!MenuLayer::init()) return false;

        auto toast = EchoChokeToast::create();
        this->addChild(toast, 999);

        return true;
    }
};

class $modify(NoclipDetectPre, PlayLayer) {
    static void onModify(auto& self) {
        (void) self.setHookPriority("PlayLayer::destroyPlayer", -0x800000);
    }

    void destroyPlayer(PlayerObject* player, GameObject* object) override {
        if (object != m_anticheatSpike) inDestroyPlayer = true;
        PlayLayer::destroyPlayer(player, object);

        if (inDestroyPlayer) {
            inDestroyPlayer = false;
        }
    }
};

struct LevelPercentRule {
    int levelId;
    float minPercent;
};

static std::vector<LevelPercentRule> parsePerLevelRules(const std::string& input) {
    std::vector<LevelPercentRule> rules;
    auto entries = utils::string::split(input, ",");
    for (auto& entry : entries) {
        auto trimmed = utils::string::trim(entry);
        if (trimmed.empty()) continue;
        auto colonPos = trimmed.find(':');
        if (colonPos == std::string::npos) continue;
        std::string idPart = utils::string::trim(trimmed.substr(0, colonPos));
        std::string pctPart = utils::string::trim(trimmed.substr(colonPos + 1));
        auto idRes = utils::numFromString<int>(idPart);
        auto pctRes = utils::numFromString<float>(pctPart);
        if (!idRes || !pctRes) continue;
        LevelPercentRule rule;
        rule.levelId = idRes.unwrap();
        rule.minPercent = pctRes.unwrap();
        rules.push_back(rule);
    }
    return rules;
}

class $modify(MyPlayLayer, PlayLayer) {
    static void onModify(auto& self) {
        (void) self.setHookPriority("PlayLayer::destroyPlayer", 0x800000);
    }
    struct Fields {
        std::vector<std::string> m_roasts;
        std::vector<std::string> m_platformerRoasts;
        std::vector<std::string> m_congrats;
        bool m_loaded = false;

        async::TaskHolder<utils::web::WebResponse> m_task;

        Ref<CCRenderTexture> m_renderTexture;

        utils::random::Generator m_rng;

        float m_lastDeathPercent = -1.f;
        int m_deathsAtSamePercent = 0;
        asp::time::Instant m_firstDeathAtPercentTime;

        std::string m_pendingStuckMessage;

        asp::time::Instant m_sessionStart;
        bool m_sessionStarted = false;
    };

    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) return false;

        auto size = CCDirector::sharedDirector()->getWinSize();
        m_fields->m_renderTexture = CCRenderTexture::create((int)size.width, (int)size.height);
        m_fields->m_sessionStart = asp::time::Instant::now();
        m_fields->m_sessionStarted = true;

        return true;
    }

    void setupHasCompleted() {
        PlayLayer::setupHasCompleted();

        if (!m_fields->m_loaded) {
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
                "{}%... what are we even doing here man 😭",
                "the audacity to die at {}% and still open the game tomorrow 💀",
                "{}% is sending me to an early grave fr 🙏",
                "bro peaked at {}% and then ceased to exist 😭",
                "{}%... i've seen tutorial players with more dignity 💀",
                "not a single soul was surprised by the {}% death tbh 🙏",
                "the level looked at () dying at {}% and felt bad 😭",
                "{}% again?? bro is stuck in a loop 💀",
                "average () experience: die at {}%, open reddit, close reddit, die again 🙏",
                "{}%... geometry dash is not for everyone and today it proved it 😭",
                "bro be grinding [] for weeks just to hit {}% and choke 💀",
                "() saw {}% and thought 'yeah that's where i live now' 🙏",
                "{}% death logged. progress: none. dignity: also none 😭",
                "dead at {}%, as tradition demands 💀",
            };

            if (!fs::exists(roastFile)) {
                auto data = utils::string::join(m_fields->m_roasts, "\n");
                (void)utils::file::writeString(roastFile, data);
            } else {
                std::string content;
                if (GEODE_UNWRAP_INTO_IF_OK(content, utils::file::readString(roastFile))) {
                    auto lines = utils::string::split(content, "\n");
                    m_fields->m_roasts.clear();

                    for (auto& line : lines) {
                        if (line.empty()) continue;
                        std::string cleaned = utils::string::replace(line, "( )", "()");
                        m_fields->m_roasts.push_back(cleaned);
                    }
                }
            }

            auto platformerRoastFile = Mod::get()->getSaveDir() / "platformer_roasts.txt";
            m_fields->m_platformerRoasts = {
                "() spent !! on [] and died 💀 get a job",
                "!! of suffering on [] and nothing to show for it 😭",
                "bro dedicated !! to [] and choked 🙏",
                "() really out here wasting !! on [] skill issue",
                "!! on [] and we're sending this to the server as evidence of suffering 💀",
                "!! invested into [] like a bad stock 😭 ()",
                "() logged !! on [] and the level still said no 🙏",
                "!! on [] is actually insane what are you doing bro 💀",
                "dedicated !! to [] and the level ate 😭 ()",
                "!! of gameplay on [] and this is what we have to show for it 🙏",
                "bro has been cooking [] for !! and the food is still raw 💀",
                "() spent !! on [] and has nothing but pain 😭",
                "!! on [] and the level remains untouched. legend 🙏",
                "sending this webhook because () wasted !! on [] and died 💀",
                "() clocked !! on [] before getting cooked 😭",
                "## into this session on [] and () is still dying 🙏",
                "## session, !! total, () and [] not seeing eye to eye 💀",
                "() just threw ## of session time on [] away 😭",
                "bro spent ## in one session on [] and has no medal 🙏",
                "## session time on [] and () found new ways to fail 💀",
            };

            if (!fs::exists(platformerRoastFile)) {
                auto data = utils::string::join(m_fields->m_platformerRoasts, "\n");
                (void)utils::file::writeString(platformerRoastFile, data);
            } else {
                std::string content;
                if (GEODE_UNWRAP_INTO_IF_OK(content, utils::file::readString(platformerRoastFile))) {
                    auto lines = utils::string::split(content, "\n");
                    m_fields->m_platformerRoasts.clear();

                    for (auto& line : lines) {
                        if (line.empty()) continue;
                        m_fields->m_platformerRoasts.push_back(line);
                    }
                }
            }

            auto congratsFile = Mod::get()->getSaveDir() / "congrats.txt";
            m_fields->m_congrats = {
                "GG WP! () beat [] 🥂",
                "massive W to () on [] after <> attempts! 😭",
                "() finally cooked [] after <> attempts fr 🙏",
                "() said bet and beat [] in <> attempts 💀",
                "in !! of total grind, () beat []. respect honestly 🥂",
                "() just ended the [] arc after <> attempts 😭",
                "() beat [] after !! of suffering. certified W 🙏",
                "not a single soul expected () to beat [] but here we are 💀",
                "() went <> deep on [] and came out victorious 🥂",
                "the [] demon has been slain by () after <> attempts 😭",
                "() clocked !! on [] and actually saw the end screen. insane 🙏",
                "respect to () for not quitting [] after <> attempts 💀",
                "() just did the unthinkable and beat [] 🥂",
                "after <> attempts of pure suffering, () beat [] 😭",
                "() is built different. [] beaten. <> attempts. 🙏",
                "GG () beat [] in !! total grind time. not a small feat 💀",
                "() said [] was easy after <> attempts. liar but congrats 🥂",
                "no way () actually beat [] 😭 after <> attempts",
                "() ran it back <> times and [] finally said yes 🙏",
                "the grind was real. () beat [] after !! 💀",
                "() popped off on [] after <> attempts no cap 🥂",
                "() said i'm not logging off till i beat [] and did it in <> attempts 😭",
            };

            if (!fs::exists(congratsFile)) {
                auto data = utils::string::join(m_fields->m_congrats, "\n");
                (void)utils::file::writeString(congratsFile, data);
            } else {
                std::string content;
                if (GEODE_UNWRAP_INTO_IF_OK(content, utils::file::readString(congratsFile))) {
                    auto lines = utils::string::split(content, "\n");
                    m_fields->m_congrats.clear();

                    for (auto& line : lines) {
                        if (line.empty()) continue;
                        m_fields->m_congrats.push_back(line);
                    }
                }
            }
            m_fields->m_loaded = true;
        }
    }

    float getCurrentPercentFloat() {
        if (m_isPlatformer) return 0.f;
        float raw = this->getCurrentPercent();
        return std::floor(raw * 100.f) / 100.f;
    }

    std::string formatPercentString(float percent) {
        float floored = std::floor(percent * 100.f) / 100.f;
        float intPart;
        float fracPart = std::modf(floored, &intPart);
        if (fracPart < 0.005f) {
            return utils::numToString(static_cast<int>(intPart));
        }
        std::string s = fmt::format("{:.2f}", floored);
        while (!s.empty() && s.back() == '0') s.pop_back();
        if (!s.empty() && s.back() == '.') s.pop_back();
        return s;
    }

    std::string getSessionTimeString() {
        if (!m_fields->m_sessionStarted) return "some time";
        auto now = asp::time::Instant::now();
        auto elapsed = static_cast<int>(now.durationSince(m_fields->m_sessionStart).seconds());
        int h = elapsed / 3600;
        int m = (elapsed % 3600) / 60;
        int s = elapsed % 60;
        if (h > 0) {
            return fmt::format("{}h {}m", h, m);
        } else if (m > 0) {
            return fmt::format("{}m {}s", m, s);
        } else {
            return fmt::format("{}s", s);
        }
    }

    void destroyPlayer(PlayerObject* player, GameObject* obj) override {
        inDestroyPlayer = false;
        if (obj == m_anticheatSpike) {
            PlayLayer::destroyPlayer(player, obj);
            return;
        }

        if (!Mod::get()->getSettingValue<bool>("mod_enabled")) {
            PlayLayer::destroyPlayer(player, obj);
            return;
        }

        bool isPlatformerMode = m_isPlatformer;
        bool isTestMode = m_isTestMode;
        bool isPractice = m_isPracticeMode;

        if (isPractice || isTestMode) {
            PlayLayer::destroyPlayer(player, obj);
            return;
        }

        bool isPlatformerRoastsEnabled = Mod::get()->getSettingValue<bool>("enable_platformer_roasts");

        if (isPlatformerMode && !isPlatformerRoastsEnabled) {
            PlayLayer::destroyPlayer(player, obj);
            return;
        }

        if (!isPlatformerMode && Mod::get()->getSettingValue<bool>("disable_roasts")) {
            PlayLayer::destroyPlayer(player, obj);
            return;
        }

        if (Mod::get()->getSettingValue<bool>("rated_only")) {
            if (m_level->m_stars.value() == 0 && m_level->m_demon.value() == 0) {
                PlayLayer::destroyPlayer(player, obj);
                return;
            }
        }

        int levelID = m_level->m_levelID.value();

        auto blacklistStr = Mod::get()->getSettingValue<std::string>("level_blacklist");
        if (!blacklistStr.empty()) {
            auto parts = utils::string::split(blacklistStr, ",");
            for (auto& part : parts) {
                auto trimmed = utils::string::trim(part);
                if (trimmed.empty()) continue;
                auto parsed = utils::numFromString<int>(trimmed);
                if (parsed && parsed.unwrap() == levelID) {
                    PlayLayer::destroyPlayer(player, obj);
                    return;
                }
            }
        }

        float effectiveMinPercent = static_cast<float>(Mod::get()->getSettingValue<int64_t>("min_percent"));

        auto whitelistStr = Mod::get()->getSettingValue<std::string>("level_whitelist");
        bool hasWhitelist = !whitelistStr.empty();
        bool foundInWhitelist = false;

        if (hasWhitelist) {
            auto rules = parsePerLevelRules(whitelistStr);
            bool foundAsRule = false;
            for (auto& rule : rules) {
                if (rule.levelId == levelID) {
                    effectiveMinPercent = rule.minPercent;
                    foundInWhitelist = true;
                    foundAsRule = true;
                    break;
                }
            }

            if (!foundAsRule) {
                auto parts = utils::string::split(whitelistStr, ",");
                for (auto& part : parts) {
                    auto trimmed = utils::string::trim(part);
                    if (trimmed.empty()) continue;
                    if (trimmed.find(':') != std::string::npos) continue;
                    auto parsed = utils::numFromString<int>(trimmed);
                    if (parsed && parsed.unwrap() == levelID) {
                        foundInWhitelist = true;
                        break;
                    }
                }
            }

            if (!foundInWhitelist) {
                PlayLayer::destroyPlayer(player, obj);
                return;
            }
        }

        float percent = isPlatformerMode ? 0.f : getCurrentPercentFloat();
        m_fields->m_pendingStuckMessage.clear();

        if (!isPlatformerMode && Mod::get()->getSettingValue<bool>("enable_stuck_messages")) {
            auto now = asp::time::Instant::now();

            if (std::abs(percent - m_fields->m_lastDeathPercent) < 0.01f) {
                m_fields->m_deathsAtSamePercent++;
            } else {
                m_fields->m_lastDeathPercent = percent;
                m_fields->m_deathsAtSamePercent = 1;
                m_fields->m_firstDeathAtPercentTime = now;
            }

            if (m_fields->m_deathsAtSamePercent > 5) {
                auto elapsed = now.durationSince(m_fields->m_firstDeathAtPercentTime).seconds();

                if (elapsed >= 60) {
                    int h = static_cast<int>(elapsed) / 3600;
                    int m = (static_cast<int>(elapsed) % 3600) / 60;

                    std::string t = h >= 1 ? utils::numToString(h) + " hour" + (h > 1 ? "s" : "")
                    : utils::numToString(m) + " minute" + (m != 1 ? "s" : "");

                    std::string percentStr = formatPercentString(percent);

                    m_fields->m_pendingStuckMessage = "bro has been stuck at " + percentStr +
                    "% for " + t + " straight someone call a therapist";
                }
            }
        }

        bool roastEvery = Mod::get()->getSettingValue<bool>("roast_every_death");
        bool shouldSend = false;

        if (isPlatformerMode) {
            shouldSend = true;
        } else {
            float currentBest = static_cast<float>(m_level->m_normalPercent.value());
            bool isNewBest = percent > currentBest;
            shouldSend = (roastEvery && percent >= effectiveMinPercent) || (isNewBest && percent >= effectiveMinPercent);
        }

        PlayLayer::destroyPlayer(player, obj);

        if (shouldSend) {
            this->getScheduler()->scheduleSelector(
                schedule_selector(MyPlayLayer::captureAndSendRoast),
                this, 0.3f, 0, 0.f, false
            );
        } else if (!m_fields->m_pendingStuckMessage.empty()) {
            this->getScheduler()->scheduleSelector(
                schedule_selector(MyPlayLayer::sendStuckMessageOnly),
                this, 0.3f, 0, 0.f, false
            );
        }
    }

    void levelComplete() {
        bool bad = m_isPracticeMode || m_isTestMode;
        PlayLayer::levelComplete();

        if (!Mod::get()->getSettingValue<bool>("mod_enabled")) return;

        if (!bad && !Mod::get()->getSettingValue<bool>("disable_congrats")) {
            this->getScheduler()->scheduleSelector(
                schedule_selector(MyPlayLayer::captureAndSendCongrats),
                this, 0.3f, 0, 0.f, false
            );
        }
    }

    void captureAndSendRoast(float) { sendToDiscord(false); }
    void captureAndSendCongrats(float) { sendToDiscord(true); }

    std::string getResolutionString() {
        auto s = CCDirector::sharedDirector()->getWinSize();
        return utils::numToString((int)s.width) + "x" + utils::numToString((int)s.height);
    }

    std::string formatCustomMessage(std::string msg, float percent) {
        std::string user = GJAccountManager::sharedState()->m_username.empty()
        ? "Guest"
        : GJAccountManager::sharedState()->m_username;

        std::string level = m_level->m_levelName;

        int totalAttempts = m_level->m_attempts.value();
        std::string attempts = geode::utils::numToString(totalAttempts);

        std::string timeStr = fmt::format(
            "{:02}:{:02}",
            static_cast<int>(m_level->m_workingTime) / 60,
            static_cast<int>(m_level->m_workingTime) % 60
        );

        std::string sessionTime = getSessionTimeString();
        std::string percentStr = formatPercentString(percent);

        utils::string::replaceIP(msg, "()", user);
        utils::string::replaceIP(msg, "[]", level);
        utils::string::replaceIP(msg, "{}", percentStr);
        utils::string::replaceIP(msg, "<>", attempts);
        utils::string::replaceIP(msg, "!!", timeStr);
        utils::string::replaceIP(msg, "~~", getResolutionString());
        utils::string::replaceIP(msg, "##", sessionTime);

        return msg;
    }

    void sendStuckMessageOnly(float) {
        if (!Mod::get()->getSettingValue<bool>("enable_stuck_messages")) return;

        auto webhook = Mod::get()->getSettingValue<std::string>("webhook_url");
        if (webhook.empty() || m_fields->m_pendingStuckMessage.empty()) return;

        std::string msg = m_fields->m_pendingStuckMessage;
        m_fields->m_pendingStuckMessage.clear();

        auto proxyUrl = Mod::get()->getSettingValue<std::string>("proxy_url");
        std::string targetUrl = proxyUrl.empty() ? webhook : (proxyUrl + "?wh=" + webhook);

        utils::web::MultipartForm form;
        form.param("content", msg);

        auto req = utils::web::WebRequest()
        .bodyMultipart(form)
        .timeout(std::chrono::seconds(15))
        .post(targetUrl);

        m_fields->m_task.spawn(std::move(req), [](utils::web::WebResponse res) {
            if (!res.ok()) log::error("webhook failed: {} code {}", res.errorMessage(), res.code());
        });
    }

    void sendToDiscord(bool victory) {
        auto webhook = Mod::get()->getSettingValue<std::string>("webhook_url");
        if (webhook.empty()) return;

        auto proxyUrl = Mod::get()->getSettingValue<std::string>("proxy_url");
        std::string targetUrl = proxyUrl.empty() ? webhook : (proxyUrl + "?wh=" + webhook);

        bool isPlatformerMode = m_isPlatformer;
        float percent = (victory || isPlatformerMode) ? 100.f : getCurrentPercentFloat();
        std::string raw;

        if (victory) {
            if (m_fields->m_congrats.empty()) {
                raw = "GG! () just beat [] 🥂";
            } else {
                size_t index = m_fields->m_rng.generate<size_t>() % m_fields->m_congrats.size();
                raw = m_fields->m_congrats[index];
            }
        } else if (isPlatformerMode) {
            if (m_fields->m_platformerRoasts.empty()) {
                raw = "() died on [] after ## session time. skill issue 💀";
            } else {
                uint32_t rareRoll = m_fields->m_rng.generate<uint32_t>() % 20;
                if (rareRoll == 0) {
                    raw = "me personally, Axiom say () has been grinding [] for ## and still dying. tragic.";
                } else {
                    size_t index = m_fields->m_rng.generate<size_t>() % m_fields->m_platformerRoasts.size();
                    raw = m_fields->m_platformerRoasts[index];
                }
            }
        } else {
            uint32_t rareRoll = m_fields->m_rng.generate<uint32_t>() % 20;
            if (rareRoll == 0) {
                raw = "me personally, Axiom say () have a massive skill issue at {}%. pathetic.";
            } else if (m_fields->m_roasts.empty()) {
                raw = "died at {}% lol get good";
            } else {
                size_t index = m_fields->m_rng.generate<size_t>() % m_fields->m_roasts.size();
                raw = m_fields->m_roasts[index];
            }
        }

        std::string finalMsg = formatCustomMessage(raw, percent);

        if (!victory && !isPlatformerMode && Mod::get()->getSettingValue<bool>("enable_ping")) {
            auto roleId = Mod::get()->getSettingValue<std::string>("role_id");
            float pingThreshold = static_cast<float>(Mod::get()->getSettingValue<int64_t>("ping_threshold"));
            if (!roleId.empty() && percent >= pingThreshold) {
                finalMsg = "<@&" + roleId + "> " + finalMsg;
            }
        }

        if (!m_fields->m_pendingStuckMessage.empty()) {
            finalMsg += "\n" + m_fields->m_pendingStuckMessage;
            m_fields->m_pendingStuckMessage.clear();
        }

        auto director = CCDirector::sharedDirector();
        auto glview = director->getOpenGLView();
        auto size = director->getWinSize();

        int logicalWidth = static_cast<int>(size.width);
        int logicalHeight = static_cast<int>(size.height);

        if (!m_fields->m_renderTexture ||
            m_fields->m_renderTexture->getSprite()->getContentSize().width != logicalWidth ||
            m_fields->m_renderTexture->getSprite()->getContentSize().height != logicalHeight) {

            m_fields->m_renderTexture = CCRenderTexture::create(logicalWidth, logicalHeight);
        }

        auto rt = m_fields->m_renderTexture;
        if (!rt) return;

        auto texSize = rt->getSprite()->getTexture()->getContentSizeInPixels();
        int pixelWidth = static_cast<int>(texSize.width);
        int pixelHeight = static_cast<int>(texSize.height);

        auto oldScaleX = glview->m_fScaleX;
        auto oldScaleY = glview->m_fScaleY;
        auto oldResolution = glview->getDesignResolutionSize();
        auto oldScreenSize = glview->m_obScreenSize;

        auto displayFactor = geode::utils::getDisplayFactor();
        glview->m_fScaleX = static_cast<float>(pixelWidth) / size.width / displayFactor;
        glview->m_fScaleY = static_cast<float>(pixelHeight) / size.height / displayFactor;

        auto aspectRatio = static_cast<float>(pixelWidth) / static_cast<float>(pixelHeight);
        auto newRes = CCSize{ std::round(320.f * aspectRatio), 320.f };

        director->m_obWinSizeInPoints = newRes;
        glview->m_obScreenSize = CCSize{ static_cast<float>(pixelWidth), static_cast<float>(pixelHeight) };
        glview->setDesignResolutionSize(newRes.width, newRes.height, kResolutionExactFit);

        rt->beginWithClear(0, 0, 0, 0);
        this->visit();

        auto rawData = std::make_shared<std::vector<GLubyte>>(pixelWidth * pixelHeight * 4);

        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        glReadPixels(0, 0, pixelWidth, pixelHeight, GL_RGBA, GL_UNSIGNED_BYTE, rawData->data());

        rt->end();

        glview->m_fScaleX = oldScaleX;
        glview->m_fScaleY = oldScaleY;
        director->m_obWinSizeInPoints = oldResolution;
        glview->m_obScreenSize = oldScreenSize;
        glview->setDesignResolutionSize(oldResolution.width, oldResolution.height, kResolutionExactFit);
        director->setViewport();

        uint32_t randomId = m_fields->m_rng.generate<uint32_t>();
        auto tmp = utils::string::pathToString(
            Mod::get()->getSaveDir() / fmt::format("tmp_screenshot_{}.png", randomId)
        );

        async::spawn([rawData, pixelWidth, pixelHeight, tmp, targetUrl, finalMsg]() -> arc::Future<void> {

            auto optData = co_await async::runtime().spawnBlocking<std::optional<std::vector<uint8_t>>>(
                [rawData, pixelWidth, pixelHeight, tmp]() -> std::optional<std::vector<uint8_t>> {

                    auto flippedData = new GLubyte[pixelWidth * pixelHeight * 4];
                    for (int i = 0; i < pixelHeight; ++i) {
                        std::memcpy(
                            &flippedData[i * pixelWidth * 4],
                            &rawData->data()[(pixelHeight - i - 1) * pixelWidth * 4],
                            pixelWidth * 4
                        );
                    }

                    CCImage image{};
                    image.m_nBitsPerComponent = 8;
                    image.m_nWidth = pixelWidth;
                    image.m_nHeight = pixelHeight;
                    image.m_bHasAlpha = true;
                    image.m_bPreMulti = false;
                    image.m_pData = flippedData;

                    bool saved = image.saveToFile(tmp.c_str(), true);

                    image.m_pData = nullptr;
                    delete[] flippedData;

                    if (!saved) return std::nullopt;

                    auto readResult = utils::file::readBinary(tmp);
                    std::error_code ec;
                    if (fs::exists(tmp)) fs::remove(tmp, ec);

                    if (!readResult.isOk()) return std::nullopt;
                    return readResult.unwrap();
                }
            );

            if (!optData.has_value() || optData->empty()) {
                co_return;
            }

            utils::web::MultipartForm form;
            form.param("content", finalMsg);
            form.file("file", std::move(optData.value()), "image.png", "image/png");

            auto req = utils::web::WebRequest()
            .bodyMultipart(form)
            .timeout(std::chrono::seconds(15))
            .post(targetUrl);

            async::spawn(
                std::move(req),
                [](utils::web::WebResponse res) {
                    if (!res.ok()) log::error("webhook failed: {} code {}", res.errorMessage(), res.code());
                }
            );
        });
    }
};