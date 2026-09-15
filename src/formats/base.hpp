#pragma once
#include <libtub/bundle.hpp>
#include <binaryio/binaryreader.hpp>
#include <binaryio/binarywriter.hpp>
#include <array>
#include <bit>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <stdint.h>
#include <string>
#include <utility>
#include <vector>

#if defined(_MSC_VER)
#	include <intrin.h>
#endif

#ifndef __has_builtin
#	define __has_builtin(x) 0
#endif

namespace libtub::Formats
{
	// Index of the highest set bit, or 0 for 0. The result is uint32_t because unsigned long is 64-bit on Linux and
	// macOS, and OR-ing a wider result into a 32-bit BND2 field makes binaryio write 8 bytes.
	inline uint32_t BitScanReverse(uint32_t input)
	{
		if (input == 0)
			return 0;

#if defined(_MSC_VER)
		unsigned long result;
		_BitScanReverse(&result, input);
		return static_cast<uint32_t>(result);
#elif __has_builtin(__builtin_clz) || defined(__GNUC__)
		return static_cast<uint32_t>(std::numeric_limits<uint32_t>::digits - 1 - __builtin_clz(input));
#else
		return static_cast<uint32_t>(std::bit_width(input) - 1);
#endif
	}

	using ResourceKey = std::pair<ResourceID, uint8_t>;

	struct ResourceData
	{
		uint32_t uncompressedSize;
		uint32_t uncompressedAlignment; // default depending on file type
		uint32_t onDiskSize;
		uint32_t onDiskAlignment;
		std::unique_ptr<uint8_t[]> data;
	};

	struct ResourceEntry
	{
		std::array<ResourceData, 4> descriptors;

		uint64_t importHash;

		uint32_t importOffset;
		uint32_t resourceType;
		uint16_t importCount;
	};

	struct ResourceDebugDataEntry
	{
		std::string name;
		std::string typeName;
	};

	struct ImportEntry
	{
		ResourceID resourceID;
		uint32_t offset;
		Import::ImportType type;
	};

	class Base
	{
	public:
		Base() = default;
		Base(uint16_t version, Platform platform, Flags flags);
		Base(const Base &) = delete;
		Base &operator=(const Base &) = delete;
		Base(Base &&) = delete;
		Base &operator=(Base &&) = delete;
		virtual ~Base() = default;

		virtual ErrorCode Load(binaryio::BinaryReader &reader) = 0;
		virtual ErrorCode Save(binaryio::BinaryWriter &writer) = 0;

		[[nodiscard]] virtual constexpr Magic GetMagic() const = 0;
		[[nodiscard]] constexpr uint16_t GetVersion() const { return m_version; }
		[[nodiscard]] constexpr Platform GetPlatform() const { return m_platform; }
		[[nodiscard]] constexpr Flags GetFlags() const { return m_flags; }

		[[nodiscard]] std::optional<ResourceDebugDataEntry> GetResourceDebugData(ResourceKey resourceKey) const;
		[[nodiscard]] std::optional<uint32_t> GetResourceType(ResourceKey resourceKey) const;
		// Returns std::nullopt when the resource is missing or one of its populated blocks can't be decoded.
		[[nodiscard]] virtual std::optional<Resource> GetResource(ResourceKey resourceKey) const = 0;
		// Decodes one block the way GetResource returns it. Blocks without data give an empty buffer; a missing
		// resource or undecodable block gives std::nullopt.
		[[nodiscard]] virtual std::optional<Buffer> GetResourceBinary(ResourceKey resourceKey, MemoryType memoryType) const;
		[[nodiscard]] bool HasResource(ResourceKey resourceKey) const { return m_entries.contains(resourceKey); }

		bool AddResource(ResourceKey resourceKey, const Resource &data);
		bool AddResourceDebugData(ResourceKey resourceID, std::string name, std::string typeName);

		bool ReplaceResource(ResourceKey resourceKey, const Resource &data);

		[[nodiscard]] uint32_t GetResourceCount() const { return static_cast<uint32_t>(m_entries.size()); }
		[[nodiscard]] std::vector<ResourceID> GetResourceIDs() const;
		[[nodiscard]] std::vector<ResourceKey> GetResourceKeys() const;
		[[nodiscard]] std::map<uint32_t, std::vector<ResourceID>> GetResourceIDsByType() const;
		[[nodiscard]] std::vector<uint8_t> GetResourceStreamIndices(ResourceID resourceID) const;

		[[nodiscard]] virtual ResourceID GetDefaultResourceID() const;
		[[nodiscard]] virtual int32_t GetDefaultResourceStreamIndex() const;
		[[nodiscard]] virtual std::string GetStreamName(uint8_t index) const;
		virtual bool SetDefaultResource(ResourceKey resourceKey);
		virtual bool SetStreamName(uint8_t index, const std::string &name);

		[[nodiscard]] virtual std::vector<MemoryType> GetMemoryTypes() const;

	protected:
		static constexpr const uint8_t kStreamLimit = 4;
		// A serialised import: a 64-bit resource ID and a 32-bit offset/kind field, padded to 8 bytes.
		static constexpr const size_t kImportEntrySize = 16;

		std::map<ResourceKey, ResourceEntry> m_entries;
		std::map<ResourceKey, ResourceDebugDataEntry> m_debugDataEntries;

		uint16_t m_version;
		Platform m_platform;
		Flags m_flags;

		[[nodiscard]] virtual constexpr bool AppendsImportsToResource() const = 0;
		// ReplaceResource calls this for formats that keep imports outside the resource payload (BNDL).
		virtual void StoreSeparateImports(ResourceKey, const std::vector<Import> &) {}
		[[nodiscard]] virtual bool IsValidPlatform() const;

		[[nodiscard]] std::endian GetPlatformEndian() const;

		// Decompresses or copies one stored block. Returns an empty buffer for a block without data and
		// std::nullopt when the stored data can't be decoded.
		[[nodiscard]] std::optional<Buffer> DecodeBinary(ResourceKey resourceKey, MemoryType memoryType) const;

		void ParseDebugData(const std::string &rstXML);
		[[nodiscard]] std::string GenerateDebugData() const;
		// Writes the NUL-terminated resource string table.
		void WriteDebugData(binaryio::BinaryWriter &writer) const;
		[[nodiscard]] virtual std::vector<ResourceKey> SortedDebugDataKeys() const;
		[[nodiscard]] virtual std::vector<std::pair<std::string, std::string>> GetDebugDataAttributes(const ResourceKey &resourceKey, const ResourceDebugDataEntry &debugData) const;

		[[nodiscard]] static ImportEntry ReadImport(binaryio::BinaryReader &reader);
		static void WriteImport(binaryio::BinaryWriter &writer, const Import &import);
	};
}
