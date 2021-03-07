#include <Windows.h>
#include <json/json.h>
#include "Core/Network/NetworkClient.h"
#include "API/RainmeterAPI.h"
#include "Core/Utils/CryptoUtils.h"

enum class MeasureType {
  mtDownload, mtUpload   
};

LPCWSTR rmDataFile = nullptr;

struct Measure
{
	NetworkClient nc;
	bool authenticated;
	MeasureType mt;
	std::string routerUrl;
	
	Measure() {
		authenticated = false;
		mt = MeasureType::mtDownload;
	}
};

PLUGIN_EXPORT void Initialize(void** data, void* rm)
{
    auto* measure = new Measure;
	*data = measure;


	if (rmDataFile == nullptr) { 
		rmDataFile = RmGetSettingsFile(); 
	}

	WCHAR loginW[256]{};
	WCHAR passwordW[256]{};
	WCHAR urlW[256]{};

	GetPrivateProfileString(L"KeeneticPlugin", L"URL", L"http://192.168.1.1", urlW, 256, rmDataFile);
	GetPrivateProfileString(L"KeeneticPlugin", L"Login", L"admin", loginW, 256, rmDataFile);
	GetPrivateProfileString(L"KeeneticPlugin", L"Password", L"", passwordW, 256, rmDataFile);

	measure->routerUrl = IuCoreUtils::WstringToUtf8(urlW);
	std::string login = IuCoreUtils::WstringToUtf8(loginW);
	std::string password = IuCoreUtils::WstringToUtf8(passwordW);
	if (password.empty()) {
		RmLog(rm, LOG_ERROR, (std::wstring(L"No password set for KeeneticPlugin in config file ") + rmDataFile).c_str());
		return;
	}
	NetworkClient* nc = &measure->nc;
	//nc->setProxy("127.0.0.1", 8866, CURLPROXY_HTTP);

	nc->doGet(measure->routerUrl + "/auth");

	std::string challenge = nc->responseHeaderByName("X-NDM-Challenge");

	
	std::string realm = nc->responseHeaderByName("X-NDM-Realm");
	
	nc->setUrl(measure->routerUrl + "/auth");

	std::string hash = IuCoreUtils::CryptoUtils::CalcSHA256HashFromString(challenge + 
		IuCoreUtils::CryptoUtils::CalcMD5HashFromString(login + ":" + realm + ":" + password)
	);


	Json::Value val;
	val["login"] = login;
	val["password"] = hash;

	std::ostringstream stream;
	stream << val;


	//std::string s = R"({"login":"admin","password":")" +  + "\"}";
	nc->addQueryHeader("Content-Type", "application/json");
	nc->doPost(stream.str());

	if (nc->responseCode() != 200) {
		RmLog(rm, LOG_ERROR, L"Authentication failed on router");
	} else {
		measure->authenticated = true;
	}
}

PLUGIN_EXPORT void Reload(void* data, void* rm, double* maxValue)
{
    auto* measure = static_cast<Measure*>(data);

	LPCWSTR value = RmReadString(rm, L"Type", L"download");

	if (value) {
		std::wstring val = value;
		measure->mt = val == L"upload" ? MeasureType::mtUpload : MeasureType::mtDownload;
	}
}

PLUGIN_EXPORT double Update(void* data)
{
	auto* measure = static_cast<Measure*>(data);

	if (!measure->authenticated) {
		return 0.0;
	}
	measure->nc.setUrl(measure->routerUrl + "/rci/");
	std::string s = R"({"show":{"interface":{"rrd":[{"name":"ISP","attribute":"rxspeed","detail":0},{"name":"ISP","attribute":"txspeed","detail":0}]}}})";
	measure->nc.addQueryHeader("Content-Type", "application/json");
    measure->nc.doPost(s);
	

	Json::Value val;
	Json::Reader reader;
	if (reader.parse(measure->nc.responseBody(), val, false) ) {
		Json::Value rrd = val["show"]["interface"]["rrd"];
		int index = measure->mt == MeasureType::mtUpload ? 1 : 0;
		Json::Value data2 = rrd[index]["data"];
		if (!data2.empty()) {
			Json::Value download = *data2.begin();
			double dlSpeed = download["v"].asDouble();
			return dlSpeed / 1048576.0;
		}
	}


	return 0;
}

PLUGIN_EXPORT void Finalize(void* data)
{
	auto* measure = static_cast<Measure*>(data);
	delete measure;
}
