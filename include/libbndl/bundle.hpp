#pragma once
#include "libbndl_export.h"
#include <array>
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

namespace libbndl
{
	namespace Formats { class Base; }

	enum class MagicNumber : uint8_t
	{
		BNDL = 1,
		BND2 = 2
	};

	enum class Platform : uint16_t
	{
		PC = 1, // (or PS4/XB1)
		Xbox360 = 2,
		PS3 = 3,
		PSVita = 4,
		WiiU = 5
	};

	namespace ResourceType
	{
		namespace Burnout
		{
			enum : uint32_t
			{
				Texture = 0x00,
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
				IdList = 0x25,
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
				Shader = 0x32, // PC
				ShaderTechnique = 0x32, // Console

				RawFile = 0x40, // found in Black
				ICETakeDictionary = 0x41,
				VideoData = 0x42,
				PolygonSoupList = 0x43,
				DeveloperList = 0x44,
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
		}

		namespace NeedForSpeed
		{
			enum : uint32_t
			{
				Texture = 0x01,
				Material = 0x02,
				VertexDescriptor = 0x03,
				VertexProgramState = 0x04,
				Renderable = 0x05,
				MaterialState = 0x06,
				SamplerState = 0x07,
				ShaderProgramBuffer = 0x08,

				AttribSysSchema = 0x10,
				AttribSysVault = 0x11,
				GeneSysDefinition = 0x12,
				GeneSysInstance = 0x13,
				GeneSysType = 0x14,
				GeneSysObject = 0x15,
				BinaryFile = 0x16,
				MiiData = 0x17,

				EntryList = 0x20,
				BundleIndex = 0x21,

				Font = 0x30,

				LuaCode = 0x40,

				InstanceList = 0x50,
				Model = 0x51,
				ColourCube = 0x52,
				Shader = 0x53,

				PolygonSoupList = 0x60,
				PolygonSoupTree = 0x61,
				IdList = 0x62,

				NavigationMesh = 0x68,

				TextFile = 0x70,
				TextFileList = 0x71,
				ResourceHandleList = 0x72,

				LuaData = 0x74,

				AllocatorInPool = 0x78,

				Ginsu = 0x80,
				Wave = 0x81,
				WaveContainerTable = 0x82,
				GameplayLinkData = 0x83,
				WaveDictionary = 0x84,
				MicroMonoStream = 0x85,
				Reverb = 0x86,

				ZoneList = 0x90,
				WorldPaintMap = 0x91,

				IceAnimDictionary = 0xA0,

				AnimationList = 0xB0,
				PathAnimation = 0xB1,
				AnimSkel = 0xB2,
				Animation = 0xB3,

				CgsVertexProgramState = 0xC0,
				CgsProgramBuffer = 0xC1,

				DeltaDeleted = 0xDE,

				VehicleList = 0x105,
				VehicleGraphicsSpec = 0x106,
				VehiclePhysicsSpec = 0x107,
				WheelList = 0x109,
				WheelGraphicsSpec = 0x10A,
				EnvironmentKeyframe = 0x112,
				EnvironmentTimeLine = 0x113,
				EnvironmentDictionary = 0x114,
				GraphicsStub = 0x115,
				FlaptFile = 0x116,

				AIData = 0x200,
				Language = 0x201,
				TriggerData = 0x202,
				RoadData = 0x203,
				DynamicInstanceList = 0x204,
				WorldObject = 0x205,
				ZoneHeader = 0x206,
				VehicleSound = 0x207,
				RoadMapData = 0x208,
				CharacterSpec = 0x209,
				CharacterList = 0x20A,
				SurfaceSounds = 0x20B,
				ReverbRoadData = 0x20C,
				CameraTake = 0x20D,
				CameraTakeList = 0x20E,
				GroundcoverCollection = 0x20F,
				ControlMesh = 0x210,
				CutsceneData = 0x211,
				CutsceneList = 0x212,
				LightInstanceList = 0x213,
				GroundcoverInstances = 0x214,
				CompoundObject = 0x215,
				CompoundInstanceList = 0x216,
				PropObject = 0x217,
				PropInstanceList = 0x218,
				ZoneAmbienceList = 0x219,

