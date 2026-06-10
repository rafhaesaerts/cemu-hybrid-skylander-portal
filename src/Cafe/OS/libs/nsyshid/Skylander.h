#pragma once

#include <memory>
#include <mutex>

#include "nsyshid.h"
#include "Backend.h"
#include "PhysicalPortalBridge.h"

#include "Common/FileStream.h"

namespace nsyshid
{
	class SkylanderPortalDevice final : public Device {
	  public:
		SkylanderPortalDevice();
		~SkylanderPortalDevice() = default;

		bool Open() override;

		void Close() override;

		bool IsOpened() override;

		ReadResult Read(ReadMessage* message) override;

		WriteResult Write(WriteMessage* message) override;

		bool GetDescriptor(uint8 descType,
						   uint8 descIndex,
						   uint16 lang,
						   uint8* output,
						   uint32 outputMaxLength) override;

		bool SetIdle(uint8 ifIndex,
					 uint8 reportId,
					 uint8 duration) override;

		bool SetProtocol(uint8 ifIndex, uint8 protocol) override;

		bool SetReport(ReportMessage* message) override;

	  private:
		bool m_IsOpened;
	};

	constexpr uint16 SKY_BLOCK_COUNT = 0x40;
	constexpr uint16 SKY_BLOCK_SIZE = 0x10;
	constexpr uint16 SKY_FIGURE_SIZE = SKY_BLOCK_COUNT * SKY_BLOCK_SIZE;
	constexpr uint8 MAX_SKYLANDERS = 16;

	class SkylanderUSB {
	  public:
		struct Skylander final
		{
			std::unique_ptr<FileStream> skyFile;
			uint8 status = 0;
			std::queue<uint8> queuedStatus;
			std::array<uint8, SKY_FIGURE_SIZE> data{};
			uint32 lastId = 0;
			// Hybrid mode: a slot backed by a figure on the REAL portal rather than a dump
			// file. portalIndex is the real portal's slot index for forwarding writes.
			bool physical = false;
			uint8 portalIndex = 0;
			void Save();

			enum : uint8
			{
				REMOVED = 0,
				READY = 1,
				REMOVING = 2,
				ADDED = 3
			};
		};

		struct SkylanderLEDColor final
		{
			uint8 red = 0;
			uint8 green = 0;
			uint8 blue = 0;
		};

		void ControlTransfer(uint8* buf, uint32 length);

		void Activate();
		void Deactivate();
		void SetLeds(uint8 side, uint8 r, uint8 g, uint8 b);

		std::array<uint8, 64> GetStatus();
		void QueryBlock(uint8 skyNum, uint8 block, uint8* replyBuf);
		void WriteBlock(uint8 skyNum, uint8 block, const uint8* toWriteBuf,
						uint8* replyBuf);

		uint8 LoadSkylander(uint8* buf, std::unique_ptr<FileStream> file);
		bool RemoveSkylander(uint8 skyNum);
		bool CreateSkylander(fs::path pathName, uint16 skyId, uint16 skyVar);
		uint16 SkylanderCRC16(uint16 initValue, const uint8* buffer, uint32 size);
		static std::map<const std::pair<const uint16, const uint16>, const char*> GetListSkylanders();
		std::string FindSkylander(uint16 skyId, uint16 skyVar);

		// Hybrid mode: merge a real Portal of Power's figures into these emulated slots.
		// StartHybrid opens the bridge; if no real portal / no libusb it is a no-op and the
		// portal stays purely emulated. OnPhysicalAdd/Remove are invoked by the bridge's
		// worker thread. Start/StopHybrid may be called from any thread (GUI checkbox,
		// backend attach): they serialize on m_hybridMutex, and m_bridge is published /
		// retired / read only under m_skyMutex.
		void StartHybrid();
		void StopHybrid();
		bool IsHybridActive()
		{
			std::lock_guard lock(m_skyMutex);
			return m_bridge != nullptr;
		}
		void OnPhysicalAdd(uint8 portalIndex, const std::array<uint8, SKY_FIGURE_SIZE>& data);
		void OnPhysicalRemove(uint8 portalIndex);

	  protected:
		std::mutex m_skyMutex;
		std::mutex m_queryMutex;
		std::array<Skylander, MAX_SKYLANDERS> m_skylanders;

	  private:
		// Force a full portal re-scan by briefly pulsing present figures off-and-on, so a game
		// that defers mid-session arrivals re-reads the whole portal. Caller holds m_skyMutex.
		// `physicalOnly` limits the pulse to physical siblings; otherwise virtual siblings pulse
		// too. Pass an out-of-range exceptSlot (0xFF) to pulse every present figure, including a
		// just-added one (used to mimic lift-and-replace of a lone new arrival).
		void PulseRescan(uint8 exceptSlot, bool physicalOnly);
		// True when any figure other than `exceptSlot` is currently present. Caller holds m_skyMutex.
		bool OtherPresent(uint8 exceptSlot) const;
		// True when the running title is Skylanders Swap Force (any region).
		bool IsSwapForce() const;

		std::queue<std::array<uint8, 64>> m_queries;
		bool m_activated = true;
		uint8 m_interruptCounter = 0;
		// Serializes StartHybrid/StopHybrid against each other (GUI checkbox + backend attach).
		std::mutex m_hybridMutex;
		// Diagnostics: last status word logged by GetStatus (sentinel so the first poll logs).
		uint32 m_lastStatusWordLogged = 0xFFFFFFFFu;
		SkylanderLEDColor m_colorRight = {};
		SkylanderLEDColor m_colorLeft = {};
		SkylanderLEDColor m_colorTrap = {};
		std::unique_ptr<PhysicalPortalBridge> m_bridge;
	};
	extern SkylanderUSB g_skyportal;
} // namespace nsyshid