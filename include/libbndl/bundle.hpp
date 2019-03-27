#pragma once
#include "libbndl_export.h"
#include <string>
#include <map>
#include <vector>
#include <mutex>
#include <memory>

namespace binaryio
{
	class BinaryReader;
	class BinaryWriter;
}

namespace libbndl
{
	class Bundle
	{
	public:
		enum Version
		{
			BNDL	= 1,
			BND2	= 2
		};

		enum Platform: uint32_t
		{
			PC = 1, // (or PS4/XB1)
			Xbox360 = 2 << 24, // Big endian
			PS3 = 3 << 24, // Big endian
		};

		enum Flags: uint32_t
		{
			Compressed = 1,
			UnusedFlag1 = 2, // Always set.
			UnusedFlag2 = 4, // Always set.
			HasResourceStringTable = 8
		};

		enum FileType: uint32_t
		{
			Raster = 0x00,
			Material = 0x01,
			TextFile = 0x03,
			VertexDesc = 0x0A,
			MaterialCRC32 = 0x0B, // 2006
			Renderable = 0x0C,
			MaterialTechnique = 0x0D, // last-gen console
			TextureState = 0x0E,
			MaterialState = 0x0F,
			ShaderProgramBuffer = 0x12,
			ShaderParameter = 0x14,
			Debug = 0x16,
			KdTree = 0x17,
			VoiceHierarchy = 0x18, // removed
			Snr = 0x19,
			InterpreterData = 0x1A, // unregistered
			AttribSysSchema = 0x1B,
			AttribSysVault = 0x1C,
			EntryList = 0x1D, // unregistered
			AptDataHeaderType = 0x1E,
			GuiPopup = 0x1F,
			Font = 0x21,
			LuaCode = 0x22,
			InstanceList = 0x23,
			CollisionMeshData = 0x24, // formerly ClusteredMesh
			IDList = 0x25,
			InstanceCollisionList = 0x26, // removed
			Language = 0x27,
			SatNavTile = 0x28,
			SatNavTileDirectory = 0x29,
			Model = 0x2A,
			RwColourCube = 0x2B,
			HudMessage = 0x2C,
			HudMessageList = 0x2D,
			HudMessageSequence = 0x2E,
			HudMessageSequenceDictionary = 0x2F,
			WorldPainter2D = 0x30,
			PFXHookBundle = 0x31,
			Shader = 0x32, // ShaderTechnique on console
			ICETakeDictionary = 0x41,
			VideoData = 0x42,
			PolygonSoupList = 0x43,
			CommsToolListDefinition = 0x45,
			CommsToolList = 0x46,
			BinaryFile = 0x50, // Used as a base class for other types, but this type ID was found in one of the builds.
			AnimationCollection = 0x51,
			Registry = 0xA000,
			GenericRwacWaveContent = 0xA020,
			GinsuWaveContent = 0xA021,
			AemsBank = 0xA022,
			Csis = 0xA023,
			Nicotine = 0xA024,
			Splicer = 0xA025,
			FreqContent = 0xA026, // unregistered
			VoiceHierarchyCollection = 0xA027, // unregistered
			GenericRwacReverbIRContent = 0xA028,
			SnapshotData = 0xA029,
			ZoneList = 0xB000,
			LoopModel = 0x10000,
			AISections = 0x10001,
			TrafficData = 0x10002,
			Trigger = 0x10003,
			DeformationModel = 0x10004,
			VehicleList = 0x10005,
			GraphicsSpec = 0x10006,
			PhysicsSpec = 0x10007, // unregistered
			ParticleDescriptionCollection = 0x10008,
			WheelList = 0x10009,
			WheelGraphicsSpec = 0x1000A,
			TextureNameMap = 0x1000B,
			ICEList = 0x1000C,
			ICEData = 0x1000D, // ICE
			Progression = 0x1000E,
			PropPhysics = 0x1000F,
			PropGraphicsList = 0x10010,
			PropInstanceData = 0x10011,
			BrnEnvironmentKeyframe = 0x10012,
			BrnEnvironmentTimeLine = 0x10013,
			BrnEnvironmentDictionary = 0x10014,
			GraphicsStub = 0x10015,
			StaticSoundMap = 0x10016,
			StreetData = 0x10018,
			BrnVFXMeshCollection = 0x10019,
			MassiveLookupTable = 0x1001A,
			VFXPropCollection = 0x1001B,
			StreamedDeformationSpec = 0x1001C,
			ParticleDescription = 0x1001D,
			PlayerCarColours = 0x1001E,
			ChallengeList = 0x1001F,
			FlaptFile = 0x10020,
			ProfileUpgrade = 0x10021,
			VehicleAnimation = 0x10023,
			BodypartRemapping = 0x10024,
			LUAList = 0x10025,
			LUAScript = 0x10026
		};


