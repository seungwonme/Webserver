#pragma once

#include <vector>
#include <map>
#include <string>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/event.h>
#include "Config.hpp"
#include "RequestHandler.hpp"
#include "Client.hpp"
#include "EventManager.hpp"
#include "HttpRequest.hpp"
#include "HttpResponse.hpp"

class Server {
public:
	Server(const Config& config);
	~Server();

	void run();

private:
	const Config& config;
	EventManager eventManager;
	RequestHandler requestHandler;
	std::vector<int> serverSockets;
	std::map<int, ServerConfig> socketToConfigMap;
	std::map<int, Client*> clients;

	void setupServerSockets();
	void setNonBlocking(int fd);

	void acceptClient(int serverSocket);
	void handleClientReadEvent(struct kevent& event);

	void sendResponse(Client* client, const HttpResponse& response);
	void closeConnection(Client* client);
};