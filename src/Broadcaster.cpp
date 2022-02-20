#include "Broadcaster.hpp"
#include "MediaStreamTrackFactory.hpp"
#include "mediasoupclient.hpp"
#include "json.hpp"
#include <chrono>
#include <cpr/cpr.h>
#include <cstdlib>
#include <ctime>
#include <functional>
#include <iostream>
#include <string>
#include <thread>

using json = nlohmann::json;

Broadcaster::~Broadcaster()
{
	this->Stop();
}

Broadcaster::Broadcaster(std::shared_ptr<WebSocketClient> webSocket, std::shared_ptr<MainWindow> window)
  : protooApi(webSocket), window(window), updateTimer_(0, 30 * 1000)
{
	protooApi.addRequestHandler(
	  "newConsumer", std::bind(&Broadcaster::onNewConsumer, this, std::placeholders::_1));
}

void Broadcaster::onNewConsumer(const nlohmann::json& data)
{
	std::string id               = data["id"];
	std::string producerId       = data["producerId"];
	std::string kind             = data["kind"];
	nlohmann::json rtpParameters = data["rtpParameters"];

	nlohmann::json appData = data["appData"];

	auto* consumer = this->recvTransport->Consume(this, id, producerId, kind, &rtpParameters, appData);
	if (kind == "video")
	{
		auto* videoTrack = static_cast<webrtc::VideoTrackInterface*>(consumer->GetTrack());
		this->window->StopRemoteRenderer();
		this->window->StartRemoteRenderer(videoTrack);
	}
}

void Broadcaster::OnTransportClose(mediasoupclient::Producer* /*producer*/)
{
	std::cout << "[INFO] Broadcaster::OnTransportClose()" << std::endl;
}

void Broadcaster::OnTransportClose(mediasoupclient::DataProducer* /*dataProducer*/)
{
	std::cout << "[INFO] Broadcaster::OnTransportClose()" << std::endl;
}

void Broadcaster::OnTransportClose(mediasoupclient::Consumer* /*dataProducer*/)
{
	std::cout << "[INFO] Broadcaster::OnTransportClose()" << std::endl;
}

/* Transport::Listener::OnConnect
 *
 * Fired for the first Transport::Consume() or Transport::Produce().
 * Update the already created remote transport with the local DTLS parameters.
 */
std::future<void> Broadcaster::OnConnect(mediasoupclient::Transport* transport, const json& dtlsParameters)
{
	std::cout << "[INFO] Broadcaster::OnConnect()" << std::endl;
	// std::cout << "[INFO] dtlsParameters: " << dtlsParameters.dump(4) << std::endl;

	if (transport->GetId() == this->sendTransport->GetId())
	{
		return this->OnConnectSendTransport(dtlsParameters);
	}
	else if (transport->GetId() == this->recvTransport->GetId())
	{
		return this->OnConnectRecvTransport(dtlsParameters);
	}
	else
	{
		std::promise<void> promise;

		promise.set_exception(std::make_exception_ptr("Unknown transport requested to connect"));

		return promise.get_future();
	}
}

std::future<void> Broadcaster::OnConnectSendTransport(const json& dtlsParameters)
{
	std::promise<void> promise;

	/* clang-format off */
	json body =
	{
		{ "transportId", this->sendTransport->GetId() },
		{ "dtlsParameters", dtlsParameters }
	};
	/* clang-format on */
	this->protooApi.invoke("connectWebRtcTransport", body);
	promise.set_value();

	return promise.get_future();
}

std::future<void> Broadcaster::OnConnectRecvTransport(const json& dtlsParameters)
{
	std::promise<void> promise;

	/* clang-format off */
	json body =
	{
		{ "transportId", this->recvTransport->GetId() },
		{ "dtlsParameters", dtlsParameters }
	};
	/* clang-format on */
	this->protooApi.invoke("connectWebRtcTransport", body);
	promise.set_value();

	return promise.get_future();
}

/*
 * Transport::Listener::OnConnectionStateChange.
 */
void Broadcaster::OnConnectionStateChange(
  mediasoupclient::Transport* /*transport*/, const std::string& connectionState)
{
	std::cout << "[INFO] Broadcaster::OnConnectionStateChange() [connectionState:" << connectionState
	          << "]" << std::endl;

	if (connectionState == "failed")
	{
		Stop();
		std::exit(0);
	}
}

/* Producer::Listener::OnProduce
 *
 * Fired when a producer needs to be created in mediasoup.
 * Retrieve the remote producer ID and feed the caller with it.
 */
