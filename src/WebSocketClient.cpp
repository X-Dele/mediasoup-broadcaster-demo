#include "WebSocketClient.hpp"

#include "Poco/Net/HTTPMessage.h"
#include "Poco/Net/HTTPRequest.h"
#include "Poco/Net/HTTPResponse.h"
#include "Poco/Net/HTTPSClientSession.h"
#include <iostream>
using Poco::Net::HTTPMessage;
using Poco::Net::HTTPRequest;
using Poco::Net::HTTPResponse;
using Poco::Net::HTTPSClientSession;
using Poco::Net::WebSocket;

WebSocketClient::WebSocketClient()
{
}

void WebSocketClient::connect(const std::string& host, const uint32_t& port, const std::string& url)
{
	HTTPSClientSession cs(host, port);
	HTTPRequest request(HTTPRequest::HTTP_GET, url, HTTPMessage::HTTP_1_1);
	request.set("Sec-WebSocket-Protocol", "protoo");
	HTTPResponse response;
	this->webSocket  = new WebSocket(cs, request, response);
	this->recvThread = std::thread(std::bind(&WebSocketClient::startRecv, this));
}

void WebSocketClient::startRecv()
{
	int flags = 0;

	while (!this->exit)
	{
		try
		{
			char receiveBuff[10240] = { 0 };
			std::cout << "Start Receive" << std::endl;

			int len = this->webSocket->receiveFrame(receiveBuff, sizeof(receiveBuff), flags);
			std::cout << "Received bytes: " << len << std::endl;
			std::cout << "Received body: " << receiveBuff << std::endl;
			this->listener->onReceive(receiveBuff);
		}
		catch (std::exception& e)
		{
			std::cout << "Exception " << e.what();
			sleep(1);
		}
	}
}

void WebSocketClient::send(const std::string& msg)
{
	int len = this->webSocket->sendFrame(msg.data(), msg.size(), WebSocket::FRAME_TEXT);
	std::cout << "Sent bytes " << len << std::endl;
	std::cout << "Sent body " << msg << std::endl;
}

void WebSocketClient::close()
{
	this->exit = true;
	this->webSocket->close();
}

void WebSocketClient::addListener(Listener* listener)
{
	this->listener = listener;
}
