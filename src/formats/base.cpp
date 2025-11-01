#include "base.hpp"
#include <limits>
#include <zlib.h>

using namespace libbndl;
using namespace libbndl::Formats;

Base::Base(uint32_t revisionNumber, Bundle::Platform platform, Bundle::Flags flags)
{
	m_revisionNumber = revisionNumber;
	m_platform = platform;
	m_flags = flags;
}

std::optional<ResourceDebugInfo> Base::GetResourceDebugInfo(uint32_t resourceID) const
{
	const auto it = m_debugInfoEntries.find(resourceID);
	if (it == m_debugInfoEntries.end())
		return {};

	return it->second;
}

std::optional<Bundle::ResourceType> Base::GetResourceType(uint32_t resourceID) const
{
	const auto it = m_entries.find(resourceID);
	if (it == m_entries.end())
		return {};

	return it->second.info.resourceType;
}

Bundle::Buffer Base::GetBinary(uint32_t resourceID, Bundle::MemoryType fileBlock) const
{
	const auto it = m_entries.find(resourceID);
	if (it == m_entries.end())
		return {};

	const auto &e = it->second;

	const auto &dataInfo = e.fileBlockData[LIBBNDL_TO_UNDERLYING(fileBlock)];

	if (dataInfo.data == nullptr)
		return {};

	const auto &buffer = dataInfo.data;
	const auto uncompressedSize = dataInfo.uncompressedSize;

	auto uncompressedBuffer = std::make_unique_for_overwrite<uint8_t[]>(uncompressedSize);

	if (dataInfo.compressedSize > 0)
	{
		assert(m_flags & Bundle::Compressed);

		uLongf uncompressedSizeLong = uncompressedSize;
		const auto ret = uncompress(uncompressedBuffer.get(), &uncompressedSizeLong, buffer.get(), static_cast<uLong>(dataInfo.compressedSize));

		assert(ret == Z_OK);
		assert(uncompressedSize == uncompressedSizeLong);
	}
	else
	{
		std::memcpy(uncompressedBuffer.get(), buffer.get(), uncompressedSize);
	}

	return { std::move(uncompressedBuffer), uncompressedSize, dataInfo.uncompressedAlignment };
}

bool Base::AddResource(uint32_t resourceID, const Bundle::Resource &resource, Bundle::ResourceType resourceType)
{
	const auto it = m_entries.find(resourceID);
	if (it != m_entries.end() || resource.GetImports().size() > std::numeric_limits<uint16_t>::max())
		return false;

	Entry &e = m_entries[resourceID];
	e.info.resourceType = resourceType;

	return ReplaceResource(resourceID, resource);
}

bool Base::AddResourceDebugInfo(uint32_t resourceID, const std::string &name, const std::string &type)
{
	const auto it = m_debugInfoEntries.find(resourceID);
	if (it != m_debugInfoEntries.end())
		return false;

	ResourceDebugInfo &debugInfo = m_debugInfoEntries[resourceID];
	debugInfo.name = name;
	debugInfo.typeName = type;

	return true;
}

