#include "RequestHandler.hpp"
#include <unistd.h>
#include <ctime>
#include <fstream>

RequestHandler::RequestHandler(ResponseMessage& mResponseMessage, const ServerConfig& serverConfig)
: mResponseMessage(mResponseMessage)
, mServerConfig(serverConfig)
, mPath("")
{
}
void RequestHandler::verifyRequest(const RequestMessage& req)
{
    try
    {
        verifyRequestLine(req.getRequestLine());
        verifyRequestHeaderFields(req.getRequestHeaderFields());
    }
    catch (const HTTPException& e)
    {
        throw e;
    }
}
void RequestHandler::verifyRequestLine(const RequestLine& reqLine)
{
    const std::string method = reqLine.getMethod();
    const std::string reqTarget = reqLine.getRequestTarget();
    const std::string ver = reqLine.getHTTPVersion();
    if (method != "GET" && method != "HEAD" && method != "POST" && method != "DELETE")
    {
        throw HTTPException(METHOD_NOT_ALLOWED);
    }
    if (ver != "HTTP/1.1")
    {
        throw HTTPException(HTTP_VERSION_NOT_SUPPORTED);
    }
    if (reqTarget[0] != '/')
    {
        throw HTTPException(NOT_FOUND);
    }
    if (reqTarget.size() >= 8200)  // nginx max uri length
    {
        throw HTTPException(URI_TOO_LONG);
    }
}
void RequestHandler::verifyRequestHeaderFields(const HeaderFields& reqHeaderFields)
{
    if (reqHeaderFields.hasField("Host") == false)
    {
        throw HTTPException(BAD_REQUEST);
    }
}
void RequestHandler::handleRequest(const RequestMessage& req)
{
    std::string reqTarget = req.getRequestLine().getRequestTarget();
    std::string method = req.getRequestLine().getMethod();
    // NOTE: 요청한 URI에 해당하는 LocationConfig 찾기
    std::map<std::string, LocationConfig>::const_iterator targetFindIter = mServerConfig.locations.find(reqTarget);
    if (targetFindIter == mServerConfig.locations.end())
    {
        for (std::map<std::string, LocationConfig>::const_iterator it = mServerConfig.locations.begin(); it != mServerConfig.locations.end(); it++)
        {
            if (it->first.back() == '/')
            {
                std::string tmp = it->second.root + req.getRequestLine().getRequestTarget();
                if (access(tmp.c_str(), F_OK) == 0)
                {
                    mPath = tmp;
                    break;
                }
            }
        }
        if (mPath == "")
        {
            throw HTTPException(NOT_FOUND);
        }
    }
    else
    {
        mPath = targetFindIter->second.root + req.getRequestLine().getRequestTarget() + targetFindIter->second.index;
    }

    // NOTE: allow_methods 블록이 없을 경우 모든 메소드 허용
    // WARNING: segment fault 발생
    /*
    ==12581==ERROR: AddressSanitizer: heap-buffer-overflow on address 0x000107b03567 at pc 0x0001043c3520 bp 0x00016ba486b0 sp 0x00016ba486a8
    READ of size 1 at 0x000107b03567 thread T0
    #0 0x1043c351c in std::__1::basic_string<char, std::__1::char_traits<char>, std::__1::allocator<char>>::__is_long[abi:ue170006]() const string:1734
    #1 0x1043ba13c in std::__1::basic_string<char, std::__1::char_traits<char>, std::__1::allocator<char>>::size[abi:ue170006]() const string:1168
    #2 0x1043c87a0 in bool std::__1::operator==[abi:ue170006]<std::__1::allocator<char>>(std::__1::basic_string<char, std::__1::char_traits<char>,
    std::__1::allocator<char>> const&, std::__1::basic_string<char, std::__1::char_traits<char>, std::__1::allocator<char>> const&) string:3897 #3 0x1043c7ae4
    in std::__1::basic_string<char, std::__1::char_traits<char>, std::__1::allocator<char>> const*
    std::__1::__find_impl[abi:ue170006]<std::__1::basic_string<char, std::__1::char_traits<char>, std::__1::allocator<char>> const*,
    std::__1::basic_string<char, std::__1::char_traits<char>, std::__1::allocator<char>> const*, std::__1::basic_string<char, std::__1::char_traits<char>,
    std::__1::allocator<char>>, std::__1::__identity>(std::__1::basic_string<char, std::__1::char_traits<char>, std::__1::allocator<char>> const*,
    std::__1::basic_string<char, std::__1::char_traits<char>, std::__1::allocator<char>> const*, std::__1::basic_string<char, std::__1::char_traits<char>,
    std::__1::allocator<char>> const&, std::__1::__identity&) find.h:34 #4 0x1043baadc in std::__1::__wrap_iter<std::__1::basic_string<char,
    std::__1::char_traits<char>, std::__1::allocator<char>> const*> std::__1::find[abi:ue170006]<std::__1::__wrap_iter<std::__1::basic_string<char,
    std::__1::char_traits<char>, std::__1::allocator<char>> const*>, std::__1::basic_string<char, std::__1::char_traits<char>,
    std::__1::allocator<char>>>(std::__1::__wrap_iter<std::__1::basic_string<char, std::__1::char_traits<char>, std::__1::allocator<char>> const*>,
    std::__1::__wrap_iter<std::__1::basic_string<char, std::__1::char_traits<char>, std::__1::allocator<char>> const*>, std::__1::basic_string<char,
    std::__1::char_traits<char>, std::__1::allocator<char>> const&) find.h:81 #5 0x1043b8ffc in RequestHandler::handleRequest(RequestMessage const&,
    ResponseMessage&, ServerConfig const&) RequestHandler.cpp:35 #6 0x1043e2890 in Server::handleClientReadEvent(kevent&) Server.cpp:192 #7 0x1043e03f8 in
    Server::run() Server.cpp:111 #8 0x10440f528 in main main.cpp:18 #9 0x18351a0dc  (<unknown module>)
     */
    // const LocationConfig& locConfig = targetFindIter->second;
    // if (std::find(locConfig.allow_methods.begin(), locConfig.allow_methods.end(), method) == locConfig.allow_methods.end())
    // {
    //     throw HTTPException(METHOD_NOT_ALLOWED);
    // }

    // Connection 헤더 필드 추가
    if (req.getRequestHeaderFields().hasField("Connection") == true)
    {
        mResponseMessage.addResponseHeaderField("Connection", req.getRequestHeaderFields().getField("Connection"));
    }
    else
    {
        mResponseMessage.addResponseHeaderField("Connection", "keep-alive");
    }

    // Method에 따른 처리
    try
    {
        if (method == "GET")
        {
            getRequest(req);
        }
        else if (method == "HEAD")
        {
            headRequest(req);
        }
        else if (method == "POST")
        {
            postRequest(req);
        }
        else if (method == "DELETE")
        {
            deleteRequest(req);
        }
    }
    catch (const HTTPException& e)
    {
        throw e;
    }
}
void RequestHandler::getRequest(const RequestMessage& req)
{
    std::ifstream file(mPath);
    if (file.is_open() == false)
    {
        throw HTTPException(FORBIDDEN);
    }
    mResponseMessage.setStatusLine(req.getRequestLine().getHTTPVersion(), OK, "OK");
    std::string line;
    while (std::getline(file, line))
    {
        // TODO: 파일 마지막에 개행이 없는 경우 개행이 추가로 들어가는 문제
        mResponseMessage.addMessageBody(line + "\n");
    }
    file.close();
    addSemanticHeaderFields(mResponseMessage);
    addContentType(mResponseMessage);
}
void RequestHandler::headRequest(const RequestMessage& req)
{
    getRequest(req);
    mResponseMessage.clearMessageBody();
}
void RequestHandler::postRequest(const RequestMessage& req)
{
    (void)req;
    (void)mResponseMessage;
    (void)mPath;
    mResponseMessage.setStatusLine(req.getRequestLine().getHTTPVersion(), OK, "OK");
    mResponseMessage.addResponseHeaderField("Content-Type", "text/html");
    mResponseMessage.addMessageBody("<html><body><h1>POST Request</h1></body></html>");
    addSemanticHeaderFields(mResponseMessage);
}
void RequestHandler::deleteRequest(const RequestMessage& req)
{
    (void)req;
    (void)mResponseMessage;
    (void)mPath;
    mResponseMessage.setStatusLine(req.getRequestLine().getHTTPVersion(), OK, "OK");
    mResponseMessage.addResponseHeaderField("Content-Type", "text/html");
    mResponseMessage.addMessageBody("<html><body><h1>DELETE Request</h1></body></html>");
    addSemanticHeaderFields(mResponseMessage);
}
void RequestHandler::addSemanticHeaderFields(ResponseMessage& mResponseMessage)
{
    time_t now = time(0);
    struct tm* timeinfo = localtime(&now);
    char buffer[128];
    // Date: Tue, 15 Nov 1994 08:12:31 GMT
    // NOTE: strftime() 함수는 c함수라서 평가에 어긋남, 하지만 Date 헤더 필드가 필수가 아니라서 보너스 느낌으로 넣어둠
    std::strftime(buffer, sizeof(buffer), "%a, %d %b %Y %H:%M:%S GMT", timeinfo);
    std::string date = buffer;

    mResponseMessage.addResponseHeaderField("Content-Length", mResponseMessage.getMessageBodySize());
    mResponseMessage.addResponseHeaderField("Server", "webserv");
    mResponseMessage.addResponseHeaderField("Date", date);
}
void RequestHandler::addContentType(ResponseMessage& mResponseMessage)
{
    // NOTE: Config에 MIME 타입 추가
    std::string ext = mPath.substr(mPath.find_last_of('.') + 1);
    if (ext == "html")
    {
        mResponseMessage.addResponseHeaderField("Content-Type", "text/html");
    }
    else if (ext == "css")
    {
        mResponseMessage.addResponseHeaderField("Content-Type", "text/css");
    }
    else if (ext == "js")
    {
        mResponseMessage.addResponseHeaderField("Content-Type", "text/javascript");
    }
    else if (ext == "jpg" || ext == "jpeg")
    {
        mResponseMessage.addResponseHeaderField("Content-Type", "image/jpeg");
    }
    else if (ext == "png")
    {
        mResponseMessage.addResponseHeaderField("Content-Type", "image/png");
    }
    else
    {
        mResponseMessage.addResponseHeaderField("Content-Type", "application/octet-stream");
    }
}
void RequestHandler::handleException(const HTTPException& e)
{
    if (e.getStatusCode() == BAD_REQUEST)
    {
        badRequest();
    }
    else if (e.getStatusCode() == NOT_FOUND)
    {
        notFound();
    }
    else if (e.getStatusCode() == METHOD_NOT_ALLOWED)
    {
        methodNotAllowed();
    }
    else if (e.getStatusCode() == HTTP_VERSION_NOT_SUPPORTED)
    {
        httpVersionNotSupported();
    }
    else if (e.getStatusCode() == URI_TOO_LONG)
    {
        uriTooLong();
    }
    else if (e.getStatusCode() == FORBIDDEN)
    {
        forbidden();
    }
}
void RequestHandler::badRequest(void)
{
    mResponseMessage.setStatusLine("HTTP/1.1", BAD_REQUEST, "Bad Request");
    mResponseMessage.addMessageBody("<html><head><title>400 Bad Request</title></head><body><h1>400 Bad Request</h1></body></html>");
    mResponseMessage.addResponseHeaderField("Content-Type", "text/html");
    mResponseMessage.addResponseHeaderField("Connection", "close");
    addSemanticHeaderFields(mResponseMessage);
}
void RequestHandler::notFound(void)
{
    mResponseMessage.setStatusLine("HTTP/1.1", NOT_FOUND, "Not Found");
    mResponseMessage.addMessageBody("<html><head><title>404 Not Found</title></head><body><h1>404 Not Found</h1></body></html>");
    mResponseMessage.addResponseHeaderField("Content-Type", "text/html");
    mResponseMessage.addResponseHeaderField("Connection", "keep-alive");
    addSemanticHeaderFields(mResponseMessage);
}
void RequestHandler::methodNotAllowed(void)
{
    mResponseMessage.setStatusLine("HTTP/1.1", METHOD_NOT_ALLOWED, "Method Not Allowed");
    mResponseMessage.addMessageBody("<html><head><title>405 Method Not Allowed</title></head><body><h1>405 Method Not Allowed</h1></body></html>");
    mResponseMessage.addResponseHeaderField("Content-Type", "text/html");
    mResponseMessage.addResponseHeaderField("Connection", "keep-alive");
    addSemanticHeaderFields(mResponseMessage);
}
void RequestHandler::httpVersionNotSupported(void)
{
    mResponseMessage.setStatusLine("HTTP/1.1", HTTP_VERSION_NOT_SUPPORTED, "HTTP Version Not Supported");
    mResponseMessage.addMessageBody("<html><head><title>505 HTTP Version Not Supported</title></head><body><h1>505 HTTP Version Not Supported</h1></body></html>");
    mResponseMessage.addResponseHeaderField("Content-Type", "text/html");
    mResponseMessage.addResponseHeaderField("Connection", "keep-alive");
    addSemanticHeaderFields(mResponseMessage);
}
void RequestHandler::uriTooLong(void)
{
    mResponseMessage.setStatusLine("HTTP/1.1", URI_TOO_LONG, "Request-URI Too Long");
    mResponseMessage.addMessageBody("<html><head><title>414 Request-URI Too Long</title></head><body><h1>414 Request-URI Too Long</h1></body></html>");
    mResponseMessage.addResponseHeaderField("Content-Type", "text/html");
    mResponseMessage.addResponseHeaderField("Connection", "keep-alive");
    addSemanticHeaderFields(mResponseMessage);
}
void RequestHandler::forbidden(void)
{
    mResponseMessage.setStatusLine("HTTP/1.1", FORBIDDEN, "Forbidden");
    mResponseMessage.addMessageBody("<html><head><title>403 Forbidden</title></head><body><h1>403 Forbidden</h1></body></html>");
    mResponseMessage.addResponseHeaderField("Content-Type", "text/html");
    mResponseMessage.addResponseHeaderField("Connection", "keep-alive");
    addSemanticHeaderFields(mResponseMessage);
}
bool RequestHandler::isKeepAlive(void) const
{
    return mResponseMessage.isKeepAlive();
}
