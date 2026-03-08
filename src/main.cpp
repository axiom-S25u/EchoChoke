#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/utils/web.hpp>

using namespace geode::prelude;
namespace fs = std::filesystem;

static bool inDestroyPlayer = false;

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


class $modify(MyPlayLayer, PlayLayer) {
    static void onModify(auto& self) {
        (void) self.setHookPriority("PlayLayer::destroyPlayer", 0x800000);
    }
    struct Fields {
        std::vector<std::string> m_roasts;
        std::vector<std::string> m_congrats;
        bool m_loaded = false;
        
        async::TaskHolder<utils::web::WebResponse> m_task;
        
        Ref<CCRenderTexture> m_renderTexture;
        
        utils::random::Generator m_rng;
        
        int m_lastDeathPercent = -1;
        int m_deathsAtSamePercent = 0;
        asp::time::Instant m_firstDeathAtPercentTime;
        
        std::string m_pendingStuckMessage;
    };
    
    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) return false;
        
        auto size = CCDirector::sharedDirector()->getWinSize();
        m_fields->m_renderTexture = CCRenderTexture::create((int)size.width, (int)size.height);
        
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
            
            auto congratsFile = Mod::get()->getSaveDir() / "congrats.txt";
            m_fields->m_congrats = {
                "GG WP! () beat []! 🥂",
                "massive W on () after <> attempts! 😭"
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
    
    void destroyPlayer(PlayerObject* player, GameObject* obj) override {
        inDestroyPlayer = false;
        if (obj == m_anticheatSpike) {
            PlayLayer::destroyPlayer(player, obj);
            return;
        }
        
        bool bad = m_isPracticeMode || m_isTestMode || m_isPlatformer;
        
        if (bad || Mod::get()->getSettingValue<bool>("disable_roasts")) {
            PlayLayer::destroyPlayer(player, obj);
            return;
        }
        
        int percent = getCurrentPercentInt();
        m_fields->m_pendingStuckMessage.clear();
        
        if (Mod::get()->getSettingValue<bool>("enable_stuck_messages")) {
            auto now = asp::time::Instant::now();
            
            if (percent == m_fields->m_lastDeathPercent) {
                m_fields->m_deathsAtSamePercent++;
            } else {
                m_fields->m_lastDeathPercent = percent;
                m_fields->m_deathsAtSamePercent = 1;
                m_fields->m_firstDeathAtPercentTime = now;
            }
            
            if (m_fields->m_deathsAtSamePercent > 5) {
                auto elapsed = now.durationSince(m_fields->m_firstDeathAtPercentTime).seconds();
                
                if (elapsed >= 60) {
                    int h = elapsed / 3600;
                    int m = (elapsed % 3600) / 60;
                    
                    std::string t = h >= 1 ? utils::numToString(h) + " hour" + (h > 1 ? "s" : "")
                    : utils::numToString(m) + " minute" + (m != 1 ? "s" : "");
                    
                    m_fields->m_pendingStuckMessage = "bro has been stuck at " + utils::numToString(percent) +
                    "% for " + t + " straight someone call a therapist";
                }
            }
        }
        
        auto minPercent = Mod::get()->getSettingValue<int64_t>("min_percent");
        
        PlayLayer::destroyPlayer(player, obj);
        
        if (percent >= minPercent) {
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
        bool bad = m_isPracticeMode || m_isTestMode || m_isPlatformer;
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
        return utils::numToString((int)s.width) + "x" + utils::numToString((int)s.height);
    }
    
    std::string formatCustomMessage(std::string msg, int percent) {
        std::string user = GJAccountManager::sharedState()->m_username.empty()
        ? "Guest"
        : GJAccountManager::sharedState()->m_username;
        
        std::string level = m_level->m_levelName;
        std::string attempts = geode::utils::numToString(static_cast<int>(GJBaseGameLayer::get()->m_attempts));
        
        std::string timeStr = fmt::format(
            "{:02}:{:02}",
            static_cast<int>(m_level->m_workingTime) / 60,
            static_cast<int>(m_level->m_workingTime) % 60
        );
        
        utils::string::replaceIP(msg, "()", user);
        utils::string::replaceIP(msg, "[]", level);
        utils::string::replaceIP(msg, "{}", utils::numToString(percent));
        utils::string::replaceIP(msg, "<>", attempts);
        utils::string::replaceIP(msg, "!!", timeStr);
        utils::string::replaceIP(msg, "~~", getResolutionString());
        
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
        
        if (victory) {
            if (m_fields->m_congrats.empty()) {
                raw = "GG! () just beat []! 🥂";
            } else {
                size_t index = m_fields->m_rng.generate<size_t>() % m_fields->m_congrats.size();
                raw = m_fields->m_congrats[index];
            }
        } else {
            if (m_fields->m_roasts.empty()) {
                raw = "died at {}% lol get good";
            } else {
                size_t index = m_fields->m_rng.generate<size_t>() % m_fields->m_roasts.size();
                raw = m_fields->m_roasts[index];
            }
        }
        
        std::string finalMsg = formatCustomMessage(raw, percent);
        
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
            
            async::spawn([rawData, pixelWidth, pixelHeight, tmp, webhook, finalMsg]() -> arc::Future<void> {
                
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
                .post(webhook);
                
                async::spawn(
                    std::move(req),
                    [](utils::web::WebResponse res) {
                        if (!res.ok()) log::error("webhook failed: {} code {}", res.errorMessage(), res.code());
                    }
                );
            });
        }
    };