std::future<std::string> Broadcaster::OnProduce(
  mediasoupclient::SendTransport* /*transport*/,
  const std::string& kind,
  json rtpParameters,
  const json& /*appData*/)
{
	std::cout << "[INFO] Broadcaster::OnProduce()" << std::endl;
	// std::cout << "[INFO] rtpParameters: " << rtpParameters.dump(4) << std::endl;

	std::promise<std::string> promise;

	/* clang-format off */
	json body =
	{
		{ "transportId", this->sendTransport->GetId() },
		{ "kind",          kind          },
		{ "rtpParameters", rtpParameters }
	};
	/* clang-format on */
	auto response = this->protooApi.invoke("produce", body);
	auto it       = response.find("id");
	if (it == response.end() || !it->is_string())
	{
		promise.set_exception(std::make_exception_ptr("'id' missing in response"));
	}

	promise.set_value((*it).get<std::string>());

	return promise.get_future();
}

/* Producer::Listener::OnProduceData
 *
 * Fired when a data producer needs to be created in mediasoup.
 * Retrieve the remote producer ID and feed the caller with it.
 */
std::future<std::string> Broadcaster::OnProduceData(
  mediasoupclient::SendTransport* /*transport*/,
  const json& sctpStreamParameters,
  const std::string& label,
  const std::string& protocol,
  const json& /*appData*/)
{
	std::cout << "[INFO] Broadcaster::OnProduceData()" << std::endl;
	// std::cout << "[INFO] rtpParameters: " << rtpParameters.dump(4) << std::endl;

	std::promise<std::string> promise;

	/* clang-format off */
	json body =
    {
				{ "transportId", this->sendTransport->GetId() },
        { "label"                , label },
        { "protocol"             , protocol },
        { "sctpStreamParameters" , sctpStreamParameters }
		// { "appData"				 , "someAppData" }
	};
	/* clang-format on */
	auto response = this->protooApi.invoke("produceData", body);
	auto it       = response.find("id");
	if (it == response.end() || !it->is_string())
	{
		promise.set_exception(std::make_exception_ptr("'id' missing in response"));
	}

	promise.set_value((*it).get<std::string>());

	return promise.get_future();
}

void Broadcaster::Start()
{
	std::cout << "[INFO] creating Broadcaster..." << std::endl;
	json routerRtpCapabilities = this->protooApi.invoke("getRouterRtpCapabilities");
	std::cout << "routerRtpCapabilities: " << routerRtpCapabilities.dump() << std::endl;
	std::cout << "[INFO] Broadcaster::Start()" << std::endl;

	// Load the device.
	this->device.Load(routerRtpCapabilities);
	json sctpCapabilities = this->device.GetSctpCapabilities();
	std::cout << "[INFO] creating mediasoup send WebRtcTransport..." << std::endl;

	this->CreateSendTransport();
	this->CreateRecvTransport();
	updateTimer_.start(Poco::TimerCallback<Broadcaster>(*this, &Broadcaster::onTimer));
}

void Broadcaster::onTimer(Poco::Timer&)
{
	json body = { { "transportId", this->sendTransport->GetId() } };
	this->protooApi.invoke("getTransportStats", body);
}

void Broadcaster::CreateDataConsumer()
{
	const std::string& dataProducerId = this->dataProducer->GetId();

	/* clang-format off */
	json body =
	{
		{ "dataProducerId", dataProducerId }
	};
	/* clang-format on */
	// create server data consumer
	nlohmann::json dataInfo = this->protooApi.invoke("newDataConsumer", body);

	auto dataConsumerId = dataInfo["id"].get<std::string>();

	if (dataInfo.find("streamId") == dataInfo.end())
	{
		std::cerr << "[ERROR] 'streamId' missing in dataInfo" << std::endl;
		return;
	}
	auto streamId = dataInfo["streamId"].get<uint16_t>();

	// Create client consumer.
	this->dataConsumer = this->recvTransport->ConsumeData(
	  this, dataConsumerId, dataProducerId, streamId, "chat", "", nlohmann::json());
}

