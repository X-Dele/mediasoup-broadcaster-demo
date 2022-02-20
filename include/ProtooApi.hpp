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
	nlohmann::json invoke(std::string method, const nlohmann::json& data = nlohmann::json::object());
	void addRequestHandler(const std::string& method, const std::function<void(nlohmann::json)>& handler);

private:
	nlohmann::json createResponse(const uint32_t& request_id);
	void sendResponse(const uint32_t& request_id);
	nlohmann::json sendAndWaitResponse(const std::string& method, const nlohmann::json& data);
	nlohmann::json createRequest(const std::string& method, const nlohmann::json& data);
	nlohmann::json waitResponse(const nlohmann::json& request);
	void onReceive(const std::string& message) override;

	std::shared_ptr<WebSocketClient> webSocket;
	std::map<uint32_t, std::function<void(const nlohmann::json&)>> handlers;
	std::map<std::string, std::function<void(const nlohmann::json&)>> requestHandlers;
};
#endif // _PROTOO_API_HPP_