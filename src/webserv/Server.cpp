#include "Server.hpp"
#include "http/message.hpp"
#include "logger/Logger.hpp"
#include "utils/string.hpp"
#include "webserv/Directives.hpp"
#include "utils/os.hpp"
#include <dirent.h>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <sys/select.h>
#include <unistd.h>
#include <vector>

Server::Server(ConfigItem* global)
{

    std::vector<ConfigItem*> serverBlocks = global->findBlocks("server");

    _config = global;

    for (std::vector<ConfigItem*>::const_iterator it = serverBlocks.begin();
         it != serverBlocks.end();
         ++it) {

        std::vector<ConfigItem*> listens = (*it)->findBlocks("listen");

        for (size_t i = 0; i != listens.size(); i++) {

            ListenData data = parseListen(listens[i]->getValue());

            if (_entrypoints.find(data.port) == _entrypoints.end()) {
                Net::ServerSocket* ssock = new Net::ServerSocket();

                // TODO: handle max connexion
                ssock->open();
                ssock->bind(data.port, data.v4).listen(1024);

                _entrypoints[data.port].ssock = ssock;
            }

            _entrypoints[data.port].candidates.push_back(*it);
        }
    }
}

Server::Server(Server const& src)
{
    *this = src;
}

Server::~Server(void)
{

    std::map<unsigned short, Socket>::iterator it, ite = _sockets.end();

    for (it = _sockets.begin(); it != ite; it++) {

        if (_sockets[it->first].socket != -1)
            close(_sockets[it->first].socket);
    }

    delete _config;
}

Server&
Server::operator=(Server const& rhs)
{
    if (this != &rhs) {
        this->_sockets = rhs._sockets;
        this->_connexion = rhs._connexion;
    }

    return *this;
}

ConfigItem*
Server::_selectServer(std::vector<ConfigItem*>& candidates,
                      const std::string& host)
{
    std::string serverName;
    ConfigItem* serverNameItem = 0;
    ConfigItem* noServerName = 0;
    std::string strippedHost = host.substr(0, host.find(':'));

    for (std::vector<ConfigItem*>::iterator it = candidates.begin();
         it != candidates.end();
         ++it) {
        serverNameItem = (*it)->findNearest("server_name");
        if (serverNameItem && serverNameItem->getValue() == strippedHost) {
            return *it;
        } else if (!serverNameItem && !noServerName) {
            noServerName = *it;
        }
    }

    return noServerName;
}

void
Server::_noAutoIndexResponse( std::string path, HTTP::Response& res, Directives& direc) {

	std::ifstream		ifs;

  	ifs.open(path.c_str());
	if (!ifs) {
		std::cout << "path: " << path << std::endl;
		if (direc.getRoot() == "none") {
			res.setStatus(HTTP::INTERNAL_SERVER_ERROR);
			_genErrorPage(ERROR_SAMPLE, "500", "Internal Server Error", "the server encountered an internal error");

		}
		else {
			res.setStatus(HTTP::NOT_FOUND);
			_genErrorPage(ERROR_SAMPLE, "404", "Not Found", "the page you are looking for doesn't exist");
		}
		
		res.sendFile(ERROR_PAGE);
		//ifs.open(path.c_str());
		//_headerParam["Content-Type"] = "text/html";
	} else {
		res.sendFile(path);
	}
	
    //buf << ifs.rdbuf();
   // ifs.close();
}

void
Server::_autoIndexResponse( std::string path, std::stringstream & buf, HTTP::Request& req, HTTP::Response& res) {

	glogger << Logger::DEBUG << PURPLE << "Autoindex is ON... Listing directory: " << path << CLR << "\n";

	DIR *				folder = opendir(path.c_str());
		
	if (folder) {
			
		struct dirent *	dir;

		while ((dir = readdir(folder)) != NULL) {
			std::cout << "Host: " << req.getHeaderField("HosT") << std::endl;
			std::cout << "resourceURI: " << req.getResourceURI() << std::endl;
			buf << "<a href=\"http://" << req.getHeaderField("Host") << req.getResourceURI() << (req.getResourceURI() == "/" ? "" : "/") << dir->d_name << "\">" << dir->d_name << "</a><br/>" << std::endl;
			glogger << Logger::DEBUG << dir->d_name << "\n";
		}

		res.send(buf.str());

		glogger << Logger::DEBUG << "\n";
		closedir(folder);
	}
}

