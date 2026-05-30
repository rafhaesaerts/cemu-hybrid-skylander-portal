#include "PhysicalPortalBridge.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <string>

#ifdef HAS_LIBUSB
#include <libusb.h>
#endif

namespace nsyshid
{
	namespace
	{
		uint64 NowMs()
		{
			return (uint64)std::chrono::duration_cast<std::chrono::milliseconds>(
					   std::chrono::steady_clock::now().time_since_epoch())
				.count();
		}

		// HID Set_Report (control OUT) parameters used to send commands to the portal.
		constexpr uint8 CTRL_REQUEST_TYPE = 0x21;
		constexpr uint8 CTRL_REQUEST = 0x09;
		constexpr uint16 CTRL_VALUE = 0x0200;
		constexpr uint16 CTRL_INDEX = 0x0000;
		constexpr uint8 EP_INTERRUPT_IN = 0x81;
		constexpr uint32 PACKET_SIZE = 32; // Skylander command/response packet size
		constexpr uint8 CACHE_MAX_RETRIES = 3;
		constexpr uint64 QUERY_TIMEOUT_MS = 250; // wait this long for a block-read reply before retrying
		constexpr uint64 HANDSHAKE_PACE_MS = 60; // gap between paced startup commands

		// Render up to `len` bytes of `buf` as a hex string for diagnostic logging.
		std::string HexDump(const uint8* buf, uint32 len)
		{
			static const char* k = "0123456789ABCDEF";
			std::string s;
			s.reserve(len * 3);
			for (uint32 i = 0; i < len; i++)
			{
				s.push_back(k[buf[i] >> 4]);
				s.push_back(k[buf[i] & 0xF]);
				s.push_back(' ');
			}
			return s;
		}

		// A real Skylander's first block holds a non-zero NUID. An all-zero (or near-zero)
		// figure means we cached garbage / an incomplete read - never hand that to the game.
		bool IsPlausibleFigure(const std::array<uint8, PhysicalPortalBridge::FIGURE_SIZE>& data)
		{
			// NUID (first 4 bytes) must be non-zero.
			if ((data[0] | data[1] | data[2] | data[3]) == 0)
				return false;
			// Require a reasonable amount of non-zero payload overall (real figures are
			// densely populated; a mostly-zero buffer is a failed read).
			uint32 nonZero = 0;
			for (uint8 b : data)
				if (b != 0)
					nonZero++;
			return nonZero >= 64;
		}
	} // namespace

	PhysicalPortalBridge::~PhysicalPortalBridge()
	{
		Stop();
	}

	void PhysicalPortalBridge::SetCallbacks(AddCallback onAdd, RemoveCallback onRemove)
	{
		m_onAdd = std::move(onAdd);
		m_onRemove = std::move(onRemove);
	}

	void PhysicalPortalBridge::SetColor(uint8 r, uint8 g, uint8 b)
	{
		OutCommand cmd;
		cmd.bytes[0] = 'C';
		cmd.bytes[1] = r;
		cmd.bytes[2] = g;
		cmd.bytes[3] = b;
		std::lock_guard lock(m_outboxMutex);
		m_outbox.push(cmd);
	}

	void PhysicalPortalBridge::QueueCommand(const uint8* data, uint32 len)
	{
		OutCommand cmd; // zero-filled
		const uint32 n = std::min<uint32>(len, static_cast<uint32>(cmd.bytes.size()));
		memcpy(cmd.bytes.data(), data, n);
		std::lock_guard lock(m_outboxMutex);
		m_outbox.push(cmd);
	}

	void PhysicalPortalBridge::QueueWrite(uint8 portalIndex, uint8 block, const uint8* data16)
	{
		OutCommand cmd;
		cmd.bytes[0] = 'W';
		cmd.bytes[1] = portalIndex; // VERIFY: real portal may expect (0x10 | portalIndex)
		cmd.bytes[2] = block;
		memcpy(&cmd.bytes[3], data16, BLOCK_SIZE);
		std::lock_guard lock(m_outboxMutex);
		m_outbox.push(cmd);
	}

