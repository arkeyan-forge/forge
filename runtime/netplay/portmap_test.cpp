// The router port mapping (portmap.h): the message builders and parsers, then whole mappings against fake routers on
// the loopback address (an SSDP and HTTP UPnP gateway, a NAT-PMP gateway).
#include "netplay/portmap.h"
#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <string>
#include <thread>
#include <vector>

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

using namespace netplay::portmap;

namespace
{
    int g_failures = 0;
    void Check(bool condition, const char* what)
    {
        if (!condition)
        {
            fprintf(stderr, "FAIL: %s\n", what);
            ++g_failures;
        }
    }

#if defined(_WIN32)
    using Socket = SOCKET;
    void CloseSocket(Socket s) { closesocket(s); }
    bool Readable(Socket s, int ms)
    {
        WSAPOLLFD fd{ s, POLLIN, 0 };
        return WSAPoll(&fd, 1, ms) > 0;
    }
#else
    using Socket = int;
    void CloseSocket(Socket s) { close(s); }
    bool Readable(Socket s, int ms)
    {
        pollfd fd{ s, POLLIN, 0 };
        return poll(&fd, 1, ms) > 0;
    }
#endif

    // A socket bound to 127.0.0.1 on a port of the system's choosing.
    Socket Bind(int type, uint16_t& port)
    {
        const Socket s = socket(AF_INET, type, 0);
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        bind(s, reinterpret_cast<sockaddr*>(&address), sizeof(address));
        socklen_t size = sizeof(address);
        getsockname(s, reinterpret_cast<sockaddr*>(&address), &size);
        port = ntohs(address.sin_port);
        if (type == SOCK_STREAM)
            listen(s, 8);
        return s;
    }

    std::string Hostport(uint16_t port) { return "127.0.0.1:" + std::to_string(port); }

    std::string Http(int status, const std::string& body, bool chunked = false)
    {
        std::string head = "HTTP/1.1 " + std::to_string(status) + (status == 200 ? " OK" : " Internal Server Error") +
                           "\r\nContent-Type: text/xml\r\n";
        if (!chunked)
            return head + "Content-Length: " + std::to_string(body.size()) + "\r\n\r\n" + body;
        const size_t half = body.size() / 2;
        char sizes[2][16];
        snprintf(sizes[0], sizeof(sizes[0]), "%zx", half);
        snprintf(sizes[1], sizeof(sizes[1]), "%zx", body.size() - half);
        return head + "Transfer-Encoding: chunked\r\n\r\n" + sizes[0] + "\r\n" + body.substr(0, half) + "\r\n" +
               sizes[1] + "\r\n" + body.substr(half) + "\r\n0\r\n\r\n";
    }

    std::string Fault(int code)
    {
        return "<?xml version=\"1.0\"?><s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\"><s:Body>"
               "<s:Fault><faultcode>s:Client</faultcode><faultstring>UPnPError</faultstring><detail>"
               "<UPnPError xmlns=\"urn:schemas-upnp-org:control-1-0\"><errorCode>" +
               std::to_string(code) +
               "</errorCode><errorDescription>x</errorDescription></UPnPError></detail></s:Fault></s:Body></"
               "s:Envelope>";
    }

