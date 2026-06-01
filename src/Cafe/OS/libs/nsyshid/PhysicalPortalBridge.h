#pragma once

#include <array>
#include <atomic>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>

#include "Common/precompiled.h"

namespace nsyshid
{
	// Bridges Cemu's emulated Skylander portal to a REAL Portal of Power (1430:0150) over
	// libusb, so that figures physically placed on the real portal are merged into the
	// emulated 16-slot view alongside virtual (dump-backed) figures. This is the core of
	// "hybrid" mode (see HYBRID_PORTAL_INTEGRATION.md).
	//
	// It owns its OWN libusb context + worker thread, intentionally separate from
	// BackendLibusb (which is told to skip 1430:0150 while hybrid mode is active, so the
	// bridge is the device's exclusive owner). All libusb specifics live in the .cpp behind
	// HAS_LIBUSB; the handles here are opaque (void*) so this header pulls in no libusb
	// header and Skylander.cpp compiles even in builds without libusb.
	//
	// NOTE: the live USB behaviour (status-byte layout, query/response index encoding,
	// packet sizes) is modelled on Cemu's emulated SkylanderUSB and the published protocol,
	// but has NOT been validated against real hardware in this environment. Points that may
	// need a one-line tweak during hardware bring-up are marked "VERIFY:".
	class PhysicalPortalBridge
	{
	  public:
		static constexpr uint16 PORTAL_VID = 0x1430;
		static constexpr uint16 PORTAL_PID = 0x0150;
		static constexpr uint32 FIGURE_SIZE = 0x40 * 0x10; // 1024 bytes (== SKY_FIGURE_SIZE)
		static constexpr uint8 BLOCK_COUNT = 0x40;
		static constexpr uint8 BLOCK_SIZE = 0x10;

		// Invoked from the worker thread when a figure appears / disappears on the real
		// portal. portalIndex is the real portal's slot index (0..15); data is the full image.
		using AddCallback = std::function<void(uint8 portalIndex, const std::array<uint8, FIGURE_SIZE>& data)>;
		using RemoveCallback = std::function<void(uint8 portalIndex)>;

		PhysicalPortalBridge() = default;
		~PhysicalPortalBridge();

		PhysicalPortalBridge(const PhysicalPortalBridge&) = delete;
		PhysicalPortalBridge& operator=(const PhysicalPortalBridge&) = delete;

		void SetCallbacks(AddCallback onAdd, RemoveCallback onRemove);

		// Open the real portal + start the worker. Returns false when libusb is unavailable
		// or no portal is connected (the caller then simply stays purely emulated).
		bool Start();
		void Stop();
		bool IsConnected() const { return m_connected.load(); }

		// Game-side actions forwarded to the real portal (thread-safe; drained by the worker).
		void SetColor(uint8 r, uint8 g, uint8 b);
		void QueueWrite(uint8 portalIndex, uint8 block, const uint8* data16);
		// Forward a raw portal command verbatim (used to mirror the game's LED commands
		// C/J/L so the real portal lights up exactly like the emulated one).
		void QueueCommand(const uint8* data, uint32 len);

	  private:
		struct OutCommand
		{
			std::array<uint8, 32> bytes{};
		};

		void ThreadMain();
		void InitHandshake();                       // paced R / A / C startup sequence
		void DrainFor(uint32 ms);                   // read+dispatch incoming packets for a while
		void Step();
		void HandleIncoming(const uint8* buf, uint32 len);
		void HandleStatus(const uint8* buf, uint32 len);
		void HandleQueryResponse(const uint8* buf, uint32 len);
		void RequestBlock(uint8 portalIndex, uint8 block);

		// libusb wrappers (real implementations behind HAS_LIBUSB; stubs otherwise).
		bool OpenDevice();
		void CloseDevice();
		bool SendControl(const uint8* data, uint32 len);
		int ReadInterrupt(uint8* buf, uint32 len, int timeoutMs); // >0 bytes, 0 timeout, <0 error

		AddCallback m_onAdd;
		RemoveCallback m_onRemove;

		std::thread m_thread;
		std::atomic<bool> m_running{false};
		std::atomic<bool> m_connected{false};

		// Opaque libusb handles (cast to the real types only inside the .cpp).
		void* m_ctx = nullptr;
		void* m_handle = nullptr;

		std::mutex m_outboxMutex;
		std::queue<OutCommand> m_outbox;

		// Presence + per-slot block caching. Touched only by the worker thread.
		std::array<bool, 16> m_present{};
		std::array<bool, 16> m_caching{};
		std::array<std::array<uint8, FIGURE_SIZE>, 16> m_cacheData{};
		std::array<std::array<bool, BLOCK_COUNT>, 16> m_cacheGot{};
		// When caching of a slot began (ms). Bounds the whole-figure read so a block that the
		// portal keeps reporting "present but not readable" can never spin forever.
		std::array<uint64, 16> m_cacheStartMs{};

		// The real portal drops commands sent back-to-back, so block reads are paced: at most
		// ONE outstanding 'Q' query at a time, only advancing once its reply arrives (or it
		// times out and is retried). Worker-thread only.
		bool m_queryActive = false;
		uint8 m_querySlot = 0;
		uint8 m_queryBlock = 0;
		uint64 m_queryDeadlineMs = 0;
		uint8 m_queryRetries = 0;
	};
} // namespace nsyshid