				BearEffect = 0x301,
				BearGlobalParameters = 0x302,
				ConvexHull = 0x303,

				HSMData = 0x501,

				TrafficGraphicsStub = 0x700,
				TrafficLaneData = 0x701
			};
		}
	}

	enum class MemoryType : uint8_t
	{
		MainMemory = 0,

		GraphicsSystem = 1, // PS3
		Physical = 1, // Xbox 360
		Mem1 = 1, // Wii U

		GraphicsLocal = 2, // PS3
		GraphicsMem2 = 2, // Wii U

		Disposable = 3 // PC in Burnout, all in NFS
	};

	class Flags
	{
	private:
		using UnderlyingType = uint32_t;
		enum class Values : UnderlyingType
		{
			Compressed = 0x1,
			MainMemOptimised = 0x2,
			GraphicsMemOptimised = 0x4,
			HasDebugData = 0x8,
			NonAsynchFixupRequired = 0x10,
			MultistreamBundle = 0x20,
			DeltaBundle = 0x40,
			ContainsDefaultResource = 0x80
		};

	public:
		using enum Values;

		constexpr Flags() noexcept : m_value(0) {}
		constexpr Flags(Values flag) noexcept : m_value(LIBBNDL_TO_UNDERLYING(flag)) {}
		constexpr Flags(const Flags &flags) noexcept = default;
		constexpr explicit Flags(UnderlyingType flags) noexcept : m_value(flags) {}

		[[nodiscard]] constexpr bool operator==(const Flags &flags) const noexcept = default;
		[[nodiscard]] constexpr bool operator==(UnderlyingType flags) const noexcept { return m_value == flags; };

		[[nodiscard]] constexpr Flags operator&(const Flags &rhs) const noexcept { return Flags(m_value & rhs.m_value); }
		[[nodiscard]] friend constexpr Flags operator&(const Values &lhs, const Flags &rhs) noexcept { return rhs & lhs; }
		[[nodiscard]] friend constexpr Flags operator&(Values lhs, Values rhs) noexcept { return Flags(lhs) & rhs; }
		[[nodiscard]] constexpr Flags operator|(const Flags &rhs) const noexcept { return Flags(m_value | rhs.m_value); }
		[[nodiscard]] friend constexpr Flags operator|(const Values &lhs, const Flags &rhs) noexcept { return rhs | lhs; }
		[[nodiscard]] friend constexpr Flags operator|(Values lhs, Values rhs) noexcept { return Flags(lhs) | rhs; }
		[[nodiscard]] constexpr Flags operator^(const Flags &rhs) const noexcept { return Flags(m_value ^ rhs.m_value); }
		[[nodiscard]] friend constexpr Flags operator^(const Values &lhs, const Flags &rhs) noexcept { return rhs ^ lhs; }
		[[nodiscard]] friend constexpr Flags operator^(Values lhs, Values rhs) noexcept { return Flags(lhs) ^ rhs; }
		[[nodiscard]] constexpr Flags operator~() const noexcept { return Flags(~m_value); }
		[[nodiscard]] friend constexpr Flags operator~(Values value) noexcept { return ~Flags(value); }

		constexpr Flags &operator|=(const Flags &rhs) noexcept
		{
			m_value |= rhs.m_value;
			return *this;
		}

		constexpr Flags &operator&=(const Flags &rhs) noexcept
		{
			m_value &= rhs.m_value;
			return *this;
		}

		constexpr Flags &operator^=(const Flags &rhs) noexcept
		{
			m_value ^= rhs.m_value;
			return *this;
		}

		[[nodiscard]] constexpr explicit operator bool() const noexcept { return !!m_value; }
		[[nodiscard]] constexpr explicit operator UnderlyingType() const noexcept { return m_value; }

	private:
		UnderlyingType m_value;
	};

	class ResourceID
	{
	private:
		using UnderlyingType = uint64_t;

	public:
		enum class EIDType : uint8_t
		{
			Normal = 0x0,
			GameChanger = 0x1,
			ResourceList = 0x80,
			DeltaBundle = 0xC0
		};

