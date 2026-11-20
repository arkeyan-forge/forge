#include "netplay/session.h"
#include <charconv>
#include <cstring>

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
using Socket = SOCKET;
using SocketLength = int;
static constexpr Socket InvalidSocket = INVALID_SOCKET;
#else
#include <cerrno>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
using Socket = int;
using SocketLength = socklen_t;
static constexpr Socket InvalidSocket = -1;
#endif

namespace netplay
{
    namespace
    {
        void Close(Socket socket)
        {
#if defined(_WIN32)
            closesocket(socket);
#else
            close(socket);
#endif
        }
        bool WouldBlock()
        {
#if defined(_WIN32)
            const int error = WSAGetLastError();
            return error == WSAEWOULDBLOCK || error == WSAEINTR || error == WSAECONNRESET;
#else
            return errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR || errno == ECONNREFUSED;
#endif
        }
        class Udp final : public Transport
        {
        public:
            Udp(Socket socket, bool listen, uint16_t port) : m_socket(socket), m_listen(listen), m_port(port) {}
            ~Udp() override { Close(m_socket); }
            uint16_t Port() const override { return m_port; }
            bool Healthy() const override { return m_healthy; }
            void AcceptPeer() override
            {
                if (!m_accepted && m_candidateLength)
                {
                    m_peer = m_candidate;
                    m_peerLength = m_candidateLength;
                    m_accepted = true;
                }
            }
            bool Send(std::span<const uint8_t> bytes) override
            {
                if (m_listen && !m_candidateLength)
                    return true;
                const auto* data = reinterpret_cast<const char*>(bytes.data());
                const auto& address = m_accepted ? m_peer : m_candidate;
                const SocketLength length = m_accepted ? m_peerLength : m_candidateLength;
                const auto sent = m_listen ? sendto(m_socket, data, int(bytes.size()), 0,
                                                 reinterpret_cast<const sockaddr*>(&address), length)
                                           : send(m_socket, data, int(bytes.size()), 0);
                return sent == int(bytes.size()) || (sent < 0 && WouldBlock());
            }
            size_t Receive(std::span<uint8_t> bytes) override
            {
                sockaddr_storage address{};
                SocketLength length = sizeof(address);
                const auto got = recvfrom(m_socket, reinterpret_cast<char*>(bytes.data()), int(bytes.size()), 0,
                    reinterpret_cast<sockaddr*>(&address), &length);
                if (got < 0)
                {
#if defined(_WIN32)
                    if (WSAGetLastError() == WSAEMSGSIZE)
                        return bytes.size() + 1;
#endif
                    m_healthy = WouldBlock();
                    return NoDatagram;
                }
                if (m_accepted && (length != m_peerLength || std::memcmp(&address, &m_peer, size_t(length)) != 0))
                    return bytes.size() + 1;
                m_candidate = address;
                m_candidateLength = length;
                return size_t(got);
            }

        private:
            Socket m_socket;
            bool m_listen, m_accepted = false, m_healthy = true;
            uint16_t m_port;
            sockaddr_storage m_peer{}, m_candidate{};
            SocketLength m_peerLength = 0, m_candidateLength = 0;
        };
        bool Prepare(Socket socket)
        {
#if defined(_WIN32)
            SetHandleInformation(reinterpret_cast<HANDLE>(socket), HANDLE_FLAG_INHERIT, 0);
            u_long nonblocking = 1;
            return ioctlsocket(socket, FIONBIO, &nonblocking) == 0;
#else
            return fcntl(socket, F_SETFD, FD_CLOEXEC) == 0 && fcntl(socket, F_SETFL, O_NONBLOCK) == 0;
#endif
        }
    }
    bool ParseEndpoint(std::string_view text, std::string& host, uint16_t& port)
    {
        const size_t separator = text.rfind(':');
        if (separator == std::string_view::npos || separator == 0)
            return false;
        auto name = text.substr(0, separator);
        if (name.front() == '[' && name.back() == ']')
            name = name.substr(1, name.size() - 2);
        else if (name.find(':') != std::string_view::npos)
            return false;
        uint32_t number = 0;
        const auto tail = text.substr(separator + 1);
        const auto [end, error] = std::from_chars(tail.data(), tail.data() + tail.size(), number);
        if (name.empty() || error != std::errc{} || end != tail.data() + tail.size() || number == 0 || number > 65535)
            return false;
        host = name;
        port = uint16_t(number);
        return true;
    }
    std::unique_ptr<Transport> OpenUdp(std::string_view host, uint16_t port, bool listen, std::string& error)
    {
#if defined(_WIN32)
        static const bool started = [] {
            WSADATA data{};
            return WSAStartup(MAKEWORD(2, 2), &data) == 0;
        }();
        if (!started)
        {
            error = "Winsock startup failed";
            return {};
        }
#endif
        addrinfo hints{};
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_DGRAM;
        hints.ai_protocol = IPPROTO_UDP;
        hints.ai_flags = listen ? AI_PASSIVE : 0;
        const std::string name(host), service = std::to_string(port);
        addrinfo* addresses = nullptr;
        const int resolved = getaddrinfo(listen ? nullptr : name.c_str(), service.c_str(), &hints, &addresses);
        if (resolved != 0)
        {
            error = "cannot resolve UDP address";
            return {};
        }
        std::unique_ptr<Transport> result;
        // A dual-stack listener also accepts IPv4 joiners; IPv4 remains a fallback on hosts without IPv6.
        for (int family : { AF_INET6, AF_INET })
            for (auto* address = addresses; address && !result; address = address->ai_next)
            {
                if (address->ai_family != family)
                    continue;
                Socket socket = ::socket(family, SOCK_DGRAM, IPPROTO_UDP);
                if (socket == InvalidSocket)
                    continue;
                if (listen && family == AF_INET6)
                {
                    const int dual = 0;
                    setsockopt(socket, IPPROTO_IPV6, IPV6_V6ONLY, reinterpret_cast<const char*>(&dual), sizeof(dual));
                }
                const bool opened = listen ? bind(socket, address->ai_addr, SocketLength(address->ai_addrlen)) == 0
                                           : connect(socket, address->ai_addr, SocketLength(address->ai_addrlen)) == 0;
                sockaddr_storage local{};
                SocketLength length = sizeof(local);
                if (!opened || !Prepare(socket) ||
                    getsockname(socket, reinterpret_cast<sockaddr*>(&local), &length) != 0)
                {
                    Close(socket);
                    continue;
                }
                const uint16_t bound = local.ss_family == AF_INET6
                                           ? ntohs(reinterpret_cast<const sockaddr_in6*>(&local)->sin6_port)
                                           : ntohs(reinterpret_cast<const sockaddr_in*>(&local)->sin_port);
                result = std::make_unique<Udp>(socket, listen, bound);
            }
        freeaddrinfo(addresses);
        if (!result)
        {
#if defined(_WIN32)
            error = "cannot open or bind UDP socket: Winsock " + std::to_string(WSAGetLastError());
#else
            error = std::string("cannot open or bind UDP socket: ") + std::strerror(errno);
#endif
        }
        return result;
    }
}
