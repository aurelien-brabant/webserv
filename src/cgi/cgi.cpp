#include "cgi/cgi.hpp"
#include "utils/string.hpp"
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <cstdlib>

CommonGatewayInterface::CommonGatewayInterface(int csockFd, fd_set& fdSet, HTTP::Request& req, const std::string& cgiExecName, const std::string& filepath):
_cgiExecName(cgiExecName), _filepath(filepath), _csockFd(csockFd), _fdSet(fdSet), _req(req), _proc(-1), _state(STREAMING_HEADER), _isDone(false)
{
    _inputFd[0] = -1;
    _inputFd[1] = -1;
    _outputFd[0] = -1;
    _outputFd[1] = -1;
}

static std::pair<const std::string, std::string> transformClientHeaders(const std::pair<const std::string, std::string>& p)
{
    return make_pair(std::string("HTTP_" + toUpperCase(p.first)), p.second);
}

void CommonGatewayInterface::start(void)
{
    std::map<std::string, std::string> cgiEnv;
    std::string tmp;
    std::ostringstream oss;
    pipe(_inputFd);
    pipe(_outputFd);
    fcntl(_inputFd[0], F_SETFL, O_NONBLOCK);
    fcntl(_inputFd[1], F_SETFL, O_NONBLOCK);
    fcntl(_outputFd[0], F_SETFL, O_NONBLOCK);
    fcntl(_outputFd[1], F_SETFL, O_NONBLOCK);

    FD_SET(_inputFd[1], &_fdSet);
    FD_SET(_outputFd[0], &_fdSet);
    
    HTTP::Header henv;

    // Merge all the request header fields into the CGI environment, prefixing each field with "HTTP_"
    henv.merge(_req.header(), &transformClientHeaders);

    // Server information

    henv.setField("SERVER_PROTOCOL", _req.getProtocol());
    henv.setField("SERVER_SOFTWARE", henv.getField("Server"));

    henv.setField("REDIRECT_STATUS", "200");
    
    henv.setField("SCRIPT_FILENAME", _filepath);
    henv.setField("SCRIPT_NAME", _filepath);

    tmp = henv.getField("HTTP_CONTENT-LENGTH");
    if (!tmp.empty()) {
        henv.setField("CONTENT_LENGTH", henv.getField("HTTP_CONTENT-LENGTH"));
    }

    tmp = henv.getField("HTTP_CONTENT-TYPE");
    if (!tmp.empty()) {
        henv.setField("CONTENT_TYPE", henv.getField("HTTP_CONTENT-TYPE"));
    }
 
    henv.setField("REMOTE_HOST", henv.getField("Host"));
    henv.setField("HTTPS", "off");
    henv.setField("REQUEST_METHOD", _req.getMethod());
    henv.setField("PATH_INFO", _filepath);

    std::cout << _filepath << std::endl;

    _proc = fork();
    
    if (_proc == 0) {
        dup2(_inputFd[0], STDIN_FILENO);
        dup2(_outputFd[1], STDOUT_FILENO);

        char* const argv[3] = {
            strdup(_cgiExecName.c_str()),
            strdup(_filepath.c_str()),
            NULL
        };
        
        char** envp = henv.toEnv();
        execvpe(argv[0], argv, envp);

        perror("execvp: ");
        exit(1);
    }
    
    close(_inputFd[0]);
    close(_outputFd[1]);
}


CommonGatewayInterface::~CommonGatewayInterface(void)
{
    FD_CLR(_inputFd[1], &_fdSet);
    FD_CLR(_outputFd[0], &_fdSet);

    if (_inputFd[1] != -1) {
        close(_inputFd[1]);
    }

    if (_outputFd[0] != -1) {
        close(_outputFd[0]);
    }
}

int CommonGatewayInterface::getClientFd() const
{
    return _csockFd;
}

int CommonGatewayInterface::getInputFd() const
{
    return _inputFd[1];
}

int CommonGatewayInterface::getOutputFd() const
{
    return _outputFd[0];
}

const std::string& CommonGatewayInterface::getData(void) const
{
    return _data;
}

bool CommonGatewayInterface::isDone(void) const
{
    return _isDone;
}

/**
 * @brief parse response from php-cgi and send a proper HTTP response to the user agent
 * 
 */

void CommonGatewayInterface::stream(void)
{
    char buf[1024] = { 0 };

    std::cout << "Before reading" << std::endl;
    int ret = ::read(getOutputFd(), buf, 1023);
    std::cout << "After reading" << std::endl;

    // EOF reached
    if (ret <= 0) {
        _isDone = true;
        return ;
    }

    // we need to wait for the complete header before we can actually process the request
    if (_state == STREAMING_HEADER) {
        _data += buf;
        std::string::size_type pos = _data.find(HTTP::BODY_DELIMITER);

        if (pos != std::string::npos) {
            HTTP::Header cgiHeader;

            cgiHeader.parse(_data.substr(0, pos));

            std::cout << cgiHeader.format() << std::endl;

            // Change status according to what php-cgi (may) say
            std::string status = cgiHeader.getField("status");
            if (!status.empty()) {
                std::vector<std::string> ss = split(status, " ");
                _res.setStatus(parseInt(ss[0], 10));
            }

            _res.send(_data.substr(pos + 4, std::string::npos));

            _res.header().merge(cgiHeader);

            _state = STREAMING_BODY;

            send(_csockFd, _res.str().c_str(), _res.str().size(), 0);
        }
    // If we've already got the header then we can send to the client what the CGI just sent
    } else {
        send(_csockFd, buf, ret, 0);
    }

    std::cout << "Finished streaming" << std::endl;
}

