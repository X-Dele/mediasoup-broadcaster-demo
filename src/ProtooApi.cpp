#include "ProtooApi.hpp"
#include <chrono>
#include <iostream>

ProtooApi::ProtooApi(std::shared_ptr<WebSocketClient> webSocket) : webSocket(webSocket)
{
	this->webSocket->addListener(this);
}

nlohmann::json ProtooApi::getRouterRtpCapabilities()
{
	return sendAndWaitResponse("getRouterRtpCapabilities", nlohmann::json::object());
}

nlohmann::json ProtooApi::createWebRtcTransport(const nlohmann::json& data)
{
	return sendAndWaitResponse("createWebRtcTransport", data);
}

nlohmann::json ProtooApi::connectWebRtcTransport(const nlohmann::json& data)
{
	return sendAndWaitResponse("connectWebRtcTransport", data);
}

nlohmann::json ProtooApi::produce(const nlohmann::json& data)
{
	return sendAndWaitResponse("produce", data);
}

nlohmann::json ProtooApi::produceData(const nlohmann::json& data)
{
	return sendAndWaitResponse("produceData", data);
}

nlohmann::json ProtooApi::join(const nlohmann::json& data)
{
	return sendAndWaitResponse("join", data);
}

nlohmann::json ProtooApi::getTransportStats(const nlohmann::json& data)
{
	return sendAndWaitResponse("getTransportStats", data);
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
	nlohmann::json response = nlohmann::json::parse(message);
	auto it                 = response.find("response");
	if (it == response.end())
	{
		return;
	}
	uint32_t response_id = response["id"];
	if (this->handlers.find(response_id) == this->handlers.end())
	{
		return;
	}
	this->handlers[response_id](response);
	this->handlers.erase(response_id);
}