void Broadcaster::CreateSendTransport()
{
	std::cout << "[INFO] creating SendTransport..." << std::endl;
	json sctpCapabilities = this->device.GetSctpCapabilities();

	nlohmann::json data          = nlohmann::json::object();
	data["forceTcp"]             = false;
	data["producing"]            = true;
	data["consuming"]            = false;
	data["sctpCapabilities"]     = sctpCapabilities;
	nlohmann::json transportInfo = this->protooApi.invoke("createWebRtcTransport", data);
	std::cout << "transportInfo: " << transportInfo.dump() << std::endl;
	auto sendTransportId = transportInfo["id"].get<std::string>();

	this->sendTransport = this->device.CreateSendTransport(
	  this,
	  sendTransportId,
	  transportInfo["iceParameters"],
	  transportInfo["iceCandidates"],
	  transportInfo["dtlsParameters"],
	  transportInfo["sctpParameters"]);
	json rtpCapabilities = this->device.GetRtpCapabilities();
	json device = { { "flag", "chrome" }, { "name", "Chrome" }, { "version", "96.0.4664.110" } };
	json body   = { { "displayName", "Pincurchin" },
                { "device", device },
                { "rtpCapabilities", rtpCapabilities },
                { "sctpCapabilities", sctpCapabilities } };
	this->protooApi.invoke("join", body);
	///////////////////////// Create Audio Producer //////////////////////////

	if (this->device.CanProduce("audio"))
	{
		auto audioTrack = createAudioTrack(std::to_string(rtc::CreateRandomId()));

		/* clang-format off */
		json codecOptions = {
			{ "opusStereo", true },
			{ "opusDtx",		true }
		};
		/* clang-format on */

		this->sendTransport->Produce(this, audioTrack, nullptr, &codecOptions, nullptr);
	}
	else
	{
		std::cerr << "[WARN] cannot produce audio" << std::endl;
	}

	///////////////////////// Create Video Producer //////////////////////////

	if (this->device.CanProduce("video"))
	{
		auto videoTrack = createSquaresVideoTrack(std::to_string(rtc::CreateRandomId()));
		this->window->StartLocalRenderer(videoTrack);
		if (true)
		{
			std::vector<webrtc::RtpEncodingParameters> encodings;
			encodings.emplace_back(webrtc::RtpEncodingParameters());
			encodings.emplace_back(webrtc::RtpEncodingParameters());
			encodings.emplace_back(webrtc::RtpEncodingParameters());

			this->sendTransport->Produce(this, videoTrack, &encodings, nullptr, nullptr);
		}
		else
		{
			this->sendTransport->Produce(this, videoTrack, nullptr, nullptr, nullptr);
		}
	}
	else
	{
		std::cerr << "[WARN] cannot produce video" << std::endl;

		return;
	}

	///////////////////////// Create Data Producer //////////////////////////

	this->dataProducer = sendTransport->ProduceData(this);

	uint32_t intervalSeconds = 10;
	std::thread([this, intervalSeconds]() {
		bool run = true;
		while (run)
		{
			std::chrono::system_clock::time_point p = std::chrono::system_clock::now();
			std::time_t t                           = std::chrono::system_clock::to_time_t(p);
			std::string s                           = std::ctime(&t);
			auto dataBuffer                         = webrtc::DataBuffer(s);
			std::cout << "[INFO] sending chat data: " << s << std::endl;
			this->dataProducer->Send(dataBuffer);
			run = timerKiller.WaitFor(std::chrono::seconds(intervalSeconds));
		}
	}).detach();
}

void Broadcaster::CreateRecvTransport()
{
	std::cout << "[INFO] creating mediasoup recv WebRtcTransport..." << std::endl;

	json sctpCapabilities = this->device.GetSctpCapabilities();
	std::cout << "[INFO] creating RecvTransport..." << std::endl;
	nlohmann::json data          = nlohmann::json::object();
	data["forceTcp"]             = false;
	data["producing"]            = false;
	data["consuming"]            = true;
	data["sctpCapabilities"]     = sctpCapabilities;
	nlohmann::json transportInfo = this->protooApi.invoke("createWebRtcTransport", data);
	std::cout << "transportInfo: " << transportInfo.dump() << std::endl;
	auto recvTransportId = transportInfo["id"].get<std::string>();

	std::cout << "[INFO] creating RecvTransport..." << std::endl;

	auto sctpParameters = transportInfo["sctpParameters"];

	this->recvTransport = this->device.CreateRecvTransport(
	  this,
	  recvTransportId,
	  transportInfo["iceParameters"],
	  transportInfo["iceCandidates"],
	  transportInfo["dtlsParameters"],
	  sctpParameters);

	// this->CreateDataConsumer();
}

void Broadcaster::OnMessage(mediasoupclient::DataConsumer* dataConsumer, const webrtc::DataBuffer& buffer)
{
	std::cout << "[INFO] Broadcaster::OnMessage()" << std::endl;
	if (dataConsumer->GetLabel() == "chat")
	{
		std::string s = std::string(buffer.data.data<char>(), buffer.data.size());
		std::cout << "[INFO] received chat data: " + s << std::endl;
	}
}

void Broadcaster::Stop()
{
	std::cout << "[INFO] Broadcaster::Stop()" << std::endl;

	this->timerKiller.Kill();

	if (this->recvTransport)
	{
		recvTransport->Close();
	}

	if (this->sendTransport)
	{
		sendTransport->Close();
	}

	cpr::DeleteAsync(
	  cpr::Url{ this->baseUrl + "/broadcasters/" + this->id }, cpr::VerifySsl{ verifySsl })
	  .get();
}

void Broadcaster::OnOpen(mediasoupclient::DataProducer* /*dataProducer*/)
{
	std::cout << "[INFO] Broadcaster::OnOpen()" << std::endl;
}

void Broadcaster::OnClose(mediasoupclient::DataProducer* /*dataProducer*/)
{
	std::cout << "[INFO] Broadcaster::OnClose()" << std::endl;
}

void Broadcaster::OnBufferedAmountChange(mediasoupclient::DataProducer* /*dataProducer*/, uint64_t /*size*/)
{
	std::cout << "[INFO] Broadcaster::OnBufferedAmountChange()" << std::endl;
}
