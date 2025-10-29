#pragma once
#include "libbndl_export.h"
#include <array>
#include <iterator>
#include <map>
#include <memory>
#include <optional>
#include <stdint.h>
#include <string>
#include <vector>
#include <version>

#if __cpp_lib_constexpr_memory > 202202L
#define LIBBNDL_BUFFER_CONSTEXPR constexpr
#else
#define LIBBNDL_BUFFER_CONSTEXPR
#endif

#ifdef __cpp_lib_to_underlying
#define LIBBNDL_TO_UNDERLYING(x) std::to_underlying(x)
#else
#define LIBBNDL_TO_UNDERLYING(x) static_cast<std::underlying_type_t<std::remove_reference_t<decltype(x)>>>(x)
#endif

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
			RenderableMesh = 0x02, // found in Black
			TextFile = 0x03,
			DrawIndexParams = 0x04, // found in Black
			IndexBuffer = 0x05, // found in Black
			MeshState = 0x06, // found in Black
			TextureAuxInfo = 0x07, // no known builds
			VertexBufferItem = 0x08, // no known builds
			VertexBuffer = 0x09, // found in Black
			VertexDescriptor = 0x0A,
			MaterialCRC32 = 0x0B, // 2006
			Renderable = 0x0C,
			MaterialTechnique = 0x0D, // last-gen console
			TextureState = 0x0E,
			MaterialState = 0x0F,
			DepthStencilState = 0x10, // found in Black
			RasterizerState = 0x11, // found in Black
			ShaderProgramBuffer = 0x12,
			RenderTargetState = 0x13, // no known builds
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
			ColourCube = 0x2B,
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

			// These have unusual categorisation, almost as if the 0x was omitted and these should be in the game-specific section.
			// All are from Black.
			CharAnimBankFile = 0x2710, // 10000
			WeaponFile = 0x2711, // 10001
			VFXFile = 0x343E, // 13374? - registered as "FileResourceType"
			BearFile = 0x343F, // 13375? - also registered as "FileResourceType"
			BkPropInstanceList = 0x3A98, // 15000
			
			Registry = 0xA000,
			GenericRwacFactoryConfiguration = 0xA010, // no known builds
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

			VFX = 0xC001, // no known builds

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


		enum class MemoryType : uint8_t
		{
			MainMemory = 0,

			GraphicsSystem = 1, // PS3
			Physical = 1, // X360
			Disposable = 1, // PC

			GraphicsLocal = 2, // PS3
		};


		struct EntryFileBlockData
		{
			uint32_t uncompressedSize;
			uint32_t uncompressedAlignment; // default depending on file type
			uint32_t compressedSize;
			std::unique_ptr<uint8_t[]> data;
		};

		struct ResourceDebugInfo
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


		class Buffer
		{
		public:
			using value_type = uint8_t;
			using size_type = size_t;
			using pointer = value_type *;
			using reference = value_type &;
			using iterator = value_type *;
			using const_iterator = const value_type *;

			constexpr Buffer() : m_ptr({}), m_size(0), m_alignment(0) {}
			Buffer(std::unique_ptr<value_type[]> ptr, size_type size, uint32_t alignment) : m_ptr(std::move(ptr)), m_size(size), m_alignment(alignment) {}
			Buffer(Buffer &&other) noexcept : m_ptr(std::move(other.m_ptr)), m_size(other.m_size), m_alignment(other.m_alignment) {}

			[[nodiscard]] constexpr size_type GetSize() const noexcept { return m_size; }
			[[nodiscard]] constexpr uint32_t GetAlignment() const noexcept { return m_alignment; }
			[[nodiscard]] LIBBNDL_BUFFER_CONSTEXPR pointer GetData() const noexcept { return m_ptr.get(); }

			[[nodiscard]] LIBBNDL_BUFFER_CONSTEXPR iterator begin() const noexcept { return m_ptr.get(); }
			[[nodiscard]] LIBBNDL_BUFFER_CONSTEXPR iterator end() const noexcept { return m_ptr.get() + m_size; }
			[[nodiscard]] LIBBNDL_BUFFER_CONSTEXPR const_iterator cbegin() const noexcept { return begin(); }
			[[nodiscard]] LIBBNDL_BUFFER_CONSTEXPR const_iterator cend() const noexcept { return end(); }

			[[nodiscard]] LIBBNDL_BUFFER_CONSTEXPR bool operator==(nullptr_t) const noexcept { return m_ptr.get() == nullptr; }
			[[nodiscard]] LIBBNDL_BUFFER_CONSTEXPR reference operator[](size_type idx) const { return m_ptr[idx]; }

			void operator=(Buffer &&buffer) noexcept
			{
				m_ptr = std::move(buffer.m_ptr);
				m_size = buffer.m_size;
				m_alignment = buffer.m_alignment;
			}

		private:
			std::unique_ptr<value_type[]> m_ptr;
			size_type m_size;
			uint32_t m_alignment;
		};


		class Resource
		{
		public:
			Resource(std::array<Buffer, 3> buffers, std::vector<Dependency> dependenices) : m_buffers(std::move(buffers)), m_dependencies(std::move(dependenices)) {}

			[[nodiscard]] constexpr const Buffer &GetBinary(MemoryType block) const { return m_buffers[LIBBNDL_TO_UNDERLYING(block)]; }
			[[nodiscard]] constexpr const std::vector<Dependency> &GetDependencies() const { return m_dependencies; }

			void ReplaceBinary(MemoryType block, Buffer &&buffer) { m_buffers[LIBBNDL_TO_UNDERLYING(block)] = std::move(buffer); }

		private:
			std::array<Buffer, 3> m_buffers;
			std::vector<Dependency> m_dependencies;
		};


		LIBBNDL_EXPORT Bundle() = default;
		LIBBNDL_EXPORT Bundle(MagicVersion magicVersion, uint32_t revisionNumber, Platform platform, Flags flags); // For creating new bundles

		LIBBNDL_EXPORT bool Load(const std::string &name);
		LIBBNDL_EXPORT bool Save(const std::string &name);

		LIBBNDL_EXPORT [[nodiscard]] constexpr MagicVersion GetMagicVersion() const { return m_magicVersion; }
		LIBBNDL_EXPORT [[nodiscard]] constexpr uint32_t GetRevisionNumber() const { return m_revisionNumber; }
		LIBBNDL_EXPORT [[nodiscard]] constexpr Platform GetPlatform() const { return m_platform; }
		LIBBNDL_EXPORT [[nodiscard]] constexpr Flags GetFlags() const { return m_flags; }

		LIBBNDL_EXPORT [[nodiscard]] std::optional<ResourceDebugInfo> GetResourceDebugInfo(const std::string &resourceName) const;
		LIBBNDL_EXPORT [[nodiscard]] std::optional<ResourceDebugInfo> GetResourceDebugInfo(uint32_t resourceID) const;
		LIBBNDL_EXPORT [[nodiscard]] std::optional<ResourceType> GetResourceType(const std::string &resourceName) const;
		LIBBNDL_EXPORT [[nodiscard]] std::optional<ResourceType> GetResourceType(uint32_t resourceID) const;
		LIBBNDL_EXPORT [[nodiscard]] std::optional<Resource> GetResource(const std::string &resourceName) const;
		LIBBNDL_EXPORT [[nodiscard]] std::optional<Resource> GetResource(uint32_t resourceID) const;
		LIBBNDL_EXPORT [[nodiscard]] Buffer GetBinary(const std::string &resourceName, MemoryType fileBlock) const;
		LIBBNDL_EXPORT [[nodiscard]] Buffer GetBinary(uint32_t resourceID, MemoryType fileBlock) const;

		LIBBNDL_EXPORT bool AddResource(const std::string &resourceName, const Resource &data, ResourceType resourceType);
		LIBBNDL_EXPORT bool AddResource(uint32_t resourceID, const Resource &data, ResourceType resourceType);
		LIBBNDL_EXPORT bool AddResourceDebugInfo(const std::string &resourceName, const std::string &name, const std::string &type);
		LIBBNDL_EXPORT bool AddResourceDebugInfo(uint32_t resourceID, const std::string &name, const std::string &type);

		LIBBNDL_EXPORT bool ReplaceResource(const std::string &resourceName, const Resource &data);
		LIBBNDL_EXPORT bool ReplaceResource(uint32_t resourceID, const Resource &data);

		LIBBNDL_EXPORT [[nodiscard]] std::vector<uint32_t> GetResourceIDs() const;
		LIBBNDL_EXPORT [[nodiscard]] std::map<ResourceType, std::vector<uint32_t>> GetResourceIDsByType() const;

		LIBBNDL_EXPORT [[nodiscard]] std::vector<MemoryType> GetMemoryTypes() const;

	private:
		std::map<uint32_t, Entry>	m_entries;
		std::map<uint32_t, ResourceDebugInfo> m_debugInfoEntries;
		std::map<uint32_t, std::vector<Dependency>> m_dependencies; // not used in bnd2 due to lazy reading.

		MagicVersion				m_magicVersion;
		uint32_t					m_revisionNumber;
		Platform					m_platform;
		Flags						m_flags;

		bool LoadBND2(binaryio::BinaryReader &reader);
		bool LoadBNDL(binaryio::BinaryReader &reader);
		bool SaveBND2(binaryio::BinaryWriter &writer);
		bool SaveBNDL(binaryio::BinaryWriter &writer);
		[[nodiscard]] int8_t MapBNDLBlockToBND2(uint8_t block) const;
		[[nodiscard]] uint32_t HashResourceName(std::string resourceName) const;

		static [[nodiscard]] Dependency ReadDependency(binaryio::BinaryReader &reader);
		static void WriteDependency(binaryio::BinaryWriter &writer, const Dependency &dependency);
	};
}