		constexpr ResourceID() noexcept : m_id(0) {}
		constexpr ResourceID(uint32_t id, uint16_t type, uint8_t index, EIDType idType) noexcept
			: m_id(id | (static_cast<uint64_t>(type) << 32) | (static_cast<uint64_t>(index) << 48) || (static_cast<uint64_t>(idType) << 56)) {}
		constexpr explicit ResourceID(UnderlyingType id) noexcept : m_id(id) {}

		[[nodiscard]] constexpr bool operator==(const ResourceID &id) const noexcept = default;
		[[nodiscard]] constexpr auto operator<=>(const ResourceID &flags) const noexcept = default;
		[[nodiscard]] constexpr auto operator==(UnderlyingType id) const noexcept { return m_id == id; };

		[[nodiscard]] constexpr uint32_t GetGameChangerID() const noexcept { return m_id & 0xFFFFFFFF; }
		[[nodiscard]] constexpr uint32_t GetID32() const noexcept { return GetGameChangerID(); }
		[[nodiscard]] constexpr uint16_t GetResourceTypeID() const noexcept { return (m_id >> 32) & 0xFFFF; }
		[[nodiscard]] constexpr uint8_t GetIndex() const noexcept { return (m_id >> 48) & 0xFF; }
		[[nodiscard]] constexpr EIDType GetIDType() const noexcept { return static_cast<EIDType>((m_id >> 56) & 0xFF); }

		[[nodiscard]] constexpr explicit operator uint32_t() const noexcept { return GetGameChangerID(); }
		[[nodiscard]] constexpr explicit operator UnderlyingType() const noexcept { return m_id; }

	private:
		UnderlyingType m_id;
	};

	class ResourceDebugInfo
	{
	public:
		constexpr ResourceDebugInfo(std::string name, std::string typeName) : m_name(std::move(name)), m_typeName(std::move(typeName)) {}

		[[nodiscard]] constexpr std::string GetName() const noexcept { return m_name; }
		[[nodiscard]] constexpr std::string GetTypeName() const noexcept { return m_typeName; }

	private:
		std::string m_name;
		std::string m_typeName;
	};

	class Import
	{
	public:
		enum class ImportType : uint8_t
		{
			Pointer = 0,
			ResourceHandle = 1
		};

		constexpr Import(ResourceID resourceID, uint32_t offset) : m_resourceID(resourceID), m_offset(offset) {}
		constexpr Import(ResourceID resourceID, uint32_t offset, ImportType type)
			: m_resourceID(resourceID), m_offset(offset | (static_cast<uint32_t>(type) << 31)) {}

		[[nodiscard]] constexpr ResourceID GetResourceID() const noexcept { return m_resourceID; }
		[[nodiscard]] constexpr uint32_t GetOffset() const noexcept { return m_offset & 0x7FFFFFFF; }
		[[nodiscard]] constexpr ImportType GetImportType() const noexcept { return static_cast<ImportType>((m_offset >> 31) & 1); }

	private:
		ResourceID m_resourceID;
		uint32_t m_offset;
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

		[[nodiscard]] LIBBNDL_BUFFER_CONSTEXPR bool operator==(std::nullptr_t) const noexcept { return m_ptr.get() == nullptr; }
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
		Resource(std::array<Buffer, 4> buffers, std::vector<Import> imports) : m_buffers(std::move(buffers)), m_imports(std::move(imports)) {}

		[[nodiscard]] constexpr const Buffer &GetBinary(MemoryType block) const { return m_buffers[LIBBNDL_TO_UNDERLYING(block)]; }
		[[nodiscard]] constexpr const std::vector<Import> GetImports() const { return m_imports; }

		void ReplaceBinary(MemoryType block, Buffer &&buffer) { m_buffers[LIBBNDL_TO_UNDERLYING(block)] = std::move(buffer); }

	private:
		std::array<Buffer, 4> m_buffers;
		std::vector<Import> m_imports;
	};

	class Bundle
	{
	public:
		LIBBNDL_EXPORT Bundle();
		LIBBNDL_EXPORT Bundle(MagicNumber magicNumber, uint16_t version, Platform platform, Flags flags); // For creating new bundles
		LIBBNDL_EXPORT ~Bundle();

