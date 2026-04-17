#pragma once
#include "libbndl_export.h"
#include <string>
#include <map>
#include <vector>
#include <mutex>
#include <memory>
#include <optional>

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
		enum MagicVersion
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

		enum ResourceType: uint32_t
		{
			Raster = 0x00,
			Material = 0x01,
			ResourceMesh = 0x02, // found in Black
			TextFile = 0x03,
			DrawIndexParams = 0x04, // found in Black
			IndexBuffer = 0x05, // found in Black
			MeshState = 0x06, // found in Black
			VertexBuffer = 0x09, // found in Black
			VertexDesc = 0x0A,
			MaterialCRC32 = 0x0B, // 2006
			Renderable = 0x0C,
			MaterialTechnique = 0x0D, // last-gen console
			TextureState = 0x0E,
			MaterialState = 0x0F,
			DepthStencilState = 0x10, // found in Black
			RasterizerState = 0x11, // found in Black
			ShaderProgramBuffer = 0x12,
			ShaderParameter = 0x14,
			RenderableAssembly = 0x15, // found in Black
			Debug = 0x16,
			KdTree = 0x17,
			VoiceHierarchy = 0x18, // removed
			Snr = 0x19,
			InterpreterData = 0x1A, // unregistered
			AttribSysSchema = 0x1B,
			AttribSysVault = 0x1C,
			EntryList = 0x1D, // unregistered
			AptDataHeader = 0x1E,
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
			RawFile = 0x40, // found in Black
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

			// Burnout Paradise
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
			LUAScript = 0x10026,

			// Black
			BkSoundWeapon = 0x11000,
			BkSoundGunsu = 0x11001,
			BkSoundBulletImpact = 0x11002,
			BkSoundBulletImpactList = 0x11003,
			BkSoundBulletImpactStream = 0x11004
		};


		struct EntryFileBlockData
		{
			uint32_t uncompressedSize;
			uint32_t uncompressedAlignment; // default depending on file type
			uint32_t compressedSize;
			std::unique_ptr<std::vector<uint8_t>> data;
		};

		struct EntryDebugInfo
		{
			std::string name;
			std::string typeName;
		};

		struct EntryInfo
		{
			uint32_t checksum; // Stored in bundle as 64-bit (8-byte)

			uint32_t dependenciesOffset;
			ResourceType resourceType;
			uint16_t numberOfDependencies;
		};

		struct Dependency
		{
			uint32_t resourceID;
			uint32_t internalOffset;
		};

		struct Entry
		{
			EntryInfo info;
			EntryFileBlockData fileBlockData[3];
		};


		struct EntryData
		{
			std::unique_ptr<std::vector<uint8_t>> fileBlockData[3];
			uint32_t alignments[3];
			std::vector<Dependency> dependencies;
		};


		LIBBNDL_EXPORT Bundle() = default;
		LIBBNDL_EXPORT Bundle(MagicVersion magicVersion, uint32_t revisionNumber, Platform platform, Flags flags); // For creating new bundles

		LIBBNDL_EXPORT bool Load(const std::string &name);
		LIBBNDL_EXPORT bool Save(const std::string &name);

		LIBBNDL_EXPORT MagicVersion GetMagicVersion() const
		{
			return m_magicVersion;
		}

		LIBBNDL_EXPORT uint32_t GetRevisionNumber() const
		{
			return m_revisionNumber;
		}

		LIBBNDL_EXPORT Platform GetPlatform() const
		{
			return m_platform;
		}

		LIBBNDL_EXPORT Flags GetFlags() const
		{
			return m_flags;
		}

		LIBBNDL_EXPORT std::optional<EntryDebugInfo> GetDebugInfo(const std::string &resourceName) const;
		LIBBNDL_EXPORT std::optional<EntryDebugInfo> GetDebugInfo(uint32_t resourceID) const;
		LIBBNDL_EXPORT std::optional<ResourceType> GetResourceType(const std::string &resourceName) const;
		LIBBNDL_EXPORT std::optional<ResourceType> GetResourceType(uint32_t resourceID) const;
		LIBBNDL_EXPORT std::optional<EntryData> GetData(const std::string &resourceName) const;
		LIBBNDL_EXPORT std::optional<EntryData> GetData(uint32_t resourceID) const;
		LIBBNDL_EXPORT std::unique_ptr<std::vector<uint8_t>> GetBinary(const std::string &resourceName, uint32_t fileBlock) const;
		LIBBNDL_EXPORT std::unique_ptr<std::vector<uint8_t>> GetBinary(uint32_t resourceID, uint32_t fileBlock) const;

		LIBBNDL_EXPORT bool AddResource(const std::string &resourceName, const EntryData &data, ResourceType resourceType);
		LIBBNDL_EXPORT bool AddResource(uint32_t resourceID, const EntryData &data, ResourceType resourceType);
		LIBBNDL_EXPORT bool AddDebugInfo(const std::string &resourceName, const std::string &name, const std::string &type);
		LIBBNDL_EXPORT bool AddDebugInfo(uint32_t resourceID, const std::string &name, const std::string &type);

		LIBBNDL_EXPORT bool ReplaceResource(const std::string &resourceName, const EntryData &data);
		LIBBNDL_EXPORT bool ReplaceResource(uint32_t resourceID, const EntryData &data);

		LIBBNDL_EXPORT std::vector<uint32_t> ListResourceIDs() const;
		LIBBNDL_EXPORT std::map<ResourceType, std::vector<uint32_t>> ListResourceIDsByType() const;

	private:
		std::map<uint32_t, Entry>	m_entries;
		std::map<uint32_t, EntryDebugInfo> m_debugInfoEntries;
		std::map<uint32_t, std::vector<Dependency>> m_dependencies; // not used in bnd2 due to lazy reading.

		MagicVersion				m_magicVersion;
		uint32_t					m_revisionNumber;
		Platform					m_platform;
		Flags						m_flags;

		bool LoadBND2(binaryio::BinaryReader &reader);
		bool LoadBNDL(binaryio::BinaryReader &reader);
		bool SaveBND2(binaryio::BinaryWriter &writer);
		bool SaveBNDL(binaryio::BinaryWriter &writer);
		int8_t MapBNDLBlockToBND2(uint8_t block) const;
		uint32_t HashResourceName(std::string resourceName) const;

		static Dependency ReadDependency(binaryio::BinaryReader &reader);
		static void WriteDependency(binaryio::BinaryWriter &writer, const Dependency &dependency);
	};
}
