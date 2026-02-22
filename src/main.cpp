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

class MyPlayLayer : public geode::Modify<MyPlayLayer, PlayLayer> {
    static inline std::vector<std::string> roasts;
    static inline std::vector<std::string> congratulations;
    static inline bool roastsLoaded = false;
    static inline bool congratsLoaded = false;

    static void loadRoasts() {
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
            if (!line.empty() && line[0] != '#') roasts.push_back(line);
        }
        file.close();

        if (roasts.empty()) roasts.push_back("bro died at {}% on {}, skill issue 💀");

        roastsLoaded = true;
    }

    static void loadCongratulations() {
        if (congratsLoaded) return;

        auto congratsFile = Mod::get()->getSaveDir() / "congratulations.txt";

        if (!fs::exists(congratsFile)) {
            std::ofstream file(congratsFile);
            file << "u rlly beat it? wow\n";
            file << "no shot u actually did it 💀\n";
            file << "i didnt think u had it in u\n";
            file << "ok ok ok, im impressed ngl\n";
            file << "bet u cant do it again\n";
            file << "...did that just happen?\n";
            file << "W behaviour actually\n";
            file.close();
        }

        std::ifstream file(congratsFile);
        std::string line;
        while (std::getline(file, line)) {
            if (!line.empty() && line[0] != '#') congratulations.push_back(line);
        }
        file.close();

        if (congratulations.empty()) congratulations.push_back("u rlly beat it? wow");

        congratsLoaded = true;
    }

    static std::string getRandomRoast(int percent, const std::string& levelName) {
        loadRoasts();
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, static_cast<int>(roasts.size()) - 1);
        return fmt::format(fmt::runtime(roasts[dis(gen)]), percent, levelName);
    }

    static std::string getRandomCongratulations(const std::string& levelName) {
        loadCongratulations();
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, static_cast<int>(congratulations.size()) - 1);
        return congratulations[dis(gen)];
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
        if (webhookUrl.empty()) return;

        std::string boundary = "----Boundary" + std::to_string(time(nullptr));
        std::string body = "--" + boundary + "\r\nContent-Disposition: form-data; name=\"content\"\r\n\r\n" + content + "\r\n";

        std::ifstream file(imagePath, std::ios::binary | std::ios::ate);
        if (file) {
            auto size = file.tellg();
            file.seekg(0);
            std::vector<char> buf(size);
            file.read(buf.data(), size);

            body += "--" + boundary + "\r\nContent-Disposition: form-data; name=\"file\"; filename=\"death.png\"\r\nContent-Type: image/png\r\n\r\n";
            body.append(buf.data(), size);
            body += "\r\n";
        }

        body += "--" + boundary + "--\r\n";

        web::WebRequest()
            .bodyString(body)
            .header("Content-Type", "multipart/form-data; boundary=" + boundary)
            .post(webhookUrl);
    }

    void sendToDiscordCompletion(const std::string& congratsMsg, const std::string& levelName) {
        auto webhookUrl = Mod::get()->getSettingValue<std::string>("webhook_url");
        if (webhookUrl.empty()) return;

        std::string msg = fmt::format("{} - {}", congratsMsg, levelName);

        web::WebRequest()
            .bodyString(msg)
            .header("Content-Type", "application/json")
            .post(webhookUrl);
    }

public:
    void destroyPlayer(PlayerObject* p0, GameObject* p1) {
        int percent = this->getCurrentPercentInt();
        int min = Mod::get()->getSettingValue<int64_t>("min_percent");
        if (percent >= min && !m_isPracticeMode) {
            captureAndSend(percent, m_level->m_levelName);
        }
        MyPlayLayer::destroyPlayer(p0, p1);
    }

    void levelComplete() {
        std::string name = m_level->m_levelName;
        sendToDiscordCompletion(getRandomCongratulations(name), name);
        MyPlayLayer::levelComplete();
    }
};
// meow meow meow meow meow meow meow meow meow meow meow meow meow meow meow meow meow meow meow meow meow meow meow meow, yes meow    