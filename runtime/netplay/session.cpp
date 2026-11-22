#include "netplay/session.h"
#include <algorithm>
#include <bit>
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
        void Append(std::vector<uint8_t>& bytes, uint64_t value, size_t size)
        {
            for (size_t i = 0; i < size; ++i)
                bytes.push_back(uint8_t(value >> (8 * i)));
        }
        uint64_t Take(std::span<const uint8_t> bytes, size_t& at, size_t size)
        {
            uint64_t value = 0;
            for (size_t i = 0; i < size; ++i)
                value |= uint64_t(bytes[at++]) << (8 * i);
            return value;
        }
    }
    struct Session::Repair
    {
        uint64_t frame = 0, start = 0, total = 0;
        std::vector<Block> blocks;
        std::vector<uint8_t> outgoing, incoming, received;
        uint64_t sinceRequest = UINT64_MAX;
        bool prepared = false, verified = false, done = false;
    };
    Session::Session(SessionConfig config, std::unique_ptr<Transport> transport)
        : m_config(std::move(config)),
          m_transport(std::move(transport)),
          m_password(PasswordKey(m_config.password)),
          m_history(std::make_unique<EventHistory>())
    {
        if (!m_config.now)
            m_config.now = Now;
        m_lastReceive = m_config.now();
        m_started = m_pingStart = m_lastReceive;
        m_thread = std::jthread([this](std::stop_token stop) { Run(std::move(stop)); });
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
    Session::Second& Session::Counters()
    {
        const auto second = m_config.now() / 1000;
        auto& bucket = m_seconds[second % m_seconds.size()];
        if (bucket.stamp != second)
        {
            bucket = {};
            bucket.stamp = second;
        }
        return bucket;
    }
    void Session::CountInputWait(uint64_t waited)
    {
        auto& counters = Counters();
        ++counters.frames;
        counters.waitMs += waited;
        counters.waited += waited > 0;
    }
    SessionStats Session::Stats() const
    {
        std::lock_guard lock(m_mutex);
        SessionStats stats;
        stats.roundTripMs = m_rtt == UINT64_MAX ? 0 : m_rtt;
        stats.goodHash = m_goodHash;
        stats.goodChecks = m_goodChecks;
        stats.delay = m_delay;
        stats.peerMenuOpen = m_peerMenuOpen;
        stats.lost = m_lost;
        stats.rejected = m_rejected;
        const auto now = m_config.now();
        stats.resyncs = m_resyncs;
        stats.backInSync = m_resyncs && now - m_lastResync < 2000 && m_failure.reason == EndReason::None;
        stats.repairing = m_repair && !m_repair->done && m_failure.reason == EndReason::None;
        stats.waiting = m_waiting && m_failure.reason == EndReason::None;
        const auto second = now / 1000;
        uint64_t frames = 0, wait = 0, minPing = UINT64_MAX;
        for (const auto& bucket : m_seconds)
            if (bucket.stamp <= second && second - bucket.stamp < 60)
            {
                frames += bucket.frames;
                wait += bucket.waitMs;
                stats.waitedFrames += bucket.waited;
                minPing = std::min(minPing, bucket.minPing);
                stats.maxPingMs = std::max(stats.maxPingMs, bucket.maxPing);
                if (second - bucket.stamp <= 1)
                {
                    stats.sentPerSecond += double(bucket.sent);
                    stats.receivedPerSecond += double(bucket.received);
                    stats.sentKB += double(bucket.sentBytes) / 1024.0;
                    stats.receivedKB += double(bucket.receivedBytes) / 1024.0;
                }
            }
        const double rateSeconds =
            std::max(0.001, double(now - std::max(m_started, second ? (second - 1) * 1000 : 0)) / 1000);
        stats.sentPerSecond /= rateSeconds;
        stats.receivedPerSecond /= rateSeconds;
        stats.sentKB /= rateSeconds;
        stats.receivedKB /= rateSeconds;
        stats.waitMs = frames ? double(wait) / double(frames) : 0;
        stats.minPingMs = minPing == UINT64_MAX ? 0 : minPing;
        const auto begin = m_pingSamplesCount > m_pingSamples.size() ? m_pingSamplesCount - m_pingSamples.size() : 0;
        for (size_t i = begin; i < m_pingSamplesCount; ++i)
        {
            const auto& ping = m_pingSamples[i % m_pingSamples.size()];
            if (now >= ping.time && now - ping.time < 60000)
                stats.pingMs[stats.pingCount++] = float(ping.ms);
        }
        return stats;
    }
    void Session::SetMenuOpen(bool open)
    {
        std::lock_guard lock(m_mutex);
        if (m_menuOpen != open)
        {
            m_menuOpen = open;
            m_changed.notify_all();
        }
    }
    bool Session::ContinuingSolo() const
    {
        std::lock_guard lock(m_mutex);
        return ContinueSolo(m_established, m_failure.reason, m_stopping);
    }
    bool Session::Solo(uint64_t frame, const Pad& local, InputFrame& pads, FrameEvents* events)
    {
        if (!ContinueSolo(m_established, m_failure.reason, m_stopping))
            return false;
        pads = {};
        pads[m_config.hello.host ? 0 : 1] = local;
        if (events)
            for (auto& player : events->players)
                player.count = 0;
        m_history->pending.count = 0;
        m_next = frame + 1;
        return true;
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
    bool Session::Wait(std::unique_lock<std::mutex>& lock, const std::function<bool()>& ready, uint64_t* waited)
    {
        const uint64_t start = m_config.now();
        bool warned = false;
        for (;;)
        {
            if (m_failure.reason != EndReason::None && m_endSends >= 16)
            {
                m_waiting = false;
                return false;
            }
            if (m_failure.reason == EndReason::None && ready())
            {
                m_waiting = false;
                return true;
            }
            const uint64_t elapsed = m_config.now() - start;
            if (m_failure.reason == EndReason::None && elapsed >= m_config.timeoutMs)
                End(m_repair && !m_repair->done ? EndReason::Desync : EndReason::Timeout, m_hashFrame, m_hash,
                    m_peerHash);
            if (!warned && m_failure.reason == EndReason::None && elapsed >= m_config.waitingMs)
            {
                fprintf(stderr, "[netplay] waiting for the other player\n");
                warned = true;
                m_waiting = true;
            }
            const auto before = m_config.now();
            m_changed.wait_for(lock, std::chrono::milliseconds(2));
            if (waited)
                *waited += m_config.now() - before;
        }
    }
    bool Session::WaitReady()
    {
        std::unique_lock lock(m_mutex);
        if (!Wait(lock, [&] { return m_ready && (!m_config.hello.host || m_peerReady); }))
            return false;
        m_established = true;
        return true;
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
        if (m_failure.reason != EndReason::None)
            return Solo(frame, local, pads, events);
        uint64_t inputWait = 0;
        const uint64_t target = frame + m_delay;
        if (!Wait(
                lock,
                [&] {
                    return target <= m_ack + 8 &&
                           m_eventSequence - m_eventAck <= m_history->local.size() - m_history->pending.count;
                },
                &inputWait))
        {
            CountInputWait(inputWait);
            return Solo(frame, local, pads, events);
        }
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
        if (!Wait(
                lock,
                [&] {
                    const auto& remote = m_remote[frame % m_remote.size()];
                    return frame < m_delay || (remote.frame == frame && remote.events <= m_receivedEvent);
                },
                &inputWait))
        {
            CountInputWait(inputWait);
            return Solo(frame, local, pads, events);
        }
        CountInputWait(inputWait);
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
                return Solo(frame, local, pads, events);
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
            StartRepair();
            if (!Wait(lock, [&] { return m_repair && m_repair->done; }))
                return false;
        }
        m_goodHash = frame;
        ++m_goodChecks;
        return true;
    }
    void Session::StartRepair()
    {
        const uint64_t now = m_config.now();
        if (!m_config.blocks || (m_repairAttempts >= 2 && now - m_repairTimes[m_repairAttempts % 2] < 60000))
        {
            End(EndReason::Desync, m_hashFrame, m_hash, m_peerHash);
            return;
        }
        m_repairTimes[m_repairAttempts++ % 2] = now;
        m_repair = std::make_unique<Repair>();
        auto& repair = *m_repair;
        repair.frame = m_hashFrame;
        repair.start = now;
        repair.total = m_hash;
        const auto blocks = m_config.blocks->ListBlocks();
        if (blocks.size() > (MaxBlockListBytes - 4) / 16)
        {
            End(EndReason::Desync, m_hashFrame, m_hash, m_peerHash);
            return;
        }
        repair.blocks.assign(blocks.begin(), blocks.end());
        if (!m_config.hello.host)
        {
            Append(repair.outgoing, blocks.size(), 4);
            for (const auto& block : blocks)
            {
                Append(repair.outgoing, block.address, 4);
                Append(repair.outgoing, uint32_t(block.heap), 4);
                Append(repair.outgoing, block.hash, 8);
            }
        }
    }
    void Session::CompleteRepair()
    {
        if (m_repair->done)
            return;
        m_repair->done = true;
        ++m_resyncs;
        fprintf(stderr, "[netplay] state repaired at frame %llu (%zu bytes sent)\n",
            static_cast<unsigned long long>(m_repair->frame),
            m_config.hello.host ? m_repair->outgoing.size() : m_repair->incoming.size());
        m_lastResync = m_config.now();
        m_changed.notify_all();
    }
    void Session::PrepareRepair()
    {
        auto& repair = *m_repair;
        size_t at = 0;
        const auto fail = [&] { End(EndReason::Desync, repair.frame, m_hash, m_peerHash); };
        if (m_config.hello.host)
        {
            if (repair.incoming.size() != 4 + repair.blocks.size() * 16 ||
                Take(repair.incoming, at, 4) != repair.blocks.size())
                return fail();
            std::vector<size_t> different;
            for (size_t i = 0; i < repair.blocks.size(); ++i)
            {
                const auto address = Take(repair.incoming, at, 4), heap = Take(repair.incoming, at, 4);
                const auto hash = Take(repair.incoming, at, 8);
                if (address != repair.blocks[i].address || heap != uint32_t(repair.blocks[i].heap))
                    return fail();
                if (hash != repair.blocks[i].hash)
                    different.push_back(i);
                if (different.size() > MaxRepairBlocks)
                    return fail();
            }
            Append(repair.outgoing, different.size(), 4);
            std::vector<uint8_t> bytes;
            for (const auto index : different)
            {
                bytes.clear();
                if (!m_config.blocks->ReadBlock(index, bytes) || bytes.empty() || bytes.size() > 65536)
                    return fail();
                Append(repair.outgoing, index, 4);
                Append(repair.outgoing, bytes.size(), 4);
                repair.outgoing.insert(repair.outgoing.end(), bytes.begin(), bytes.end());
            }
        }
        else
        {
            if (repair.incoming.size() < 4)
                return fail();
            const auto count = Take(repair.incoming, at, 4);
            if (count > MaxRepairBlocks)
                return fail();
            // Validate the whole transfer before the first write; peer bytes never choose a guest address.
            size_t previous = 0;
            for (uint64_t i = 0; i < count; ++i)
            {
                if (repair.incoming.size() - at < 8)
                    return fail();
                const auto index = Take(repair.incoming, at, 4), size = Take(repair.incoming, at, 4);
                if (index >= repair.blocks.size() || (i && index <= previous) || !size || size > 65536 ||
                    size > repair.incoming.size() - at)
                    return fail();
                previous = size_t(index);
                at += size_t(size);
            }
            if (at != repair.incoming.size())
                return fail();
            at = 4;
            for (uint64_t i = 0; i < count; ++i)
            {
                const auto index = Take(repair.incoming, at, 4), size = Take(repair.incoming, at, 4);
                if (!m_config.blocks->WriteBlock(size_t(index), std::span(repair.incoming).subspan(at, size_t(size))))
                    return fail();
                at += size_t(size);
            }
            repair.total = m_config.blocks->TotalHash();
        }
        repair.prepared = true;
    }
    void Session::ReceiveRepair(const Packet& packet)
    {
        if (!m_repair || packet.hashFrame != m_repair->frame || m_failure.reason != EndReason::None)
            return;
        auto& repair = *m_repair;
        if (!repair.done && m_config.now() - repair.start >= 5000)
        {
            End(EndReason::Desync, repair.frame, m_hash, m_peerHash);
            return;
        }
        if (packet.type == PacketType::BlockRequest)
        {
            const auto stream = m_config.hello.host ? RepairStream::Data : RepairStream::List;
            if (packet.stream != stream || repair.outgoing.empty())
                return;
            for (uint32_t bit = 0; bit < 64; ++bit)
            {
                const uint64_t offset = uint64_t(packet.chunk.index + bit) * RepairChunkBytes;
                if (!(packet.value & (uint64_t(1) << bit)) || offset >= repair.outgoing.size())
                    continue;
                Packet chunk;
                chunk.type = m_config.hello.host ? PacketType::BlockData : PacketType::BlockList;
                chunk.hashFrame = repair.frame;
                chunk.chunk.total = uint32_t(repair.outgoing.size());
                chunk.chunk.index = packet.chunk.index + bit;
                chunk.chunk.size = uint16_t(std::min(uint64_t(RepairChunkBytes), repair.outgoing.size() - offset));
                std::copy_n(repair.outgoing.begin() + ptrdiff_t(offset), chunk.chunk.size, chunk.chunk.bytes.begin());
                Send(chunk);
            }
        }
        else if (packet.type == (m_config.hello.host ? PacketType::BlockList : PacketType::BlockData))
        {
            if (repair.prepared)
                return;
            const auto& chunk = packet.chunk;
            if (repair.incoming.empty())
            {
                if (m_config.hello.host && chunk.total != 4 + repair.blocks.size() * 16)
                {
                    End(EndReason::Desync, repair.frame, m_hash, m_peerHash);
                    return;
                }
                repair.incoming.resize(chunk.total);
                repair.received.resize((chunk.total + RepairChunkBytes - 1) / RepairChunkBytes);
            }
            if (repair.incoming.size() != chunk.total)
            {
                End(EndReason::Desync, repair.frame, m_hash, m_peerHash);
                return;
            }
            if (!repair.received[chunk.index])
            {
                std::copy_n(chunk.bytes.begin(), chunk.size,
                    repair.incoming.begin() + ptrdiff_t(chunk.index) * ptrdiff_t(RepairChunkBytes));
                repair.received[chunk.index] = 1;
                if (std::all_of(
                        repair.received.begin(), repair.received.end(), [](uint8_t value) { return value != 0; }))
                    PrepareRepair();
            }
        }
        else if (packet.type == PacketType::RepairResult && repair.prepared)
        {
            if (m_config.hello.host && packet.phase == RepairPhase::Verify)
            {
                if (packet.hash != repair.total)
                    End(EndReason::Desync, repair.frame, repair.total, packet.hash);
                else
                    repair.verified = true;
            }
            else if (!m_config.hello.host && packet.phase == RepairPhase::Commit && packet.hash == repair.total)
            {
                repair.verified = true;
                CompleteRepair();
            }
            else if (m_config.hello.host && packet.phase == RepairPhase::Ack && repair.verified &&
                     packet.hash == repair.total)
                CompleteRepair();
        }
    }
    void Session::SendRepair()
    {
        if (!m_repair || m_failure.reason != EndReason::None)
            return;
        auto& repair = *m_repair;
        // New host input proves it received the commit acknowledgement and released its barrier.
        if (repair.done && (m_config.hello.host || m_remoteAck >= repair.frame + m_delay))
            return;
        if (!repair.done && m_config.now() - repair.start >= 5000)
        {
            End(EndReason::Desync, repair.frame, m_hash, m_peerHash);
            return;
        }
        Packet packet;
        packet.hashFrame = repair.frame;
        if (!repair.prepared)
        {
            // Asked again only once the chunks asked for had a round trip to come: asking every loop (2 ms) would
            // have the sender repeat each chunk dozens of times over a slow link. Counted in loops, not in the
            // session's clock, which a test may hold still.
            const uint64_t loops = std::clamp<uint64_t>(m_rtt == UINT64_MAX ? 25 : (m_rtt + 10) / 2, 5, 125);
            if (repair.sinceRequest != UINT64_MAX)
            {
                ++repair.sinceRequest;
                if (repair.sinceRequest < loops)
                    return;
            }
            repair.sinceRequest = 0;
            packet.type = PacketType::BlockRequest;
            packet.stream = m_config.hello.host ? RepairStream::List : RepairStream::Data;
            if (repair.received.empty())
            {
                packet.value = 1;
                Send(packet);
                return;
            }
            // Up to four windows of 64 chunks in flight per round trip.
            size_t from = 0;
            for (int window = 0; window < 4; ++window)
            {
                const auto missing = std::find(repair.received.begin() + ptrdiff_t(from), repair.received.end(), 0);
                if (missing == repair.received.end())
                    break;
                packet.chunk.index = uint32_t(missing - repair.received.begin());
                packet.value = 0;
                for (size_t i = 0; i < 64 && packet.chunk.index + i < repair.received.size(); ++i)
                    if (!repair.received[packet.chunk.index + i])
                        packet.value |= uint64_t(1) << i;
                Send(packet);
                from = size_t(packet.chunk.index) + 64;
                if (from >= repair.received.size())
                    break;
            }
        }
        else if (!m_config.hello.host || repair.verified)
        {
            packet.type = PacketType::RepairResult;
            packet.hash = repair.total;
            packet.phase =
                m_config.hello.host ? RepairPhase::Commit : (repair.verified ? RepairPhase::Ack : RepairPhase::Verify);
            Send(packet);
        }
    }
    void Session::Send(Packet packet, bool bootstrap)
    {
        Datagram bytes;
        packet.sequence = ++m_sequence;
        packet.ack = m_remoteAck;
        if (EncodePacket(packet, bootstrap ? HelloKey(m_password, packet.hello) : m_key, bytes))
        {
            if (!m_transport->Send(bytes.View()))
                End(EndReason::Transport);
            else
            {
                auto& counters = Counters();
                ++counters.sent;
                counters.sentBytes += bytes.size;
            }
        }
    }
    bool Session::AcceptSequence(uint64_t sequence)
    {
        if (!sequence)
        {
            ++m_dropped;
            return false;
        }
        if (sequence > m_receivedSequence)
        {
            const uint64_t delta = sequence - m_receivedSequence;
            m_receivedMask = (delta >= 64 ? 0 : m_receivedMask << delta) | 1;
            m_receivedSequence = sequence;
        }
        else
        {
            const uint64_t delta = m_receivedSequence - sequence;
            if (delta >= 64 || (m_receivedMask & (uint64_t(1) << delta)))
            {
                ++m_dropped;
                return false;
            }
            m_receivedMask |= uint64_t(1) << delta;
        }
        m_firstSequence = m_firstSequence ? std::min(m_firstSequence, sequence) : sequence;
        ++m_uniqueReceived;
        const auto span = m_receivedSequence - m_firstSequence + 1;
        const auto width = std::min(span, uint64_t(64));
        const auto mask = width == 64 ? UINT64_MAX : (uint64_t(1) << width) - 1;
        const auto pendingLoss = width - uint64_t(std::popcount(m_receivedMask & mask));
        m_lost = span - m_uniqueReceived - pendingLoss;
        return true;
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
            if (!AcceptSequence(packet.sequence))
                return;
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
        if (!AcceptSequence(packet.sequence))
            return;
        m_lastReceive = m_config.now();
        switch (packet.type)
        {
        case PacketType::BlockList:
        case PacketType::BlockData:
        case PacketType::BlockRequest:
        case PacketType::RepairResult: ReceiveRepair(packet); break;
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
                auto& counters = Counters();
                counters.minPing = std::min(counters.minPing, m_rtt);
                counters.maxPing = std::max(counters.maxPing, m_rtt);
                m_pingSamples[m_pingSamplesCount++ % m_pingSamples.size()] = { m_config.now(), m_rtt };
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
            if (packet.sequence > m_menuSequence)
            {
                m_menuSequence = packet.sequence;
                m_peerMenuOpen = packet.menuOpen;
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
                auto& counters = Counters();
                ++counters.received;
                counters.receivedBytes += std::min(size, bytes.bytes.size());
                if (size > bytes.bytes.size())
                {
                    ++m_dropped;
                    continue;
                }
                Packet packet;
                const bool bootstrap = size > 10 && bytes.bytes[10] == uint8_t(PacketType::Hello);
                const auto view = std::span(bytes.bytes).first(size);
                if (bootstrap ? DecodeHello(view, m_password, packet, m_dropped, &m_rejected)
                              : DecodePacket(view, m_key, packet, m_dropped, &m_rejected))
                    Receive(packet, bootstrap);
            }
            if (!m_transport->Healthy())
                End(EndReason::Transport);
            SendRepair();
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
                    frames.menuOpen = m_menuOpen;
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
            if (m_failure.reason != EndReason::None && m_endSends >= 16)
                m_changed.wait(lock, [&] { return stop.stop_requested(); });
            else
                m_changed.wait_for(lock, std::chrono::milliseconds(2));
        }
    }
}