bool Base::ReplaceResource(uint32_t resourceID, const Bundle::Resource &resource)
{
	const auto it = m_entries.find(resourceID);
	const auto &imports = resource.GetImports();
	if (it == m_entries.end() || imports.size() > std::numeric_limits<uint16_t>::max())
		return false;

	Entry &e = it->second;

	e.info.checksum = 0;
	e.info.importsOffset = 0;
	e.info.numberOfImports = 0;

	for (const auto &memoryType : GetMemoryTypes())
	{
		const auto &inDataInfo = resource.GetBinary(memoryType);
		auto &outDataInfo = e.fileBlockData[LIBBNDL_TO_UNDERLYING(memoryType)];

		if (inDataInfo == nullptr)
		{
			outDataInfo.data = nullptr;
			outDataInfo.uncompressedSize = 0;
			outDataInfo.compressedSize = 0;
			continue;
		}

		std::unique_ptr<uint8_t[]> inBuffer;
		size_t inSize;
		std::unique_ptr<uint8_t[]> outBuffer;

		if (memoryType == Bundle::MemoryType::MainMemory)
		{
			//
		}

		if (AppendsImportsToResource() && memoryType == Bundle::MemoryType::MainMemory && !imports.empty())
		{
			binaryio::BinaryWriter writer;
			for (const auto &import : imports)
			{
				WriteImport(writer, import);
				e.info.checksum &= import.GetResourceID();
			}
			const auto depSize = writer.GetSize();
			auto depStream = writer.GetStream();

			const auto inDataInfoSize = inDataInfo.GetSize();
			binaryio::Align(inDataInfoSize, 16);

			inSize = inDataInfoSize + depSize;
			inBuffer = std::make_unique_for_overwrite<uint8_t[]>(inSize);
			std::memcpy(inBuffer.get(), inDataInfo.GetData(), inDataInfoSize);
			std::memcpy(inBuffer.get() + inDataInfoSize, depStream.view().data(), depSize);

			e.info.importsOffset = static_cast<uint32_t>(inSize);
			e.info.numberOfImports = static_cast<uint16_t>(imports.size());
		}
		else
		{
			inSize = inDataInfo.GetSize();
			inBuffer = std::make_unique_for_overwrite<uint8_t[]>(inSize);
			std::memcpy(inBuffer.get(), inDataInfo.GetData(), inSize);
		}

		const auto uncompressedSize = static_cast<uint32_t>(inSize);

		if (m_flags & Bundle::Compressed)
		{
			const auto compBufferSize = compressBound(static_cast<uLong>(inSize));
			std::vector<uint8_t> compBuffer(compBufferSize);
			uLongf actualSize = compBufferSize;
			const auto ret = compress2(compBuffer.data(), &actualSize, inBuffer.get(), static_cast<uLong>(inSize), Z_BEST_COMPRESSION);

			if (ret != Z_OK)
			{
				assert(0);
				return false;
			}

			outBuffer = std::make_unique_for_overwrite<uint8_t[]>(actualSize);
			std::memcpy(outBuffer.get(), compBuffer.data(), actualSize);

			outDataInfo.compressedSize = actualSize;
		}
		else
		{
			outBuffer = std::move(inBuffer);
			outDataInfo.compressedSize = 0;
		}

		outDataInfo.uncompressedSize = uncompressedSize;
		outDataInfo.data = std::move(outBuffer);
		outDataInfo.uncompressedAlignment = inDataInfo.GetAlignment();
	}

	return true;
}

std::vector<uint32_t> Base::GetResourceIDs() const
{
	std::vector<uint32_t> entries;
	for (const auto &e : m_entries)
	{
		entries.push_back(e.first);
	}
	return entries;
}

std::map<Bundle::ResourceType, std::vector<uint32_t>> Base::GetResourceIDsByType() const
{
	std::map<Bundle::ResourceType, std::vector<uint32_t>> entriesByResourceType;
	for (const auto &e : m_entries)
	{
		entriesByResourceType[e.second.info.resourceType].push_back(e.first);
	}
	return entriesByResourceType;
}

std::vector<Bundle::MemoryType> Base::GetMemoryTypes() const
{
	std::vector<Bundle::MemoryType> types;
	types.reserve(m_platform == Bundle::PS3 ? 3 : 2);

	types.emplace_back(Bundle::MemoryType::MainMemory);
	types.emplace_back(Bundle::MemoryType::GraphicsSystem);
	if (m_platform == Bundle::PS3)
		types.emplace_back(Bundle::MemoryType::GraphicsLocal);

	return types;
}

ImportEntry Base::ReadImport(binaryio::BinaryReader &reader)
{
	const ImportEntry &dep = {
		static_cast<uint32_t>(reader.Read<uint64_t>()),
		reader.Read<uint32_t>()
	};
	reader.Skip<uint32_t>();
	return dep;
}

void Base::WriteImport(binaryio::BinaryWriter &writer, const Bundle::Import &import)
{
	writer.Write<uint64_t>(import.GetResourceID());
	writer.Write(import.GetOffset());
	writer.Align(8);
}