	void PhysicalPortalBridge::RequestBlock(uint8 portalIndex, uint8 block)
	{
		OutCommand cmd;
		cmd.bytes[0] = 'Q';
		cmd.bytes[1] = 0x10 | (portalIndex & 0x0F); // real portal read: high nibble 0x10 selects "read"
		cmd.bytes[2] = block;
		std::lock_guard lock(m_outboxMutex);
		m_outbox.push(cmd);
	}

#ifdef HAS_LIBUSB
	bool PhysicalPortalBridge::OpenDevice()
	{
		libusb_context* ctx = nullptr;
		const int initRc = libusb_init(&ctx);
		if (initRc != 0)
		{
			cemuLog_log(LogType::Force, "PhysicalPortalBridge::OpenDevice: libusb_init failed ({})", initRc);
			return false;
		}
		libusb_device_handle* handle =
			libusb_open_device_with_vid_pid(ctx, PORTAL_VID, PORTAL_PID);
		if (!handle)
		{
			cemuLog_log(LogType::Force,
							 "PhysicalPortalBridge::OpenDevice: portal {:04X}:{:04X} not found / not openable "
							 "(needs WinUSB driver via Zadig)",
							 PORTAL_VID, PORTAL_PID);
			libusb_exit(ctx);
			return false;
		}
		// Linux: detach the kernel HID driver so we can claim the interface. No-op elsewhere.
		libusb_set_auto_detach_kernel_driver(handle, 1);
		const int claimRc = libusb_claim_interface(handle, 0);
		if (claimRc != 0)
		{
			cemuLog_log(LogType::Force,
							 "PhysicalPortalBridge::OpenDevice: claim_interface(0) failed ({})", claimRc);
			libusb_close(handle);
			libusb_exit(ctx);
			return false;
		}
		m_ctx = ctx;
		m_handle = handle;
		return true;
	}

	void PhysicalPortalBridge::CloseDevice()
	{
		if (m_handle)
		{
			libusb_release_interface(static_cast<libusb_device_handle*>(m_handle), 0);
			libusb_close(static_cast<libusb_device_handle*>(m_handle));
			m_handle = nullptr;
		}
		if (m_ctx)
		{
			libusb_exit(static_cast<libusb_context*>(m_ctx));
			m_ctx = nullptr;
		}
	}

	bool PhysicalPortalBridge::SendControl(const uint8* data, uint32 len)
	{
		if (!m_handle)
			return false;
		const int r = libusb_control_transfer(
			static_cast<libusb_device_handle*>(m_handle), CTRL_REQUEST_TYPE, CTRL_REQUEST,
			CTRL_VALUE, CTRL_INDEX, const_cast<uint8*>(data), static_cast<uint16>(len), 1000);
		return r >= 0;
	}

	int PhysicalPortalBridge::ReadInterrupt(uint8* buf, uint32 len, int timeoutMs)
	{
		if (!m_handle)
			return -1;
		int transferred = 0;
		const int r = libusb_interrupt_transfer(static_cast<libusb_device_handle*>(m_handle),
												EP_INTERRUPT_IN, buf, static_cast<int>(len),
												&transferred, timeoutMs);
		if (r == 0)
			return transferred;
		if (r == LIBUSB_ERROR_TIMEOUT)
			return 0;
		return -1;
	}
#else
	bool PhysicalPortalBridge::OpenDevice() { return false; }
	void PhysicalPortalBridge::CloseDevice() {}
	bool PhysicalPortalBridge::SendControl(const uint8*, uint32) { return false; }
	int PhysicalPortalBridge::ReadInterrupt(uint8*, uint32, int) { return -1; }
#endif // HAS_LIBUSB

	bool PhysicalPortalBridge::Start()
	{
		if (m_running.load())
			return true;
		if (!OpenDevice())
			return false;

		// The paced startup handshake (R / A / C) runs on the worker thread - see InitHandshake.
		m_connected.store(true);
		m_running.store(true);
		m_thread = std::thread(&PhysicalPortalBridge::ThreadMain, this);
		cemuLog_log(LogType::Force, "nsyshid::PhysicalPortalBridge: connected to real portal");
		return true;
	}

	void PhysicalPortalBridge::Stop()
	{
		m_running.store(false);
		if (m_thread.joinable())
			m_thread.join();
		CloseDevice();
		m_connected.store(false);
	}

	void PhysicalPortalBridge::ThreadMain()
	{
		InitHandshake();
		while (m_running.load())
			Step();
	}

	// Read and dispatch any interrupt packets that arrive within `ms`. Used to (a) pace the
	// startup commands and (b) consume the portal's R/A replies.
	void PhysicalPortalBridge::DrainFor(uint32 ms)
	{
		const uint64 until = NowMs() + ms;
		while (m_running.load() && NowMs() < until)
		{
			uint8 buf[64] = {};
			const int n = ReadInterrupt(buf, sizeof(buf), 10);
			if (n > 0)
				HandleIncoming(buf, static_cast<uint32>(n));
		}
	}

