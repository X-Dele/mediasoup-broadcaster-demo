#ifndef _WEB_SOCKET_CLIENT_HPP_
#define _WEB_SOCKET_CLIENT_HPP_
#include <functional>
#include <thread>

#include "Poco/Net/WebSocket.h"
class WebSocketClient
{
public:
	class Listener
	{
	public:
		virtual void onReceive(const std::string& message) = 0;
	};
	WebSocketClient();
	void connect(const std::string& host, const uint32_t& port, const std::string& url);
	void send(const std::string& msg);
	void close();
	void addListener(Listener* listener);

private:
	void startRecv();
	bool exit = false;
	std::thread recvThread;
	Poco::Net::WebSocket* webSocket = nullptr;
	Listener* listener              = nullptr;
};
#endif // _WEB_SOCKET_CLIENT_HPP_