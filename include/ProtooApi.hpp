#ifndef _PROTOO_API_HPP_
#define _PROTOO_API_HPP_
#include "WebSocketClient.hpp"
#include "json.hpp"
#include <functional>
#include <future>
#include <memory>

class ProtooApi : public WebSocketClient::Listener
{
public:
	ProtooApi(std::shared_ptr<WebSocketClient> webSocket);
	nlohmann::json getRouterRtpCapabilities();
	nlohmann::json createWebRtcTransport(const nlohmann::json& data);
	nlohmann::json connectWebRtcTransport(const nlohmann::json& data);
	nlohmann::json produce(const nlohmann::json& data);
	nlohmann::json produceData(const nlohmann::json& data);
	nlohmann::json join(const nlohmann::json& data);
	nlohmann::json getTransportStats(const nlohmann::json& data);

private:
	nlohmann::json sendAndWaitResponse(const std::string& method, const nlohmann::json& data);
	nlohmann::json createRequest(const std::string& method, const nlohmann::json& data);
	nlohmann::json waitResponse(const nlohmann::json& request);
	void onReceive(const std::string& message) override;

	std::shared_ptr<WebSocketClient> webSocket;
	std::map<uint32_t, std::function<void(const nlohmann::json&)>> handlers;
};
#endif // _PROTOO_API_HPP_