	// The real portal drops commands sent back-to-back, so the startup sequence is paced:
	// reset ('R'), activate ('A 01'), then light the LED ('C') so the user can see the bridge
	// is live. A short drain after each command lets the portal process it and reply.
	void PhysicalPortalBridge::InitHandshake()
	{
		uint8 pkt[PACKET_SIZE] = {};

		pkt[0] = 'R';
		SendControl(pkt, PACKET_SIZE);
		DrainFor(HANDSHAKE_PACE_MS);

		std::memset(pkt, 0, sizeof(pkt));
		pkt[0] = 'A';
		pkt[1] = 0x01;
		SendControl(pkt, PACKET_SIZE);
		DrainFor(HANDSHAKE_PACE_MS);

		// LED colour is left to the game: its C/J/L commands are forwarded to the real portal,
		// and StartHybrid pushes the current colour on connect so it matches immediately.
		cemuLog_log(LogType::Force, "nsyshid::PhysicalPortalBridge: handshake complete (R/A sent)");
	}

	void PhysicalPortalBridge::Step()
	{
		// 1. Send at most ONE queued game command (LED / write) per step - the portal drops
		//    commands sent in bursts, so everything to the portal is paced one-per-step.
		{
			std::lock_guard lock(m_outboxMutex);
			if (!m_outbox.empty())
			{
				OutCommand cmd = m_outbox.front();
				m_outbox.pop();
				SendControl(cmd.bytes.data(), static_cast<uint32>(cmd.bytes.size()));
			}
		}

		// 2. If no block-read is outstanding, issue the next one for whichever figure is being
		//    cached (one at a time - this is the key fix for reliable reads).
		if (!m_queryActive)
		{
			for (uint8 p = 0; p < 16; p++)
			{
				if (!m_caching[p])
					continue;
				int nextBlock = -1;
				for (uint8 b = 0; b < BLOCK_COUNT; b++)
				{
					if (!m_cacheGot[p][b])
					{
						nextBlock = b;
						break;
					}
				}
				if (nextBlock < 0)
					continue; // fully cached; completion handled in step 5
				m_querySlot = p;
				m_queryBlock = static_cast<uint8>(nextBlock);
				m_queryRetries = 0;
				m_queryActive = true;
				m_queryDeadlineMs = NowMs() + QUERY_TIMEOUT_MS;
				RequestBlock(p, static_cast<uint8>(nextBlock));
				break;
			}
		}

		// 3. Read one interrupt-IN packet (status push or query reply). A 'Q' reply clears
		//    m_queryActive (see HandleQueryResponse) so the next block can be requested.
		uint8 buf[64] = {};
		const int n = ReadInterrupt(buf, sizeof(buf), 10);
		if (n > 0)
			HandleIncoming(buf, static_cast<uint32>(n));

		// 4. Retry / give up on the outstanding query if it timed out.
		const uint64 now = NowMs();
		if (m_queryActive && now > m_queryDeadlineMs)
		{
			if (m_queryRetries < CACHE_MAX_RETRIES)
			{
				m_queryRetries++;
				m_queryDeadlineMs = now + QUERY_TIMEOUT_MS;
				RequestBlock(m_querySlot, m_queryBlock);
			}
			else
			{
				// Could not read this block. Abort the figure WITHOUT injecting anything; a later
				// status push will retry the whole read cleanly.
				const uint8 p = m_querySlot;
				m_queryActive = false;
				if (m_caching[p])
				{
					m_caching[p] = false;
					cemuLog_log(LogType::Force,
								"nsyshid::PhysicalPortalBridge: figure {} read incomplete at block {} - aborted",
								p, m_queryBlock);
				}
			}
		}

		// 5. Deliver any figure whose blocks are now all cached.
		for (uint8 p = 0; p < 16; p++)
		{
			if (!m_caching[p])
				continue;
			bool complete = true;
			for (uint8 b = 0; b < BLOCK_COUNT; b++)
			{
				if (!m_cacheGot[p][b])
				{
					complete = false;
					break;
				}
			}
			if (!complete)
				continue;

			m_caching[p] = false;
			if (m_queryActive && m_querySlot == p)
				m_queryActive = false;

			// Never hand a failed/garbled read to the game (that is what produced the in-game
			// "a toy has a problem" error).
			if (IsPlausibleFigure(m_cacheData[p]))
			{
				m_present[p] = true;
				cemuLog_log(LogType::Force,
							"nsyshid::PhysicalPortalBridge: figure {} read OK (id {:02X}{:02X}{:02X}{:02X})",
							p, m_cacheData[p][0], m_cacheData[p][1], m_cacheData[p][2], m_cacheData[p][3]);
				if (m_onAdd)
					m_onAdd(p, m_cacheData[p]);
			}
			else
			{
				cemuLog_log(LogType::Force,
							"nsyshid::PhysicalPortalBridge: slot {} cached data implausible - discarded "
							"(first block: {})",
							p, HexDump(m_cacheData[p].data(), BLOCK_SIZE));
			}
		}
	}

