#include "netplay/session.h"
#include <algorithm>
#include <chrono>
#include <cstdio>

namespace netplay
{
    namespace
    {
        uint64_t Now()
        {
            return uint64_t(std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch())
                    .count());
        }
    }
    Session::Session(SessionConfig config, std::unique_ptr<Transport> transport)
        : m_config(std::move(config)),
          m_transport(std::move(transport)),
          m_password(PasswordKey(m_config.password)),
          m_history(std::make_unique<EventHistory>())
    {
        if (!m_config.now)
            m_config.now = Now;
        m_lastReceive = m_config.now();
        m_pingStart = m_lastReceive;
        m_thread = std::jthread([this](std::stop_token stop) { Run(stop); });
    }
    Session::~Session()
    {
        Stop();
        m_thread.request_stop();
        m_changed.notify_all();
        m_thread.join();
    }
    uint32_t Session::Delay() const
    {
        std::lock_guard lock(m_mutex);
        return m_delay;
    }
    SessionFailure Session::Failure() const
    {
        std::lock_guard lock(m_mutex);
        return m_failure;
    }
    uint64_t Session::Dropped() const
    {
        std::lock_guard lock(m_mutex);
        return m_dropped;
    }
    void Session::End(EndReason reason, uint64_t frame, uint64_t local, uint64_t remote)
    {
        if (m_failure.reason == EndReason::None)
        {
            m_failure = { reason, frame, local, remote };
            m_changed.notify_all();
        }
    }
    void Session::Stop()
    {
        std::unique_lock lock(m_mutex);
        m_stopping = true;
        End(EndReason::Quit);
        m_changed.wait(lock, [&] { return m_endSends >= 16; });
    }
    bool Session::Wait(std::unique_lock<std::mutex>& lock, const std::function<bool()>& ready)
    {
        const uint64_t start = m_config.now();
        bool warned = false;
        for (;;)
        {
            if (m_failure.reason != EndReason::None && m_endSends >= 16)
                return false;
            if (m_failure.reason == EndReason::None && ready())
                return true;
            const uint64_t elapsed = m_config.now() - start;
            if (m_failure.reason == EndReason::None && elapsed >= m_config.timeoutMs)
                End(EndReason::Timeout);
            if (!warned && m_failure.reason == EndReason::None && elapsed >= m_config.waitingMs)
            {
                fprintf(stderr, "[netplay] waiting for the other player\n");
                warned = true;
            }
            m_changed.wait_for(lock, std::chrono::milliseconds(2));
        }
    }
    bool Session::WaitReady()
    {
        std::unique_lock lock(m_mutex);
        return Wait(lock, [&] { return m_ready && (!m_config.hello.host || m_peerReady); });
    }
    void Session::SetDelay(uint32_t delay)
    {
        if (m_delay)
            return;
        m_delay = delay;
        m_ack = m_remoteAck = delay - 1;
        m_ready = true;
        m_changed.notify_all();
    }
    uint64_t Session::RoundTripMs() const
    {
        std::lock_guard lock(m_mutex);
        return m_rtt == UINT64_MAX ? 0 : m_rtt;
    }
    uint64_t Session::LastGoodHash() const
    {
        std::lock_guard lock(m_mutex);
        return m_goodHash;
    }
    bool Session::QueueEvents(std::span<const PortalEvent> events)
    {
        std::lock_guard lock(m_mutex);
        if (m_failure.reason != EndReason::None || events.empty() || !events.back().last ||
            events.size() > MaxFrameEvents - m_history->pending.count)
            return false;
        for (const auto& event : events)
            if (event.slot >= 16 || !SeatAllowed(m_config.hello.host, m_config.slotPlayers[event.slot]))
                return false;
        for (const auto& event : events)
            m_history->pending.events[m_history->pending.count++] = event;
        return true;
    }
    bool Session::Exchange(uint64_t frame, const Pad& local, InputFrame& pads, FrameEvents* events)
    {
        std::unique_lock lock(m_mutex);
        if (!m_ready || frame != m_next || frame > UINT64_MAX - 64)
        {
            End(EndReason::Invalid);
            return false;
        }
        const uint64_t target = frame + m_delay;
        if (!Wait(lock, [&] {
                return target <= m_ack + 8 &&
                       m_eventSequence - m_eventAck <= m_history->local.size() - m_history->pending.count;
            }))
            return false;
        for (size_t i = 0; i < m_history->pending.count; ++i)
        {
            auto event = m_history->pending.events[i];
            event.sequence = ++m_eventSequence;
            event.frame = target;
            m_history->local[event.sequence % m_history->local.size()] = event;
        }
        m_history->pending.count = 0;
        m_local[target % m_local.size()] = { target, local, m_eventSequence };
        m_latest = target;
        m_changed.notify_all();
        if (!Wait(lock, [&] {
                const auto& remote = m_remote[frame % m_remote.size()];
                return frame < m_delay || (remote.frame == frame && remote.events <= m_receivedEvent);
            }))
            return false;
        pads = {};
        if (!events && m_config.applyEvents)
            events = &m_history->application;
        if (events)
            for (auto& player : events->players)
                player.count = 0;
        if (frame >= m_delay)
        {
            const size_t seat = m_config.hello.host ? 0 : 1;
            pads[seat] = m_local[frame % m_local.size()].pad;
            pads[1 - seat] = m_remote[frame % m_remote.size()].pad;
            const uint64_t remoteEnd = m_remote[frame % m_remote.size()].events;
            if (remoteEnd < m_appliedEvent || remoteEnd - m_appliedEvent > MaxFrameEvents)
            {
                End(EndReason::Invalid);
                return false;
            }
            for (uint64_t sequence = m_appliedEvent + 1; sequence <= remoteEnd; ++sequence)
            {
                const auto& event = m_history->remote[sequence % m_history->remote.size()];
                if (event.sequence != sequence || event.frame != frame || (sequence == remoteEnd && !event.last))
                {
                    End(EndReason::Invalid);
                    return false;
                }
                if (events)
                    events->players[1 - seat].events[events->players[1 - seat].count++] = event;
            }
            m_appliedEvent = remoteEnd;
            if (events)
            {
                const uint64_t localEnd = m_local[frame % m_local.size()].events;
                const uint64_t localBegin = frame == m_delay ? 0 : m_local[(frame - 1) % m_local.size()].events;
                for (uint64_t sequence = localBegin + 1; sequence <= localEnd; ++sequence)
                    events->players[seat].events[events->players[seat].count++] =
                        m_history->local[sequence % m_history->local.size()];
            }
        }
        if (m_config.applyEvents && events && (events->players[0].count || events->players[1].count))
        {
            if (events != &m_history->application)
                m_history->application = *events;
            m_applyDone = false;
            m_applyRequested = true;
            m_changed.notify_all();
            if (!Wait(lock, [&] { return m_applyDone; }))
                return false;
        }
        ++m_next;
        return true;
    }
    bool Session::CheckHash(uint64_t frame, uint64_t hash)
    {
        std::unique_lock lock(m_mutex);
        if (!frame || frame % 30 || frame != m_next || frame <= m_hashFrame)
        {
            End(EndReason::Invalid);
            return false;
        }
        m_hashFrame = frame;
        m_hash = hash;
        if (m_peerEnd.reason == EndReason::Desync && m_peerEnd.frame == frame)
            End(EndReason::Desync, frame, hash, m_peerEnd.remoteHash);
        m_changed.notify_all();
        if (!Wait(lock, [&] { return m_peerHashFrame == frame; }))
            return false;
        if (m_peerHash != hash)
        {
            End(EndReason::Desync, frame, hash, m_peerHash);
            Wait(lock, [] { return false; });
            return false;
        }
        m_goodHash = frame;
        return true;
    }
    void Session::Send(Packet packet, bool bootstrap)
    {
        Datagram bytes;
        packet.sequence = ++m_sequence;
        packet.ack = m_remoteAck;
        if (EncodePacket(packet, bootstrap ? HelloKey(m_password, packet.hello) : m_key, bytes) &&
            !m_transport->Send(bytes.View()))
            End(EndReason::Transport);
    }
    void Session::Receive(const Packet& packet, bool bootstrap)
    {
        if (bootstrap)
        {
            if (packet.type != PacketType::Hello || (m_havePeer && packet.hello.nonce != m_peer.nonce) ||
                (packet.hello.echo != Nonce{} && packet.hello.echo != m_config.hello.nonce))
            {
                ++m_dropped;
                return;
            }
            m_transport->AcceptPeer();
            m_peer = packet.hello;
            m_havePeer = true;
            m_key = m_config.hello.host ? SessionKey(m_password, m_config.hello.nonce, m_peer.nonce)
                                        : SessionKey(m_password, m_peer.nonce, m_config.hello.nonce);
            const auto problem =
                m_config.hello.host == m_peer.host ? EndReason::Seats : CompareHello(m_config.hello, m_peer);
            if (problem != EndReason::None)
                End(problem);
            m_lastReceive = m_config.now();
            return;
        }
        if (!m_havePeer)
            return;
        if (packet.sequence > m_receivedSequence)
        {
            const uint64_t delta = packet.sequence - m_receivedSequence;
            m_receivedMask = (delta >= 64 ? 0 : m_receivedMask << delta) | 1;
            m_receivedSequence = packet.sequence;
        }
        else
        {
            const uint64_t delta = m_receivedSequence - packet.sequence;
            if (delta >= 64 || (m_receivedMask & (uint64_t(1) << delta)))
            {
                ++m_dropped;
                return;
            }
            m_receivedMask |= uint64_t(1) << delta;
        }
        m_lastReceive = m_config.now();
        switch (packet.type)
        {
        case PacketType::Hello: ++m_dropped; break;
        case PacketType::Ping:
        {
            Packet pong;
            pong.type = PacketType::Pong;
            pong.value = packet.value;
            Send(pong);
        }
        break;
        case PacketType::Pong:
            if ((m_config.hello.host || m_ready) && packet.value == m_ping)
            {
                m_rtt = m_ready ? m_config.now() - m_pingStart : std::min(m_rtt, m_config.now() - m_pingStart);
                ++m_pings;
                ++m_ping;
                m_pingStart = m_config.now();
                if (!m_ready && m_pings == 5)
                    SetDelay(m_config.delay ? m_config.delay : ChooseDelay(m_rtt, m_config.hello.fps));
            }
            break;
        case PacketType::Ready:
            if (packet.value < 1 || packet.value > 8 || (m_delay && m_delay != packet.value) ||
                (m_config.hello.host && !m_ready))
            {
                ++m_dropped;
                break;
            }
            if (!m_config.hello.host)
                SetDelay(uint32_t(packet.value));
            m_peerReady = true;
            break;
        case PacketType::Frames:
            if (!m_ready || packet.eventAck > m_eventSequence ||
                packet.ack > (m_latest == UINT64_MAX ? m_delay - 1 : m_latest))
            {
                ++m_dropped;
                break;
            }
            for (size_t i = 0; i < packet.count; ++i)
                if (packet.pads[i].frame < m_delay || packet.pads[i].frame >= m_next + 32 ||
                    (i && packet.pads[i].frame != packet.pads[i - 1].frame + 1))
                {
                    ++m_dropped;
                    return;
                }
            if (packet.hashFrame && (packet.hashFrame % 30 || packet.hashFrame > m_next + 32))
            {
                ++m_dropped;
                break;
            }
            m_ack = std::max(m_ack, packet.ack);
            m_eventAck = std::max(m_eventAck, packet.eventAck);
            for (size_t i = 0; i < packet.count; ++i)
            {
                const auto& entry = packet.pads[i];
                if (entry.frame >= m_next)
                    m_remote[entry.frame % m_remote.size()] = entry;
            }
            while (m_remote[(m_remoteAck + 1) % m_remote.size()].frame == m_remoteAck + 1)
                ++m_remoteAck;
            if (packet.hashFrame >= m_peerHashFrame)
            {
                m_peerHashFrame = packet.hashFrame;
                m_peerHash = packet.hash;
            }
            break;
        case PacketType::Event:
        {
            const auto& event = packet.event;
            if (!m_ready || event.sequence <= m_receivedEvent)
                break;
            if (event.sequence != m_receivedEvent + 1)
                break;
            if (event.frame < m_next || event.frame >= m_next + 32 ||
                event.sequence - m_appliedEvent > m_history->remote.size() ||
                !SeatAllowed(!m_config.hello.host, m_config.slotPlayers[event.slot]) ||
                (m_config.prepareEvent && !m_config.prepareEvent(!m_config.hello.host, event)))
            {
                End(EndReason::Invalid);
                break;
            }
            m_history->remote[event.sequence % m_history->remote.size()] = event;
            m_receivedEvent = event.sequence;
            break;
        }
        case PacketType::End:
            if (packet.reason == EndReason::Desync &&
                (!packet.hashFrame || packet.hashFrame % 30 || packet.hashFrame > m_next + 32))
            {
                ++m_dropped;
                break;
            }
            if (packet.reason == EndReason::Desync && packet.hashFrame != m_hashFrame)
                m_peerEnd = { packet.reason, packet.hashFrame, packet.value, packet.hash };
            else
                End(packet.reason, packet.hashFrame, packet.value, packet.hash);
            break;
        }
        m_changed.notify_all();
    }
    void Session::Run(std::stop_token stop)
    {
        std::unique_lock lock(m_mutex);
        while (!stop.stop_requested())
        {
            if (m_applyRequested && m_failure.reason == EndReason::None)
            {
                if (!m_config.applyEvents(m_history->application))
                    End(EndReason::Invalid);
                m_applyRequested = false;
                m_applyDone = true;
                m_changed.notify_all();
            }
            for (unsigned i = 0; i < 64; ++i)
            {
                Datagram bytes;
                const size_t size = m_transport->Receive(bytes.bytes);
                if (size == Transport::NoDatagram)
                    break;
                if (size > bytes.bytes.size())
                {
                    ++m_dropped;
                    continue;
                }
                Packet packet;
                const bool bootstrap = size > 10 && bytes.bytes[10] == uint8_t(PacketType::Hello);
                const auto view = std::span(bytes.bytes).first(size);
                if (bootstrap ? DecodeHello(view, m_password, packet, m_dropped)
                              : DecodePacket(view, m_key, packet, m_dropped))
                    Receive(packet, bootstrap);
            }
            if (!m_transport->Healthy())
                End(EndReason::Transport);
            if (m_config.now() - m_lastReceive >= m_config.timeoutMs)
                End(EndReason::Timeout);
            if ((!m_ready || m_failure.reason != EndReason::None) && (m_havePeer || !m_config.hello.host))
            {
                Packet hello;
                hello.hello = m_config.hello;
                if (m_havePeer)
                    hello.hello.echo = m_peer.nonce;
                Send(hello, true);
            }
            if (m_havePeer)
            {
                if (m_failure.reason == EndReason::None &&
                    ((m_config.hello.host && !m_ready) || (m_ready && m_config.now() - m_lastPingSent >= 1000)))
                {
                    if (m_ready)
                    {
                        ++m_ping;
                        m_pingStart = m_config.now();
                    }
                    m_lastPingSent = m_config.now();
                    Packet ping;
                    ping.type = PacketType::Ping;
                    ping.value = m_ping;
                    Send(ping);
                }
                if (m_ready)
                {
                    Packet ready;
                    ready.type = PacketType::Ready;
                    ready.value = m_delay;
                    Send(ready);
                    Packet frames;
                    frames.type = PacketType::Frames;
                    if (m_latest != UINT64_MAX)
                    {
                        const uint64_t begin = std::max(uint64_t(m_delay), m_latest > 7 ? m_latest - 7 : 0);
                        for (uint64_t frame = begin; frame <= m_latest; ++frame)
                            frames.pads[frames.count++] = m_local[frame % m_local.size()];
                    }
                    frames.hashFrame = m_hashFrame;
                    frames.hash = m_hash;
                    frames.eventAck = m_receivedEvent;
                    Send(frames);
                    const uint64_t end = std::min(m_eventSequence, m_eventAck + 8);
                    for (uint64_t sequence = m_eventAck + 1; sequence <= end; ++sequence)
                    {
                        Packet event;
                        event.type = PacketType::Event;
                        event.event = m_history->local[sequence % m_history->local.size()];
                        Send(event);
                    }
                }
                if (m_failure.reason != EndReason::None)
                {
                    Packet end;
                    end.type = PacketType::End;
                    end.reason = m_failure.reason;
                    end.hashFrame = m_failure.frame;
                    end.hash = m_failure.localHash;
                    end.value = m_failure.remoteHash;
                    Send(end);
                }
            }
            if (m_failure.reason != EndReason::None)
            {
                m_endSends = std::min(m_endSends + 1, 16u);
                m_changed.notify_all();
                if (m_endSends >= 16 && !m_notifiedEnd && !m_stopping && m_config.ended)
                {
                    m_notifiedEnd = true;
                    const auto failure = m_failure;
                    lock.unlock();
                    m_config.ended(failure);
                    lock.lock();
                }
            }
            m_changed.wait_for(lock, std::chrono::milliseconds(2));
        }
    }
}