    const char* Description = R"(<?xml version="1.0"?>
<root xmlns="urn:schemas-upnp-org:device-1-0"><device><deviceType>urn:schemas-upnp-org:device:InternetGatewayDevice:1</deviceType>
<serviceList><service><serviceType>urn:schemas-upnp-org:service:Layer3Forwarding:1</serviceType><controlURL>/l3f</controlURL></service></serviceList>
<deviceList><device><deviceType>urn:schemas-upnp-org:device:WANDevice:1</deviceType><deviceList><device>
<deviceType>urn:schemas-upnp-org:device:WANConnectionDevice:1</deviceType><serviceList>
<service><serviceType>urn:schemas-upnp-org:service:WANPPPConnection:1</serviceType><controlURL>/ppp</controlURL></service>
<service><serviceType>urn:schemas-upnp-org:service:WANIPConnection:1</serviceType><serviceId>urn:upnp-org:serviceId:WANIPConn1</serviceId>
<controlURL>ctl/IPConn?a=1&amp;b=2</controlURL></service></serviceList></device></deviceList></device></deviceList></device></root>)";

    // A UPnP gateway: answers one SSDP search, serves the description, takes SOAP actions.
    struct FakeIgd
    {
        Socket ssdp, http;
        uint16_t ssdpPort = 0, httpPort = 0;
        int addFault = 0;       // fault for the first AddPortMapping
        bool chunked = false;   // the description in chunks
        std::vector<std::string> actions; // the SOAPAction of each request
        std::string lastAdd;    // the body of the last AddPortMapping
        std::atomic<bool> stop{ false };
        std::thread thread;

        FakeIgd()
        {
            ssdp = Bind(SOCK_DGRAM, ssdpPort);
            http = Bind(SOCK_STREAM, httpPort);
            thread = std::thread([this] { Run(); });
        }
        // Before reading what it recorded: its thread writes it.
        void Stop()
        {
            if (!stop.exchange(true))
                thread.join();
        }
        ~FakeIgd()
        {
            Stop();
            CloseSocket(ssdp);
            CloseSocket(http);
        }

        void Run()
        {
            while (!stop)
            {
                if (Readable(ssdp, 5))
                {
                    char buffer[1024];
                    sockaddr_in from{};
                    socklen_t size = sizeof(from);
                    const auto n =
                        recvfrom(ssdp, buffer, int(sizeof(buffer)), 0, reinterpret_cast<sockaddr*>(&from), &size);
                    const std::string search(buffer, size_t(n > 0 ? n : 0));
                    if (search.starts_with("M-SEARCH") && search.find("InternetGatewayDevice") != std::string::npos)
                    {
                        const std::string answer =
                            "HTTP/1.1 200 OK\r\nCACHE-CONTROL: max-age=120\r\nST: "
                            "urn:schemas-upnp-org:device:InternetGatewayDevice:1\r\nLocation: http://" +
                            Hostport(httpPort) + "/igd/desc.xml\r\nSERVER: test\r\n\r\n";
                        sendto(ssdp, answer.data(), int(answer.size()), 0, reinterpret_cast<sockaddr*>(&from), size);
                    }
                }
                if (Readable(http, 5))
                    Serve(accept(http, nullptr, nullptr));
            }
        }

        void Serve(Socket client)
        {
            std::string request;
            while (Readable(client, 500))
            {
                char buffer[4096];
                const auto n = recv(client, buffer, int(sizeof(buffer)), 0);
                if (n <= 0)
                    break;
                request.append(buffer, size_t(n));
                const size_t end = request.find("\r\n\r\n");
                const size_t length = request.find("Content-Length: ");
                if (end != std::string::npos &&
                    (length == std::string::npos ||
                        request.size() >= end + 4 + std::stoul(request.substr(length + 16))))
                    break;
            }
            std::string answer;
            if (request.starts_with("GET /igd/desc.xml "))
                answer = Http(200, Description, chunked);
            else if (request.starts_with("POST /igd/ctl/IPConn?a=1&b=2 "))
            {
                const size_t at = request.find("SOAPAction: \"");
                const size_t hash = request.find('#', at);
                const std::string action = request.substr(hash + 1, request.find('"', hash) - hash - 1);
                actions.push_back(action);
                if (action == "AddPortMapping")
                {
                    lastAdd = request.substr(request.find("\r\n\r\n") + 4);
                    if (addFault)
                    {
                        answer = Http(500, Fault(addFault));
                        addFault = 0;
                    }
                    else
                        answer = Http(200, "<s:Envelope><s:Body><u:AddPortMappingResponse/></s:Body></s:Envelope>");
                }
                else if (action == "GetExternalIPAddress")
                    answer = Http(200, "<s:Envelope><s:Body><u:GetExternalIPAddressResponse xmlns:u=\"x\">"
                                       "<NewExternalIPAddress>203.0.113.7</NewExternalIPAddress>"
                                       "</u:GetExternalIPAddressResponse></s:Body></s:Envelope>");
                else
                    answer = Http(200, "<s:Envelope/>");
            }
            else
                answer = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n";
            send(client, answer.data(), int(answer.size()), 0);
            CloseSocket(client);
        }
    };

    // A NAT-PMP gateway: external address 198.51.100.9, external port = internal + 1.
    struct FakeNatPmp
    {
        Socket s;
        uint16_t port = 0;
        int requests = 0;
        uint32_t lastLifetime = 99;
        std::atomic<bool> stop{ false };
        std::thread thread;

        FakeNatPmp()
        {
            s = Bind(SOCK_DGRAM, port);
            thread = std::thread([this] {
                while (!stop)
                {
                    if (!Readable(s, 5))
                        continue;
                    uint8_t in[64];
                    sockaddr_in from{};
                    socklen_t size = sizeof(from);
                    const auto n = recvfrom(
                        s, reinterpret_cast<char*>(in), int(sizeof(in)), 0, reinterpret_cast<sockaddr*>(&from), &size);
                    ++requests;
                    std::vector<uint8_t> out;
                    if (n == 2 && in[1] == 0)
                        out = { 0, 128, 0, 0, 0, 0, 0, 1, 198, 51, 100, 9 };
                    else if (n == 12 && (in[1] == 1 || in[1] == 2))
                    {
                        const uint16_t internal = uint16_t(in[4] << 8 | in[5]);
                        lastLifetime = uint32_t(in[8]) << 24 | uint32_t(in[9]) << 16 | uint32_t(in[10]) << 8 | in[11];
                        const uint16_t external = lastLifetime ? uint16_t(internal + 1) : 0;
                        out = { 0, uint8_t(128 + in[1]), 0, 0, 0, 0, 0, 1, in[4], in[5], uint8_t(external >> 8),
                            uint8_t(external), in[8], in[9], in[10], in[11] };
                    }
                    else
                        continue;
                    sendto(s, reinterpret_cast<const char*>(out.data()), int(out.size()), 0,
                        reinterpret_cast<sockaddr*>(&from), size);
                }
            });
        }
        void Stop()
        {
            if (!stop.exchange(true))
                thread.join();
        }
        ~FakeNatPmp()
        {
            Stop();
            CloseSocket(s);
        }
    };

    void TestParsers()
    {
        Url url;
        Check(ParseUrl("http://192.168.1.1:5000/rootDesc.xml", url) && url.host == "192.168.1.1" && url.port == 5000 &&
                  url.path == "/rootDesc.xml",
            "url with a port");
        Check(ParseUrl("HTTP://10.0.0.1", url) && url.port == 80 && url.path == "/", "url without port or path");
        Check(!ParseUrl("https://10.0.0.1/", url) && !ParseUrl("http://:80/", url) && !ParseUrl("http://a:0/", url) &&
                  !ParseUrl("http://a:99999/", url),
            "bad urls refused");
        Url base, resolved;
        ParseUrl("http://10.0.0.1:1900/igd/desc.xml", base);
        Check(ResolveUrl(base, "/ctl", resolved) && resolved.path == "/ctl" && resolved.port == 1900, "absolute path");
        Check(ResolveUrl(base, "ctl/x", resolved) && resolved.path == "/igd/ctl/x", "relative path");
        Check(ResolveUrl(base, "http://10.0.0.2:80/c", resolved) && resolved.host == "10.0.0.2", "absolute url");

        std::string location;
        Check(ParseSsdpResponse("HTTP/1.1 200 OK\r\nST: x\r\nlocation:  http://10.0.0.1/d.xml \r\n\r\n", location) &&
                  location == "http://10.0.0.1/d.xml",
            "ssdp location, any case, trimmed");
        Check(!ParseSsdpResponse("NOTIFY * HTTP/1.1\r\nLOCATION: http://x/\r\n\r\n", location), "ssdp notify ignored");
        Check(!ParseSsdpResponse("HTTP/1.1 200 OK\r\nST: x\r\n\r\nLOCATION: http://x/\r\n", location),
            "ssdp location after the headers ignored");
        Check(SsdpSearch("urn:x").find("ST: urn:x\r\n\r\n") != std::string::npos, "ssdp search");

        WanService service;
        Url description;
        ParseUrl("http://10.0.0.1:5000/igd/desc.xml", description);
        Check(FindWanService(Description, description, service) &&
                  service.type == "urn:schemas-upnp-org:service:WANIPConnection:1" &&
                  service.control.path == "/igd/ctl/IPConn?a=1&b=2",
            "WANIPConnection preferred over PPP, entities decoded, relative control url");
        const std::string pppOnly =
            "<root><URLBase>http://10.0.0.9:80/</URLBase><service><serviceType>urn:schemas-upnp-org:"
            "service:WANPPPConnection:1</serviceType><controlURL>ppp</controlURL></service></root>";
        Check(FindWanService(pppOnly, description, service) && service.control.host == "10.0.0.9" &&
                  service.control.path == "/ppp",
            "PPP only, URLBase");
        Check(!FindWanService("<root><service><serviceType>urn:x</serviceType></service>", description, service),
            "no WAN service");

        const std::pair<std::string_view, std::string> args[] = { { "NewPortMappingDescription", "a<b&c" } };
        const std::string soap = SoapRequest(service.control, "urn:t:1", "AddPortMapping", args);
        Check(soap.starts_with("POST /ppp HTTP/1.1\r\nHost: 10.0.0.9:80\r\n") &&
                  soap.find("SOAPAction: \"urn:t:1#AddPortMapping\"") != std::string::npos &&
                  soap.find("<NewPortMappingDescription>a&lt;b&amp;c</NewPortMappingDescription>") != std::string::npos,
            "soap request");
        const size_t bodyAt = soap.find("\r\n\r\n") + 4;
        Check(soap.find("Content-Length: " + std::to_string(soap.size() - bodyAt)) != std::string::npos,
            "soap content length");

        SoapResult result;
        Check(ParseSoapResponse(Http(200, "<a><NewExternalIPAddress> 1.2.3.4 </NewExternalIPAddress></a>"),
                  "NewExternalIPAddress", result) &&
                  result.status == 200 && result.value == "1.2.3.4",
            "soap value");
        Check(ParseSoapResponse(Http(500, Fault(718)), "", result) && result.status == 500 && result.errorCode == 718,
            "soap fault code");
        int status = 0;
        std::string body;
        Check(SplitHttpResponse(Http(200, "hello world", true), status, body) && body == "hello world", "chunked body");
        Check(!SplitHttpResponse("HTTP/1.1 200 OK\r\nContent-Length: 50\r\n\r\nshort", status, body),
            "short body refused");
        Check(!SplitHttpResponse("HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\nffffffff\r\nx", status, body),
            "oversized chunk refused");

        const auto map = NatPmpMapRequest(47640, 47641, 7200);
        Check(map[1] == 1 && map[4] == 0xBA && map[5] == 0x18 && map[7] == 0x19 && map[10] == 0x1C && map[11] == 0x20,
            "nat-pmp map request");
        const uint8_t address[] = { 0, 128, 0, 0, 0, 0, 0, 5, 198, 51, 100, 9 };
        uint16_t code = 1;
        uint32_t ip = 0;
        Check(ParseNatPmpAddress(address, code, ip) && code == 0 && ip == 0xC6336409, "nat-pmp address");
        const uint8_t refused[] = { 0, 129, 0, 2, 0, 0, 0, 5 };
        uint16_t internal = 0, external = 0;
        uint32_t lifetime = 0;
        Check(ParseNatPmpMap(refused, code, internal, external, lifetime) && code == 2, "nat-pmp refusal");
        const uint8_t wrongOp[] = { 0, 128, 0, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0 };
        Check(!ParseNatPmpMap(wrongOp, code, internal, external, lifetime), "nat-pmp wrong opcode");
        // Truncated or random packets never read past their end (the sanitizers job watches this).
        uint8_t noise[16];
        std::mt19937 random(1);
        for (int i = 0; i < 20000; ++i)
        {
            for (auto& byte : noise)
                byte = uint8_t(random());
            const size_t size = size_t(random()) % (sizeof(noise) + 1);
            ParseNatPmpAddress(std::span(noise, size), code, ip);
            ParseNatPmpMap(std::span(noise, size), code, internal, external, lifetime);
            std::string text(reinterpret_cast<const char*>(noise), size);
            SplitHttpResponse("HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n" + text, status, body);
            ParseSsdpResponse("HTTP/1.1 200 OK\r\n" + text, location);
            FindWanService("<service><serviceType>urn:schemas-upnp-org:service:WANIPConnection:1</serviceType>"
                           "<controlURL>" +
                               text,
                description, service);
        }

        Check(ParseIpv4("192.168.1.254", ip) && ip == 0xC0A801FE && Ipv4Text(ip) == "192.168.1.254", "ipv4 text");
        Check(!ParseIpv4("1.2.3", ip) && !ParseIpv4("1.2.3.256", ip) && !ParseIpv4("1.2.3.4.5", ip) &&
                  !ParseIpv4("1..3.4", ip),
            "bad ipv4 refused");
        Check(IsPublicIpv4(0xCB007107) && !IsPublicIpv4(0xC0A80101) && !IsPublicIpv4(0x0A000001) &&
                  !IsPublicIpv4(0x64400001) && !IsPublicIpv4(0xAC1F0001) && IsPublicIpv4(0xAC200001) &&
                  !IsPublicIpv4(0x7F000001),
            "public and private addresses");
    }

    Options TestOptions()
    {
        Options options;
        options.port = 47640;
        options.timeout = std::chrono::milliseconds(800);
        options.natPmpTarget = "127.0.0.1:9"; // nothing answers there
        return options;
    }

    void TestUpnp()
    {
        for (const bool chunked : { false, true })
        {
            FakeIgd igd;
            igd.chunked = chunked;
            Options options = TestOptions();
            options.ssdpTarget = Hostport(igd.ssdpPort);
            options.tryNatPmp = false;
            const Mapping mapping = Map(options);
            Check(mapping.ok && mapping.method == "UPnP" && mapping.externalIp == "203.0.113.7" &&
                      mapping.externalPort == 47640,
                chunked ? "upnp mapping, chunked description" : "upnp mapping");
            Unmap(mapping, options);
            igd.Stop();
            Check(igd.lastAdd.find("<NewInternalClient>127.0.0.1</NewInternalClient>") != std::string::npos &&
                      igd.lastAdd.find("<NewProtocol>UDP</NewProtocol>") != std::string::npos &&
                      igd.lastAdd.find("<NewLeaseDuration>7200</NewLeaseDuration>") != std::string::npos,
                "upnp mapping arguments");
            Check(!igd.actions.empty() && igd.actions.back() == "DeletePortMapping", "upnp unmap");
        }
        {
            FakeIgd igd;
            Options options = TestOptions();
            options.ssdpTarget = Hostport(igd.ssdpPort);
            options.tryNatPmp = false;
            options.alsoTcp = true;
            const Mapping mapping = Map(options);
            Unmap(mapping, options);
            igd.Stop();
            Check(mapping.ok && std::count(igd.actions.begin(), igd.actions.end(), "AddPortMapping") == 2 &&
                      std::count(igd.actions.begin(), igd.actions.end(), "DeletePortMapping") == 2 &&
                      igd.lastAdd.find("<NewProtocol>TCP</NewProtocol>") != std::string::npos,
                "upnp udp and tcp");
        }
        {
            FakeIgd igd;
            igd.addFault = 725;
            Options options = TestOptions();
            options.ssdpTarget = Hostport(igd.ssdpPort);
            options.tryNatPmp = false;
            const Mapping mapping = Map(options);
            igd.Stop();
            Check(mapping.ok && igd.lastAdd.find("<NewLeaseDuration>0</NewLeaseDuration>") != std::string::npos,
                "upnp permanent lease retry");
        }
        {
            FakeIgd igd;
            igd.addFault = 718;
            Options options = TestOptions();
            options.ssdpTarget = Hostport(igd.ssdpPort);
            options.tryNatPmp = false;
            const Mapping mapping = Map(options);
            Check(!mapping.ok && mapping.error.find("another computer") != std::string::npos, "upnp conflict message");
        }
    }

    void TestNatPmp()
    {
        FakeNatPmp gateway;
        Options options = TestOptions();
        options.tryUpnp = false;
        options.natPmpTarget = Hostport(gateway.port);
        const Mapping mapping = Map(options);
        Check(mapping.ok && mapping.method == "NAT-PMP" && mapping.externalIp == "198.51.100.9" &&
                  mapping.externalPort == 47641,
            "nat-pmp mapping, the router's external port kept");
        Unmap(mapping, options);
        gateway.Stop();
        Check(gateway.requests == 4 && gateway.lastLifetime == 0, "nat-pmp unmap");

        FakeNatPmp both;
        options.natPmpTarget = Hostport(both.port);
        options.alsoTcp = true;
        const Mapping twice = Map(options);
        both.Stop();
        Check(twice.ok && twice.externalPort == 47641 && both.requests == 3, "nat-pmp udp and tcp");
    }

    void TestNothingAnswers()
    {
        uint16_t silentPort = 0;
        const Socket silent = Bind(SOCK_DGRAM, silentPort);
        Options options = TestOptions();
        options.ssdpTarget = Hostport(silentPort);
        options.natPmpTarget = Hostport(silentPort);
        options.timeout = std::chrono::milliseconds(200);
        const Mapping mapping = Map(options);
        Check(!mapping.ok && mapping.error.find("UPnP: no UPnP router answered") != std::string::npos &&
                  mapping.error.find("NAT-PMP: no NAT-PMP router answered") != std::string::npos,
            "both methods' reasons when nothing answers");
        CloseSocket(silent);
    }
}

int main()
{
#if defined(_WIN32)
    WSADATA data;
    WSAStartup(MAKEWORD(2, 2), &data);
#endif
    TestParsers();
    TestUpnp();
    TestNatPmp();
    TestNothingAnswers();
    uint32_t gateway = 0;
    if (DefaultGateway(gateway))
        printf("default gateway: %s\n", Ipv4Text(gateway).c_str());
    if (g_failures)
        return 1;
    printf("ArkeyanPortMapTest: all passed\n");
    return 0;
}
