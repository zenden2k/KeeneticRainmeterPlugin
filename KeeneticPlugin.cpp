#include <Windows.h>

#include <sstream>
#include <thread>
#include <map>

#include <json/json.h>
#include "Core/Network/NetworkClient.h"
#include "API/RainmeterAPI.h"
#include "Core/Utils/CryptoUtils.h"
#include "Core/Utils/StringUtils.h"

enum class MeasureType
{
    mtDownload,
    mtUpload
};

class Worker;

struct Settings{
    std::string routerUrl;
    std::string login;
    std::string password;
    std::string proxy;
    std::vector<std::string> interfaces;
    int proxyPort = 0;
};

class SettingsLoader
{
public:
    static std::unique_ptr<Settings> loadSettings(void* rm, LPCWSTR routerID, LPCTSTR configFile) {
        WCHAR loginW[256]{};
        WCHAR passwordW[256]{};
        WCHAR urlW[256]{};
        WCHAR proxyW[256]{};
        WCHAR interfaceW[1024]{};

        GetPrivateProfileString(routerID, L"URL", L"http://192.168.1.1", urlW, 256, configFile);
        GetPrivateProfileString(routerID, L"Login", L"admin", loginW, 256, configFile);
        GetPrivateProfileString(routerID, L"Password", L"", passwordW, 256, configFile);
        GetPrivateProfileString(routerID, L"Proxy", L"", proxyW, 256, configFile);
        GetPrivateProfileString(routerID, L"Interface", L"ISP", interfaceW, 1024, configFile);

        if (!lstrlen(passwordW)) {
            RmLog(rm, LOG_ERROR, (std::wstring(L"No password set for ") + routerID + std::wstring(L" in config file ") + configFile).c_str());
            return {};
        }

        std::unique_ptr<Settings> res = std::make_unique<Settings>();

        res->proxyPort = GetPrivateProfileInt(routerID, L"ProxyPort", 8080, configFile);

        res->routerUrl = IuCoreUtils::WstringToUtf8(urlW);
        res->login = IuCoreUtils::WstringToUtf8(loginW);
        res->password = IuCoreUtils::WstringToUtf8(passwordW);
        res->proxy = IuCoreUtils::WstringToUtf8(proxyW);
        std::string interfaces = IuCoreUtils::WstringToUtf8(interfaceW);
        IuStringUtils::Split(interfaces, ",", res->interfaces);
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

    ~Worker() {
        try {
            abort();
        } catch(...) {
            
        }
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
        std::unique_lock<std::mutex> lk(dataMutex_);
        downloadSpeed_.clear();
        uploadSpeed_.clear();
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

    double getUploadSpeed(const std::string& interf) const {
        std::unique_lock<std::mutex> lk(dataMutex_);
        if (!interf.empty()) {
            auto it = uploadSpeed_.find(interf);
            if (it != uploadSpeed_.end()) {
                return it->second;
            }
            return 0.0;
        }
        return uploadSpeed_.empty() ? 0.0: uploadSpeed_.begin()->second;
    }

    double getDownloadSpeed(const std::string& interf) const {
        std::unique_lock<std::mutex> lk(dataMutex_);
        if (!interf.empty()) {
            auto it = downloadSpeed_.find(interf);
            if (it != downloadSpeed_.end()) {
                return it->second;
            }
            return 0.0;
        }
        return downloadSpeed_.empty() ? 0.0: downloadSpeed_.begin()->second;
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
    mutable std::mutex dataMutex_;

    std::map<std::string, std::atomic<double>> uploadSpeed_;
    std::map<std::string, std::atomic<double>> downloadSpeed_;

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
        bool success = false;
        nc_->setUrl(settings_->routerUrl + "/rci/");

        Json::Value rrd(Json::arrayValue);

        for(auto& el: settings_->interfaces) {
            Json::Value rrd1;
            rrd1["name"] = el;
            rrd1["attribute"] = "rxspeed";
            rrd1["detail"] = 0;

            Json::Value rrd2;
            rrd2["name"] = el;
            rrd2["attribute"] = "txspeed";
            rrd2["detail"] = 0;

            rrd.append(rrd1);
            rrd.append(rrd2);
        }

        Json::Value root;
        root["show"]["interface"]["rrd"] = rrd;

        Json::StreamWriterBuilder builder;
        builder["commentStyle"] = "None";
        builder["indentation"] = "   ";
        std::string s = Json::writeString(builder, root);

        nc_->addQueryHeader("Content-Type", "application/json");
        nc_->doPost(s);

        if (nc_->responseCode() == 200) {
            Json::Value val;
            Json::Reader reader;
            if (reader.parse(nc_->responseBody(), val, false)) {
                try {
                    Json::Value& rrdObj = val["show"]["interface"]["rrd"];
                    if (rrdObj.isArray()) {
                        //int index = measure->mt == MeasureType::mtUpload ? 1 : 0;
                        std::unique_lock<std::mutex> lk(dataMutex_);
                        for (size_t i = 0; i < settings_->interfaces.size(); ++i)  {
                            auto& interf = settings_->interfaces[i];
                            int offset = i * 2;
                            Json::Value& status = rrdObj[offset]["status"];

                            if (status.isArray() && status[0]["status"] == "error") {
                                std::wstring msg = std::wstring(L"Server answered with error: ") +
                                    IuCoreUtils::Utf8ToWstring(status[0]["message"].asCString());
                                RmLog(rm_, LOG_ERROR, msg.c_str());
                            }

                            Json::Value& data2 = rrdObj[offset]["data"];

                            if (!data2.empty()) {
                                Json::Value download = *data2.begin();
                                downloadSpeed_[interf] = download["v"].asDouble() / 1000000.0;
                            }

                            Json::Value& status2 = rrdObj[offset+1]["status"];
                            if (status2.isArray() && status2[0]["status"] == "error") {
                                std::wstring msg = std::wstring(L"Server answered with error: ") +
                                    IuCoreUtils::Utf8ToWstring(status2[0]["message"].asCString());
                                RmLog(rm_, LOG_ERROR, msg.c_str());
                            }

                            Json::Value& data3 = rrdObj[offset+1]["data"];
                            if (!data3.empty()) {
                                Json::Value upload = *data3.begin();
                                uploadSpeed_[interf] = upload["v"].asDouble() / 1000000.0;
                            }
                        }
                        success = true;
                    }
                } catch (const std::exception& ex) {
                    RmLog(rm_, LOG_ERROR, IuCoreUtils::Utf8ToWstring(ex.what()).c_str());
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

        if (!success) {
            std::unique_lock<std::mutex> lk(dataMutex_);
            downloadSpeed_.clear();
            uploadSpeed_.clear();
        }
    }
};

struct Measure {
    MeasureType mt = MeasureType::mtDownload;
    void* rm = nullptr;
    std::wstring routerID;
    std::string interf;
    std::shared_ptr<Worker> worker;
};

std::map<std::wstring,std::weak_ptr<Worker>> workers;

PLUGIN_EXPORT void Initialize(void** data, void* rm) {
    auto* measure = new Measure;
    *data = measure;
    LPCWSTR rmDataFile = RmGetSettingsFile();
    LPCWSTR routerID = RmReadString(rm, L"Router", L"KeeneticPlugin");
    std::shared_ptr<Worker> worker = workers[routerID].lock();
    if (!worker) {
        std::shared_ptr<Settings> settings = SettingsLoader::loadSettings(rm, routerID, rmDataFile);
        if (!settings) {
            return;
        }
        workers[routerID] = worker = std::make_shared<Worker>(rm, settings);
        worker->start();
    }
    measure->worker = worker;
    measure->routerID = routerID;
}

PLUGIN_EXPORT void Reload(void* data, void* rm, double* maxValue) {
    auto* measure = static_cast<Measure*>(data);

    LPCWSTR value = RmReadString(rm, L"Type", L"download");

    if (value) {
        std::wstring val = value;
        measure->mt = val == L"upload" ? MeasureType::mtUpload : MeasureType::mtDownload;
    }

    LPCWSTR interf = RmReadString(rm, L"Interface", L"");
    if (interf) {
        measure->interf = IuCoreUtils::WstringToUtf8(interf);
    }
}

PLUGIN_EXPORT double Update(void* data) {
    auto* measure = static_cast<Measure*>(data);
    if (!measure->worker) {
        return {};
    }
    return measure->mt == MeasureType::mtDownload ? measure->worker->getDownloadSpeed(measure->interf)
        : measure->worker->getUploadSpeed(measure->interf);
}

PLUGIN_EXPORT void Finalize(void* data) {
    auto* measure = static_cast<Measure*>(data);
    // Worker belongs to at least two "Measures".
    // Worker thread will exit on destruction, no need to call here.
    // TODO: Reload settings on skin refresh
    /*
     * if (measure->worker) {
        measure->worker->abort();
    }*/
    delete measure;
}
