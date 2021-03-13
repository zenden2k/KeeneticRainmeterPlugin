#include <Windows.h>

#include <thread>

#include <json/json.h>
#include "Core/Network/NetworkClient.h"
#include "API/RainmeterAPI.h"
#include "Core/Utils/CryptoUtils.h"

enum class MeasureType
{
    mtDownload,
    mtUpload
};

LPCWSTR rmDataFile = nullptr;
class Worker;

struct Settings{
    std::string routerUrl;
    std::string login;
    std::string password;
    std::string proxy;
    int proxyPort = 0;
};

class SettingsLoader
{
public:
    static std::unique_ptr<Settings> loadSettings(void* rm, LPCTSTR configFile) {
        WCHAR loginW[256]{};
        WCHAR passwordW[256]{};
        WCHAR urlW[256]{};
        WCHAR proxyW[256]{};

        GetPrivateProfileString(L"KeeneticPlugin", L"URL", L"http://192.168.1.1", urlW, 256, rmDataFile);
        GetPrivateProfileString(L"KeeneticPlugin", L"Login", L"admin", loginW, 256, rmDataFile);
        GetPrivateProfileString(L"KeeneticPlugin", L"Password", L"", passwordW, 256, rmDataFile);
        GetPrivateProfileString(L"KeeneticPlugin", L"Proxy", L"", proxyW, 256, rmDataFile);

        if (!lstrlen(passwordW)) {
            RmLog(rm, LOG_ERROR, (std::wstring(L"No password set for KeeneticPlugin in config file ") + rmDataFile).c_str());
            return {};
        }

        std::unique_ptr<Settings> res = std::make_unique<Settings>();

        res->proxyPort = GetPrivateProfileInt(L"KeeneticPlugin", L"ProxyPort", 8080, rmDataFile);

        res->routerUrl = IuCoreUtils::WstringToUtf8(urlW);
        res->login = IuCoreUtils::WstringToUtf8(loginW);
        res->password = IuCoreUtils::WstringToUtf8(passwordW);
        res->proxy = IuCoreUtils::WstringToUtf8(proxyW);

        return res;
    }
};

class Worker
{
public:
    Worker(void *rm, std::shared_ptr<Settings> settings) {
        stopSignal = false;
        rm_ = rm;
        settings_ = std::move(settings);
    }

    void start() {
        if (started_) {
            return;
        }
        thread_ = std::thread(&Worker::run, this);
    }

    void run() {
        nc_ = std::make_unique<NetworkClient>();
        if (!settings_->proxy.empty() && settings_->proxyPort > 0) {
            nc_->setProxy(settings_->proxy, settings_->proxyPort, CURLPROXY_HTTP);
        }

        nc_->setCurlOptionInt(CURLOPT_CONNECTTIMEOUT, 5);

        while (!stopSignal) {
            if (!authenticated) {
                if (!authenticate()) {
                    Sleep(1000);
                    continue;
                }
            }
            if (stopSignal) {
                break;
            }
            loadData();
            Sleep(1000);
        }
        logout();
        nc_ = nullptr;
    }

    void abort() {
        stopSignal = true;
        if (thread_.joinable()) {
            thread_.join();
        }
    }

    void setProxyPort(int port) {
        proxyPort_ = port;
    }

    double getUploadSpeed() const {
        return uploadSpeed_;
    }

    double getDownloadSpeed() const {
        return downloadSpeed_;
    }

private:
    std::shared_ptr<Settings> settings_;
    void* rm_;
    bool started_ = false;
    std::thread thread_;
    std::unique_ptr<NetworkClient> nc_;
    ULONGLONG lastAuthErrorTime_ = 0;
    
    int proxyPort_ = 0;
    std::atomic_bool stopSignal = false;
    std::mutex dataMutex_;

    std::atomic<double> uploadSpeed_ = 0.0;
    std::atomic<double> downloadSpeed_ = 0.0;

    bool authenticated = false;