	void PhysicalPortalBridge::HandleIncoming(const uint8* buf, uint32 len)
	{
		if (len < 1)
			return;
		switch (buf[0])
		{
		case 'S': // status push (0x53)
			HandleStatus(buf, len);
			break;
		case 'Q': // query reply
			HandleQueryResponse(buf, len);
			break;
		default:
			break; // 'W' ack and others: nothing to do
		}
	}

	void PhysicalPortalBridge::HandleStatus(const uint8* buf, uint32 len)
	{
		if (len < 5)
			return;
		// Log the status only when the status WORD (bytes 1..4) actually changes - byte 5 is a
		// free-running counter, so comparing it would log every single packet.
		{
			static std::array<uint8, 5> s_lastStatus{};
			if (len >= 5 && std::memcmp(buf, s_lastStatus.data(), 5) != 0)
			{
				std::memcpy(s_lastStatus.data(), buf, 5);
				cemuLog_log(LogType::Force, "nsyshid::PhysicalPortalBridge: status changed {}",
								 HexDump(buf, 5));
			}
		}
		// Status word: 2 bits per slot index, low bit = present, slot 0 in the lowest bits
		// (mirrors SkylanderUSB::GetStatus). VERIFY against real hardware.
		const uint32 status = static_cast<uint32>(buf[1]) |
							  (static_cast<uint32>(buf[2]) << 8) |
							  (static_cast<uint32>(buf[3]) << 16) |
							  (static_cast<uint32>(buf[4]) << 24);
		for (uint8 p = 0; p < 16; p++)
		{
			const bool present = ((status >> (p * 2)) & 0x1) != 0;
			if (present)
			{
				if (!m_present[p] && !m_caching[p])
				{
					// New arrival: begin caching. Step() issues the block reads one at a time.
					m_caching[p] = true;
					m_cacheGot[p].fill(false);
					m_cacheData[p].fill(0);
					cemuLog_log(LogType::Force, "nsyshid::PhysicalPortalBridge: figure arrived on slot {} - reading", p);
				}
			}
			else // not present
			{
				if (m_caching[p])
				{
					// Removed mid-read: abort cleanly.
					m_caching[p] = false;
					if (m_queryActive && m_querySlot == p)
						m_queryActive = false;
				}
				if (m_present[p])
				{
					m_present[p] = false;
					if (m_onRemove)
						m_onRemove(p);
				}
			}
		}
	}

	void PhysicalPortalBridge::HandleQueryResponse(const uint8* buf, uint32 len)
	{
		if (len < 3u + BLOCK_SIZE)
			return;
		const uint8 idxByte = buf[1];
		const uint8 p = idxByte & 0xF; // reply index byte is 0x10|idx on success, idx on failure
		const uint8 block = buf[2];
		if (p >= 16 || block >= BLOCK_COUNT)
			return;
		if (!m_caching[p])
			return;
		// 0x10 in the index byte means the read succeeded (figure present & block returned).
		// If it is missing, the figure was not readable yet - leave the block un-got and let the
		// paced retry try again rather than caching empty data.
		if ((idxByte & 0x10) == 0)
		{
			if (m_queryActive && m_querySlot == p && m_queryBlock == block)
				m_queryActive = false; // allow re-issue
			return;
		}
		memcpy(m_cacheData[p].data() + (block * BLOCK_SIZE), &buf[3], BLOCK_SIZE);
		m_cacheGot[p][block] = true;
		if (m_queryActive && m_querySlot == p && m_queryBlock == block)
			m_queryActive = false; // reply received -> Step issues the next block
	}
} // namespace nsyshid