void
Server::_createResponse(HTTP::Request& req, HTTP::Response& res, ConfigItem* server)
{
    Directives directives;
    std::stringstream buf;
    std::string location;
    bool haveLoc = false;

    std::vector<ConfigItem*> locations = server->findBlocks("location");

    for (std::vector<ConfigItem*>::iterator it = locations.begin();
         it != locations.end();
         it++) {

        location = (*it)->getValue();

        if ((haveLoc = directives.haveLocation(req.getResourceURI(),
                                               location)) == true) {
            glogger << Logger::DEBUG << "haveLoc is true\n";
            directives.getConfig(*it, req.getResourceURI());
        }
    }

    if (haveLoc == false) {
        glogger << Logger::DEBUG << "haveLoc is false\n";
        directives.getConfig(server, req.getResourceURI());
    }

	std::cout << "Path: " << directives.getPath() << std::endl;
    if (isFolder(directives.getPath()) == true) {
        glogger << Logger::DEBUG << "IS FOLDER\n";
        directives.setPathWithIndex();

        glogger << Logger ::DEBUG << "Path+index [" << directives.getPath()
                << "]\n";
        if (isFolder(directives.getPath()) == true) {

            if (directives.getAutoIndex() == "on")
                _autoIndexResponse(directives.getPath(), buf, req, res);
            else {
                std::string errorPage =
                  _checkErrorPage(directives.getDefaultErrorFile(),
                                  "403",
                                  "Forbidden",
                                  "the access is permanently forbidden");
				std::cout << "Error page: " << errorPage << std::endl;
				

                std::ifstream ifs;
                ifs.open(errorPage.c_str());
                buf << ifs.rdbuf();
                ifs.close();

				res.setHeaderField("Content-Type", "text/html");
				res.setStatus(HTTP::FORBIDDEN).sendFile(errorPage);
				/*
                _headerParam["Status-Code"] = "403 Forbidden";
                _headerParam["Content-Type"] = "text/html";
				*/
            }
        } else
            _noAutoIndexResponse(directives.getPath(), res, directives);
    } else {
        _noAutoIndexResponse(directives.getPath(), res, directives);
	}

	res.setHeaderField("Last-Modified", getLastModified(directives.getPath()));
	res.setHeaderField("Date", getDate(time(0)));
	res.setHeaderField("Location", req.getResourceURI());

	// add referer

	/*
    _response = _headerParam["HTTP"] + " " + _headerParam["Status-Code"] +
                "\r\n" + "Content-Type: " + _headerParam["Content-Type"] +
                ";charset=UTF-8\r\n" +
                "Content-Length: " + _headerParam["Content-Length"] + "\r\n" +
                "Date: " + getDate(time(0)) + "\r\n" +
                "Last-Modified: " + getLastModified(directives.getPath()) +
                "\r\n" + "Location: " + _headerParam["Referer"] + "\r\n" +
                "Server: webserv" + "\r\n\r\n" + buf.str();
	*/
}

std::string
Server::_replace(std::string in, std::string s1, std::string s2) {

	std::string		out;
	std::string		line;
	size_t			len = s1.size();
	std::istringstream	iss(in);

	while (getline(iss, line)) {

		for (size_t i = 0; i < line.size(); i++) {
				
				size_t	found = line.find(s1, i);

				if (found == std::string::npos) {

					out += &line[i];
					break;
				}
				else {

					if (found > i) {

						std::string	subLine = line.substr(i, found - i);
						out += subLine;
						i += subLine.size();
					}
					out += s2;
					i += len - 1;
				}
		}
		out += "\n";
	}
	return out;
}

void
Server::_genErrorPage( std::string file, std::string code, std::string msg, std::string sentence ) {

	std::ifstream	ifs(file.c_str());
	std::string		res;
	std::string		line;
	std::string		out;

	while (getline(ifs, line)) {

		res += line;
		res += "\n";
	}

	ifs.close();

	out = _replace(res, "ERROR_CODE", code);
	out = _replace(out, "ERROR_MESSAGE", msg);
	out = _replace(out, "ERROR_SENTENCE", sentence);

	
	std::ofstream	ofs(ERROR_PAGE);
	ofs << out;
	ofs.close();
}

