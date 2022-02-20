#include "Broadcaster.hpp"

#include "Poco/Net/AcceptCertificateHandler.h"
#include "Poco/Net/SSLManager.h"
#include "Poco/SharedPtr.h"
#include "ProtooApi.hpp"
#include "WebSocketClient.hpp"
#include "mediasoupclient.hpp"
#include "wnd.h"
#include <cpr/cpr.h>
#include <csignal>
#include <cstdlib>
#include <glib.h>
#include <gtk/gtk.h>
#include <iostream>
#include <stdio.h>
#include <string>

using Poco::Net::Context;

using json = nlohmann::json;

void signalHandler(int signum)
{
	std::cout << "[INFO] interrupt signal (" << signum << ") received" << std::endl;

	std::cout << "[INFO] leaving!" << std::endl;

	std::exit(signum);
}

void initializeSSL()
{
	Poco::Net::initializeSSL();

	Poco::SharedPtr<Poco::Net::InvalidCertificateHandler> pCert =
	  new Poco::Net::AcceptCertificateHandler(false); // ask the user via console
	Context::Ptr pContext = new Context(
	  Context::CLIENT_USE, "", "", "", Context::VERIFY_NONE, 9, false, "ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH");
	Poco::Net::SSLManager::instance().initializeClient(0, pCert, pContext);
}

int main(int argc, char* argv[])
{
	gtk_init(&argc, &argv);
	std::shared_ptr<GtkMainWnd> wnd =
	  std::make_shared<GtkMainWnd>("192.168.43.250", 4443, false, false);
	wnd->Create();
	initializeSSL();
	// Register signal SIGINT and signal handler.
	signal(SIGINT, signalHandler);
	std::shared_ptr<WebSocketClient> socket = std::make_shared<WebSocketClient>();
	socket->connect("192.168.43.250", 4443, "/?roomId=1234&peerId=12411");

	// Set RTC logging severity.

	rtc::LogMessage::LogToDebug(rtc::LoggingSeverity::LS_INFO);

	auto logLevel = mediasoupclient::Logger::LogLevel::LOG_DEBUG;
	mediasoupclient::Logger::SetLogLevel(logLevel);
	mediasoupclient::Logger::SetDefaultHandler();

	// Initilize mediasoupclient.
	mediasoupclient::Initialize();

	std::cout << "[INFO] welcome to mediasoup broadcaster app!\n" << std::endl;

	Broadcaster broadcaster(socket, wnd);

	broadcaster.Start();
	gtk_main();
	std::cout << "[INFO] press Ctrl+C or Cmd+C to leave..." << std::endl;

	while (true)
	{
		std::cin.get();
	}
	wnd->Destroy();

	return 0;
}