    bool authenticate() {
        if (lastAuthErrorTime_ && (GetTickCount64() - lastAuthErrorTime_ < 10000)) {
            return false;
        }
        nc_->doGet(settings_->routerUrl + "/auth");

        std::string challenge = nc_->responseHeaderByName("X-NDM-Challenge");


        std::string realm = nc_->responseHeaderByName("X-NDM-Realm");

        if (challenge.empty() || realm.empty()) {
            std::wstring msg = std::wstring(L"Failed to obtain realm token. Response code : ")
                + std::to_wstring(nc_->responseCode()) + L", CURL error: " + IuCoreUtils::Utf8ToWstring(nc_->errorString());
            RmLog(rm_, LOG_ERROR, msg.c_str());
            return false;
        }
        nc_->setUrl(settings_->routerUrl + "/auth");

        std::string hash = IuCoreUtils::CryptoUtils::CalcSHA256HashFromString(challenge +
            IuCoreUtils::CryptoUtils::CalcMD5HashFromString(settings_->login + ":" + realm + ":" + settings_->password)
        );

        Json::Value val;
        val["login"] = settings_->login;
        val["password"] = hash;

        std::ostringstream stream;
        stream << val;

        nc_->addQueryHeader("Content-Type", "application/json");
        nc_->doPost(stream.str());

        if (nc_->responseCode() != 200) {
            std::wstring msg = std::wstring(L"Authentication failed on router. Response code : ")
                + std::to_wstring(nc_->responseCode()) + L", CURL error: " + IuCoreUtils::Utf8ToWstring(nc_->errorString());
            RmLog(rm_, LOG_ERROR, msg.c_str());
            lastAuthErrorTime_ = GetTickCount64();
            return false;
        }

        authenticated = true;
        return true;
    }

    bool logout() {
        nc_->setUrl(settings_->routerUrl + "/auth");
        nc_->setMethod("DELETE");
        nc_->doPost({});

        return nc_->responseCode() == 200;
    }

    void loadData() {
        nc_->setUrl(settings_->routerUrl + "/rci/");
        std::string s =
            R"({"show":{"interface":{"rrd":[{"name":"ISP","attribute":"rxspeed","detail":0},{"name":"ISP","attribute":"txspeed","detail":0}]}}})";
        nc_->addQueryHeader("Content-Type", "application/json");
        nc_->doPost(s);

        if (nc_->responseCode() == 200) {
            Json::Value val;
            Json::Reader reader;
            if (reader.parse(nc_->responseBody(), val, false)) {
                Json::Value rrd = val["show"]["interface"]["rrd"];
                //int index = measure->mt == MeasureType::mtUpload ? 1 : 0;
                std::unique_lock<std::mutex> lk(dataMutex_);
                Json::Value data2 = rrd[0]["data"];
                if (!data2.empty()) {
                    Json::Value download = *data2.begin();
                    downloadSpeed_ = download["v"].asDouble() / 1000000.0;
                }

                Json::Value data3 = rrd[1]["data"];
                if (!data3.empty()) {
                    Json::Value upload = *data3.begin();
                    uploadSpeed_ = upload["v"].asDouble() / 1000000.0;
                }
            }
        }
        else {
            if (nc_->responseCode() == 401) {
                authenticated = false;
            }
            std::wstring msg = std::wstring(L"Failed to get data from router. Response code: ")
                + std::to_wstring(nc_->responseCode()) + L", CURL error: " + IuCoreUtils::Utf8ToWstring(nc_->errorString());

            RmLog(rm_, LOG_ERROR, msg.c_str());
        }
    }
};

struct Measure {
    MeasureType mt = MeasureType::mtDownload;
    void* rm = nullptr;
    std::shared_ptr<Worker> worker;
};
std::shared_ptr<Worker> worker;


PLUGIN_EXPORT void Initialize(void** data, void* rm) {

    auto* measure = new Measure;
    *data = measure;

    if (rmDataFile == nullptr) {
        rmDataFile = RmGetSettingsFile();
    }

    if (!worker) {
        std::shared_ptr<Settings> settings = SettingsLoader::loadSettings(rm, rmDataFile);
        if (!settings) {
            return;
        }
        worker = std::make_shared<Worker>(rm, settings);
        worker->start();
    }
    measure->worker = worker;
}

PLUGIN_EXPORT void Reload(void* data, void* rm, double* maxValue) {
    auto* measure = static_cast<Measure*>(data);

    LPCWSTR value = RmReadString(rm, L"Type", L"download");

    if (value) {
        std::wstring val = value;
        measure->mt = val == L"upload" ? MeasureType::mtUpload : MeasureType::mtDownload;
    }
}

PLUGIN_EXPORT double Update(void* data) {
    auto* measure = static_cast<Measure*>(data);
    if (!measure->worker) {
        return {};
    }
    return measure->mt == MeasureType::mtDownload ? measure->worker->getDownloadSpeed() : measure->worker->getUploadSpeed();
}

PLUGIN_EXPORT void Finalize(void* data) {
    auto* measure = static_cast<Measure*>(data);
    measure->worker->abort();
    delete measure;
    worker.reset();
}