std::string
Server::_checkErrorPage( std::string defaultPage, std::string code, std::string errorMsg, std::string errorSentence ) {

	struct stat	s;

	if (stat(defaultPage.c_str(), &s) == 0) {

		glogger << Logger::DEBUG << Logger::getTimestamp() << " Default error page exists. Using " << defaultPage << "\n";
		return defaultPage;
	}
	else {
		
		glogger << Logger::DEBUG << Logger::getTimestamp() << " Default error page does not exist. Using webserv default error page\n";
		_genErrorPage(ERROR_SAMPLE, code, errorMsg, errorSentence);
		return ERROR_PAGE;
	}
}

void
Server::start(void)
{
    Net::SocketSet set;
    std::map<Net::ClientSocket*, std::string> data;
    std::map<Net::ClientSocket*, HTTP::Request> incomingRequests;

    for (SockMap::const_iterator cit = _entrypoints.begin();
         cit != _entrypoints.end();
         ++cit) {
        set += *cit->second.ssock;
    }

    std::cout << "webserv is running\nHit Ctrl-C to exit." << std::endl;

    while (isWebservAlive) {
        std::list<Net::Socket*> ready = set.select();

        // std::cout << ready.size() << " socket(s) are ready" << std::endl;

        for (std::list<Net::Socket*>::iterator cit = ready.begin();
             cit != ready.end();
             ++cit) {
            Net::ClientSocket* csock = dynamic_cast<Net::ClientSocket*>(*cit);

            // if this is a client socket, we need to ready the data from it to
            // build the request
            if (csock) {

                // parsing request header
                if (incomingRequests.find(csock) == incomingRequests.end()) {
                    std::cout << data[csock] << std::endl;

                    data[csock] += csock->recv();

                    if (data[csock].find(HTTP::BODY_DELIMITER) !=
                        std::string::npos) {
                        incomingRequests.insert(
                          std::make_pair(csock, HTTP::Request(data[csock])));
                    }
                }

                // parsing request body
                if (incomingRequests.find(csock) != incomingRequests.end()) {
                    HTTP::Request req = incomingRequests[csock];
                    std::istringstream oss(
                      req.getHeaderField("Content-Length"));
                    size_t contentLength = 0;
                    oss >> contentLength;

                    std::cout << "Length: " << contentLength << "\n";

                    if (req.body.str().size() < contentLength) {
                        std::cout << contentLength - req.body.str().size()
                                  << std::endl;
                        std::string s =
                          csock->recv(contentLength - req.body.str().size());
                        req.body << s;
                        std::cout << "size: " << req.body.str().size()
                                  << std::endl;
                    }

                    if (req.body.str().size() >= contentLength) {
                        HTTP::Response res(req);

                        // socket cleanup
                        incomingRequests.erase(csock);
                        data.erase(csock);

                        std::cout << "Request received on port "
                                  << csock->getPort() << std::endl;

                        ConfigItem* server = _selectServer(
                          _entrypoints[csock->getPort()].candidates,
                          req.getHeaderField("Host"));

                        // TODO: check server
						std::cout << *server << std::endl;
                        _createResponse(req, res, server);

                        csock->send(res.str());

                        //////// MERGE RESPONSE LOGIC ///////////

                        // header.createResponse(_sockets[it->first].item);
                        // sendResponse(header);

                        /*
if (req.body.str().empty()) {
    res.send("Hello world");
} else {
    res.send(req.body.str());
}
                        */

                        //////// MERGE RESPONSE LOGIC ////////////

                        set -= *csock;
                        csock->close();
                        delete csock;
                    }
                }

                // if this is a server socket, then we need to accept a new
                // connection
            } else {
                Net::ServerSocket* ssock =
                  dynamic_cast<Net::ServerSocket*>(*cit);

                // add the new client to the socket list
                Net::ClientSocket* csock =
                  new Net::ClientSocket(ssock->waitForConnection());
                set += *csock;
            }
        }
    }

    // delete every server socket

    for (SockMap::const_iterator cit = _entrypoints.begin();
         cit != _entrypoints.end();
         ++cit) {
        delete cit->second.ssock;
    }
}

void
Server::sendResponse(Header header)
{
    std::string response = header.getResponse();

    if (response.size() > 512)
        glogger << Logger::DEBUG << "\n"
                << Logger::getTimestamp() << PURPLE << " Response Header:\n\n"
                << CLR << response.substr(0, 512) << "\n\n[ ...SNIP... ]\n";
    else
        glogger << Logger::DEBUG << "\n"
                << Logger::getTimestamp() << PURPLE << " Response Header:\n\n"
                << CLR << response << "\n";

    // write(_connexion, response.c_str(), response.length());
    send(_connexion, response.c_str(), response.size(), 0);
}