		LIBBNDL_EXPORT bool Load(const std::string &name);
		LIBBNDL_EXPORT bool Save(const std::string &name);

		[[nodiscard]] LIBBNDL_EXPORT MagicNumber GetMagicNumber() const;
		[[nodiscard]] LIBBNDL_EXPORT uint16_t GetVersion() const;
		[[nodiscard]] LIBBNDL_EXPORT Platform GetPlatform() const;
		[[nodiscard]] LIBBNDL_EXPORT Flags GetFlags() const;

		[[nodiscard]] LIBBNDL_EXPORT bool IsBurnoutEra() const;
		[[nodiscard]] LIBBNDL_EXPORT bool IsNeedForSpeedEra() const;

		[[nodiscard]] LIBBNDL_EXPORT std::optional<ResourceDebugInfo> GetResourceDebugInfo(const std::string &resourceName, uint8_t streamIndex = 0) const;
		[[nodiscard]] LIBBNDL_EXPORT std::optional<ResourceDebugInfo> GetResourceDebugInfo(ResourceID resourceID, uint8_t streamIndex = 0) const;
		[[nodiscard]] LIBBNDL_EXPORT std::optional<uint32_t> GetResourceType(const std::string &resourceName, uint8_t streamIndex = 0) const;
		[[nodiscard]] LIBBNDL_EXPORT std::optional<uint32_t> GetResourceType(ResourceID resourceID, uint8_t streamIndex = 0) const;
		[[nodiscard]] LIBBNDL_EXPORT std::optional<Resource> GetResource(const std::string &resourceName, uint8_t streamIndex = 0) const;
		[[nodiscard]] LIBBNDL_EXPORT std::optional<Resource> GetResource(ResourceID resourceID, uint8_t streamIndex = 0) const;
		[[nodiscard]] LIBBNDL_EXPORT Buffer GetBinary(const std::string &resourceName, MemoryType memoryType, uint8_t streamIndex = 0) const;
		[[nodiscard]] LIBBNDL_EXPORT Buffer GetBinary(ResourceID resourceID, MemoryType memoryType, uint8_t streamIndex = 0) const;

		LIBBNDL_EXPORT bool AddResource(const std::string &resourceName, const Resource &data, uint32_t resourceType, uint8_t streamIndex = 0);
		LIBBNDL_EXPORT bool AddResource(ResourceID resourceID, const Resource &data, uint32_t resourceType, uint8_t streamIndex = 0);
		LIBBNDL_EXPORT bool AddResourceDebugInfo(const std::string &resourceName, const std::string &name, const std::string &type, uint8_t streamIndex = 0);
		LIBBNDL_EXPORT bool AddResourceDebugInfo(ResourceID resourceID, const std::string &name, const std::string &type, uint8_t streamIndex = 0);

		LIBBNDL_EXPORT bool ReplaceResource(const std::string &resourceName, const Resource &data, uint8_t streamIndex = 0);
		LIBBNDL_EXPORT bool ReplaceResource(ResourceID resourceID, const Resource &data, uint8_t streamIndex = 0);

		[[nodiscard]] LIBBNDL_EXPORT std::vector<ResourceID> GetResourceIDs() const;
		[[nodiscard]] LIBBNDL_EXPORT std::map<uint32_t, std::vector<ResourceID>> GetResourceIDsByType() const;
		[[nodiscard]] LIBBNDL_EXPORT std::vector<uint8_t> GetResourceStreamIndices(ResourceID resourceID) const;

		[[nodiscard]] LIBBNDL_EXPORT ResourceID GetDefaultResourceID() const;
		[[nodiscard]] LIBBNDL_EXPORT int32_t GetDefaultResourceStreamIndex() const;
		[[nodiscard]] LIBBNDL_EXPORT std::string GetStreamName(uint8_t index) const;

		[[nodiscard]] LIBBNDL_EXPORT std::vector<MemoryType> GetMemoryTypes() const;

	private:
		std::unique_ptr<Formats::Base> m_impl;

		[[nodiscard]] ResourceID HashResourceName(std::string resourceName) const;
	};
}
