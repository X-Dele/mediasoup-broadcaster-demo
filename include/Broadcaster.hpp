#ifndef BROADCASTER_H
#define BROADCASTER_H

#include "Poco/Timer.h"
#include "ProtooApi.hpp"
#include "WebSocketClient.hpp"
#include "examples/peerconnection/client/main_wnd.h"
#include "mediasoupclient.hpp"
#include "json.hpp"
#include <chrono>
#include <condition_variable>
#include <future>
#include <mutex>
#include <string>
class Broadcaster : public mediasoupclient::SendTransport::Listener,
                    mediasoupclient::RecvTransport::Listener,
                    mediasoupclient::Producer::Listener,
                    mediasoupclient::Consumer::Listener,
                    mediasoupclient::DataProducer::Listener,
                    mediasoupclient::DataConsumer::Listener
{
public:
	struct TimerKiller
	{
		// returns false if killed:
		template<class R, class P>
		bool WaitFor(std::chrono::duration<R, P> const& time) const
		{
			std::unique_lock<std::mutex> lock(m);
			return !cv.wait_for(lock, time, [&] { return terminate; });
		}
		void Kill()
		{
			std::unique_lock<std::mutex> lock(m);
			terminate = true; // Should be modified inside mutex lock.
			cv.notify_all();  // It is safe, and *sometimes* optimal, to do this outside the lock.
		}

	private:
		mutable std::condition_variable cv;
		mutable std::mutex m;
		bool terminate = false;
	};

	/* Virtual methods inherited from SendTransport::Listener. */
public:
	std::future<void> OnConnect(
	  mediasoupclient::Transport* transport, const nlohmann::json& dtlsParameters) override;
	void OnConnectionStateChange(
	  mediasoupclient::Transport* transport, const std::string& connectionState) override;
	std::future<std::string> OnProduce(
	  mediasoupclient::SendTransport* /*transport*/,
	  const std::string& kind,
	  nlohmann::json rtpParameters,
	  const nlohmann::json& appData) override;

	std::future<std::string> OnProduceData(
	  mediasoupclient::SendTransport* transport,
	  const nlohmann::json& sctpStreamParameters,
	  const std::string& label,
	  const std::string& protocol,
	  const nlohmann::json& appData) override;

	/* Virtual methods inherited from Producer::Listener. */
public:
	void OnTransportClose(mediasoupclient::Producer* producer) override;

	/* Virtual methods inherited from DataConsumer::Listener */
public:
	void OnMessage(mediasoupclient::DataConsumer* dataConsumer, const webrtc::DataBuffer& buffer) override;
	void OnConnecting(mediasoupclient::DataConsumer* dataConsumer) override
	{
	}
	void OnClosing(mediasoupclient::DataConsumer* dataConsumer) override
	{
	}
	void OnClose(mediasoupclient::DataConsumer* dataConsumer) override
	{
	}
	void OnOpen(mediasoupclient::DataConsumer* dataConsumer) override
	{
	}
	void OnTransportClose(mediasoupclient::DataConsumer* dataConsumer) override
	{
	}

	/* Virtual methods inherited from DataProducer::Listener */
public:
	void OnOpen(mediasoupclient::DataProducer* dataProducer) override;
	void OnClose(mediasoupclient::DataProducer* dataProducer) override;
	void OnBufferedAmountChange(mediasoupclient::DataProducer* dataProducer, uint64_t size) override;
	void OnTransportClose(mediasoupclient::DataProducer* dataProducer) override;

public:
	void OnTransportClose(mediasoupclient::Consumer* consumer) override;

public:
	void Start();
	void Stop();
	Broadcaster(std::shared_ptr<WebSocketClient> webSocket, std::shared_ptr<MainWindow> window);
	~Broadcaster();

private:
	ProtooApi protooApi;
	std::shared_ptr<MainWindow> window;
	mediasoupclient::Device device;
	mediasoupclient::SendTransport* sendTransport{ nullptr };
	mediasoupclient::RecvTransport* recvTransport{ nullptr };
	mediasoupclient::DataProducer* dataProducer{ nullptr };
	mediasoupclient::DataConsumer* dataConsumer{ nullptr };

	std::string id = std::to_string(rtc::CreateRandomId());
	void onNewConsumer(const nlohmann::json& data);
	std::string baseUrl;
	std::thread sendDataThread;

	struct TimerKiller timerKiller;
	bool verifySsl = true;
	Poco::Timer updateTimer_;

	std::future<void> OnConnectSendTransport(const nlohmann::json& dtlsParameters);
	std::future<void> OnConnectRecvTransport(const nlohmann::json& dtlsParameters);

	void CreateSendTransport();
	void CreateRecvTransport();
	void CreateDataConsumer();
	void onTimer(Poco::Timer&);
};

#endif // STOKER_HPP