		struct EntryFileBlockData
		{
			uint32_t uncompressedSize;
			uint32_t uncompressedAlignment; // default depending on file type
			uint32_t compressedSize;
			std::unique_ptr<std::vector<uint8_t>> data;
		};

		struct EntryInfo
		{
			// In ResourceStringTable
			std::string name;
			std::string typeName;

			uint32_t checksum; // Stored in bundle as 64-bit (8-byte)

			uint32_t dependenciesOffset;
			FileType fileType;
			uint16_t numberOfDependencies;
		};

		struct Dependency
		{
			uint32_t fileID;
			uint32_t internalOffset;
		};

		struct Entry
		{
			EntryInfo info;
			EntryFileBlockData fileBlockData[3];
			std::vector<Dependency> dependencies;
		};


		struct EntryData
		{
			std::unique_ptr<std::vector<uint8_t>> fileBlockData[3];
			std::vector<Dependency> dependencies;
		};


		LIBBNDL_EXPORT bool Load(const std::string &name);
		LIBBNDL_EXPORT void Save(const std::string &name);

		LIBBNDL_EXPORT Version GetVersion() const
		{
			return m_version;
		}

		LIBBNDL_EXPORT Platform GetPlatform() const
		{
			return m_platform;
		}

		LIBBNDL_EXPORT EntryInfo GetInfo(const std::string &fileName) const;
		LIBBNDL_EXPORT EntryInfo GetInfo(uint32_t fileID) const;
		LIBBNDL_EXPORT EntryData GetData(const std::string &fileName) const;
		LIBBNDL_EXPORT EntryData GetData(uint32_t fileID) const;
		LIBBNDL_EXPORT std::unique_ptr<std::vector<uint8_t>> GetBinary(const std::string &fileName, uint32_t fileBlock) const;
		LIBBNDL_EXPORT std::unique_ptr<std::vector<uint8_t>> GetBinary(uint32_t fileID, uint32_t fileBlock) const;

		// Add Entry coming soon

		LIBBNDL_EXPORT bool ReplaceEntry(const std::string &fileName, const EntryData &data);
		LIBBNDL_EXPORT bool ReplaceEntry(uint32_t fileID, const EntryData &data);

		LIBBNDL_EXPORT std::vector<uint32_t> ListFileIDs() const;
		LIBBNDL_EXPORT std::map<FileType, std::vector<uint32_t>> ListFileIDsByFileType() const;

	private:
		std::mutex					m_mutex;
		std::map<uint32_t, Entry>	m_entries;
		std::map<uint32_t, std::vector<Dependency>> m_dependencies; // not used in bnd2 due to lazy reading.

		Version						m_version;
		Platform					m_platform;
		uint32_t					m_numEntries;
		uint32_t					m_fileBlockOffsets[3];
		Flags						m_flags;

		bool LoadBND2(binaryio::BinaryReader &reader);
		bool LoadBNDL(binaryio::BinaryReader &reader);
		uint32_t HashFileName(std::string fileName) const;

		static Dependency ReadDependency(binaryio::BinaryReader &reader);
		static void WriteDependency(binaryio::BinaryWriter &writer, const Dependency &dependency);
	};
}
