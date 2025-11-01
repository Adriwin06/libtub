#pragma once
#include <libbndl/bundle.hpp>
#include <binaryio/binaryreader.hpp>
#include <binaryio/binarywriter.hpp>
#include <map>
#include <memory>
#include <optional>
#include <stdint.h>
#include <string>
#include <vector>

namespace libbndl
{
	namespace Formats
	{
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

			uint32_t importsOffset;
			Bundle::ResourceType resourceType;
			uint16_t numberOfImports;
		};

		struct ImportEntry
		{
			uint32_t resourceID;
			uint32_t offset;
		};

		struct Entry
		{
			EntryInfo info;
			EntryFileBlockData fileBlockData[3];
		};

		class Base
		{
		public:
			Base() = default;
			Base(uint32_t revisionNumber, Bundle::Platform platform, Bundle::Flags flags);
			virtual ~Base() = default;

			virtual bool Load(binaryio::BinaryReader &reader) = 0;
			virtual bool Save(binaryio::BinaryWriter &reader) = 0;

			[[nodiscard]] virtual constexpr Bundle::MagicVersion GetMagicVersion() const = 0;
			[[nodiscard]] constexpr uint32_t GetRevisionNumber() const { return m_revisionNumber; }
			[[nodiscard]] constexpr Bundle::Platform GetPlatform() const { return m_platform; }
			[[nodiscard]] constexpr Bundle::Flags GetFlags() const { return m_flags; }

			[[nodiscard]] std::optional<ResourceDebugInfo> GetResourceDebugInfo(uint32_t resourceID) const;
			[[nodiscard]] std::optional<Bundle::ResourceType> GetResourceType(uint32_t resourceID) const;
			[[nodiscard]] virtual std::optional<Bundle::Resource> GetResource(uint32_t resourceID) const = 0;
			[[nodiscard]] Bundle::Buffer GetBinary(uint32_t resourceID, Bundle::MemoryType fileBlock) const;

			bool AddResource(uint32_t resourceID, const Bundle::Resource &data, Bundle::ResourceType resourceType);
			bool AddResourceDebugInfo(uint32_t resourceID, const std::string &name, const std::string &type);

			bool ReplaceResource(uint32_t resourceID, const Bundle::Resource &data);

			[[nodiscard]] std::vector<uint32_t> GetResourceIDs() const;
			[[nodiscard]] std::map<Bundle::ResourceType, std::vector<uint32_t>> GetResourceIDsByType() const;

			[[nodiscard]] std::vector<Bundle::MemoryType> GetMemoryTypes() const;

		protected:
			std::map<uint32_t, Entry> m_entries;
			std::map<uint32_t, ResourceDebugInfo> m_debugInfoEntries;

			uint32_t m_revisionNumber;
			Bundle::Platform m_platform;
			Bundle::Flags m_flags;

			virtual constexpr bool AppendsImportsToResource() const = 0;

			static [[nodiscard]] ImportEntry ReadImport(binaryio::BinaryReader &reader);
			static void WriteImport(binaryio::BinaryWriter &writer, const Bundle::Import &import);
		};
	}
}
