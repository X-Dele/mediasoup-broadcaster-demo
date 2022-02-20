#include "ProtooApi.hpp"
#include <chrono>
#include <iostream>

ProtooApi::ProtooApi(std::shared_ptr<WebSocketClient> webSocket) : webSocket(webSocket)
{
	this->webSocket->addListener(this);
}

nlohmann::json ProtooApi::invoke(std::string method, const nlohmann::json& data)
{
	return sendAndWaitResponse(method, data);
}

nlohmann::json ProtooApi::createRequest(const std::string& method, const nlohmann::json& data)
{
	nlohmann::json request;
	request["request"] = true;
	auto timeNow       = std::chrono::duration_cast<std::chrono::milliseconds>(
    std::chrono::system_clock::now().time_since_epoch());
	uint32_t request_id = timeNow.count();
	request["id"]       = request_id;
	request["method"]   = method;
	request["data"]     = data;
	return request;
}

nlohmann::json ProtooApi::waitResponse(const nlohmann::json& request)
{
	uint32_t request_id = request["id"];
	this->webSocket->send(request.dump());
	auto promise = std::make_shared<std::promise<nlohmann::json>>();
	auto callback = [promise](const nlohmann::json& response) { promise->set_value(response["data"]); };
	this->handlers.emplace(request_id, callback);

	std::future<nlohmann::json> future = promise->get_future();
	std::chrono::milliseconds span(3000);
	if (future.wait_for(span) == std::future_status::timeout)
	{
		std::cout << "timeout";
		return {};
	}

	return future.get();
}

nlohmann::json ProtooApi::sendAndWaitResponse(const std::string& method, const nlohmann::json& data)
{
	auto request = createRequest(method, data);
	return waitResponse(request);
}

void ProtooApi::onReceive(const std::string& message)
{
	if (message.empty())
	{
		return;
	}
	nlohmann::json messageJson = nlohmann::json::parse(message);
	auto responseIt            = messageJson.find("response");
	if (responseIt != messageJson.end())
	{
		uint32_t response_id = messageJson["id"];
		if (this->handlers.find(response_id) == this->handlers.end())
		{
			return;
		}
		this->handlers[response_id](messageJson);
		this->handlers.erase(response_id);
	}

	auto requestIt = messageJson.find("request");
	if (requestIt != messageJson.end())
	{
		std::string method = messageJson["method"];
		if (this->requestHandlers.find(method) == this->requestHandlers.end())
		{
			return;
		}
		uint32_t request_id = messageJson["id"];
		this->requestHandlers[method](messageJson["data"]);
		sendResponse(request_id);
	}
}

void ProtooApi::sendResponse(const uint32_t& request_id)
{
	nlohmann::json response = createResponse(request_id);
	this->webSocket->send(response.dump());
}

nlohmann::json ProtooApi::createResponse(const uint32_t& request_id)
{
	nlohmann::json response;
	response["response"] = true;

	response["id"]   = request_id;
	response["ok"]   = true;
	response["data"] = nlohmann::json::object();
	return response;
}

void ProtooApi::addRequestHandler(
  const std::string& method, const std::function<void(nlohmann::json)>& handler)
{
	this->requestHandlers.emplace(method, handler);
}