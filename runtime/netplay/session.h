#pragma once

#include "netplay/protocol.h"
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

namespace netplay
{
    class Transport
    {
    public:
        static constexpr size_t NoDatagram = SIZE_MAX;
        virtual ~Transport() = default;
        virtual bool Send(std::span<const uint8_t> bytes) = 0;
        virtual size_t Receive(std::span<uint8_t> bytes) = 0;
        virtual void AcceptPeer() {}
        virtual bool Healthy() const { return true; }
        virtual uint16_t Port() const { return 0; }
    };
    std::unique_ptr<Transport> OpenUdp(std::string_view host, uint16_t port, bool listen, std::string& error);
    bool ParseEndpoint(std::string_view text, std::string& host, uint16_t& port);
    struct SessionFailure
    {
        EndReason reason = EndReason::None;
        uint64_t frame = 0, localHash = 0, remoteHash = 0;
    };
    struct SessionConfig
    {
        Hello hello{};
        std::string password;
        uint32_t delay = 0;
        uint64_t timeoutMs = 15000, waitingMs = 1000;
        std::function<uint64_t()> now;
        std::function<void(const SessionFailure&)> ended;
        std::array<uint8_t, 16> slotPlayers{};
        std::function<bool(bool, const PortalEvent&)> prepareEvent;
        std::function<bool(const FrameEvents&)> applyEvents;
    };
    class Session
    {
    public:
        Session(SessionConfig config, std::unique_ptr<Transport> transport);
        ~Session();
        bool WaitReady();
        bool Exchange(uint64_t frame, const Pad& local, InputFrame& pads, FrameEvents* events = nullptr);
        bool QueueEvents(std::span<const PortalEvent> events);
        uint64_t RoundTripMs() const;
        uint64_t LastGoodHash() const;
        bool CheckHash(uint64_t frame, uint64_t hash);
        uint32_t Delay() const;
        SessionFailure Failure() const;
        uint64_t Dropped() const;
        void Stop();

    private:
        void Run(std::stop_token stop);
        void Receive(const Packet& packet, bool bootstrap);
        void Send(Packet packet, bool bootstrap = false);
        void End(EndReason reason, uint64_t frame = 0, uint64_t local = 0, uint64_t remote = 0);
        void SetDelay(uint32_t delay);
        bool Wait(std::unique_lock<std::mutex>& lock, const std::function<bool()>& ready);
        SessionConfig m_config;
        std::unique_ptr<Transport> m_transport;
        Key m_password{}, m_key{};
        mutable std::mutex m_mutex;
        std::condition_variable m_changed;
        Hello m_peer{};
        bool m_havePeer = false, m_ready = false, m_peerReady = false, m_notifiedEnd = false;
        bool m_stopping = false;
        uint32_t m_delay = 0, m_pings = 0, m_endSends = 0;
        uint64_t m_sequence = 0, m_dropped = 0, m_lastReceive = 0, m_rtt = UINT64_MAX;
        uint64_t m_ping = 1, m_pingStart = 0;
        uint64_t m_next = 0, m_latest = UINT64_MAX, m_ack = 0, m_remoteAck = 0;
        std::array<FramePad, 64> m_local{}, m_remote{};
        struct EventHistory
        {
            EventBatch pending{};
            FrameEvents application{};
            std::array<PortalEvent, 512> local{}, remote{};
        };
        std::unique_ptr<EventHistory> m_history;
        bool m_applyRequested = false, m_applyDone = false;
        uint64_t m_eventSequence = 0, m_eventAck = 0, m_receivedEvent = 0, m_appliedEvent = 0;
        uint64_t m_goodHash = 0;
        uint64_t m_lastPingSent = 0;
        uint64_t m_hashFrame = 0, m_hash = 0, m_peerHashFrame = 0, m_peerHash = 0;
        uint64_t m_receivedSequence = 0, m_receivedMask = 0;
        SessionFailure m_failure{}, m_peerEnd{};
        std::jthread m_thread;
    };
}
