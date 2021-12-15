#include "Constants.hpp"
#include "core.hpp"
#include "http/Exception.hpp"
#include "http/status.hpp"
#include "utils/ErrorPageGenerator.hpp"
#include "utils/string.hpp"
#include <cstdlib>
#include <sstream>

using std::map;
using std::ostringstream;
using std::string;

using HTTP::StatusCode;
using HTTP::toStatusCodeString;

static string
genDefaultErrorPage(StatusCode code, const std::string& hint)
{
    ostringstream oss;

    oss << "<style> body { text-align: center; } h1 { margin-bottom: 0; }"
           "</style>\n<h1> "
        << code << " " << toStatusCodeString(code) << " </h1>";

    const char* errorHint = getenv("ERROR_HINT");

    if (errorHint && parseInt(errorHint, 10)) {
        oss << "<br/><h4>" << hint << "</h4>";
    }

    oss << "<hr/>\nwebserv";

    return oss.str();
}

void
handleHttpException(HTTP::Exception& e)
{
    HTTP::Request* req = e.req();
    HTTP::Response* res = req->response();
    int fd = req->getClientFd();
    unsigned int code = HTTP::toStatusCode(e.status());

    // do not keep alive
    req->setHeaderField("Connection", "close");
    res->setStatus(e.status());
    req->parser->stop();

    if (uploaders.find(fd) != uploaders.end()) {
        delete uploaders[fd];
        uploaders.erase(fd);
    }

    if (cgis.find(fd) != cgis.end()) {
        delete cgis[fd];
        cgis.erase(fd);
    }

    map<unsigned int, string> errorPages = parseErrorPage(req->getBlock());

    if (errorPages.find(code) != errorPages.end()) {
        req->rewrite(errorPages[code]);
        res->data = res->formatHeader();
        res->data += res->body;
        return;
    }

    res->send(genDefaultErrorPage(e.status(), e.what()));
    res->data = res->formatHeader();
    res->data += res->body;
}
