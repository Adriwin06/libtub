#include "base.hpp"
#include <iomanip>
#include <limits>
#include <regex>
#include <pugixml.hpp>
#include <zlib.h>

using namespace libbndl;
using namespace libbndl::Formats;

Base::Base(uint32_t version, Platform platform, Flags flags)
{
	m_version = version;
	m_platform = platform;
	m_flags = flags;
}

std::optional<ResourceDebugInfoEntry> Base::GetResourceDebugInfo(ResourceID resourceID) const
{
	const auto it = m_debugInfoEntries.find(resourceID);
	if (it == m_debugInfoEntries.end())
		return {};

	return it->second;
}

std::optional<uint32_t> Base::GetResourceType(ResourceID resourceID) const
{
	const auto it = m_entries.find(resourceID);
	if (it == m_entries.end())
		return {};

	return it->second.resourceType;
}

Buffer Base::GetBinary(ResourceID resourceID, MemoryType memoryType) const
{
	const auto it = m_entries.find(resourceID);
	if (it == m_entries.end())
		return {};

	const auto &e = it->second;

	const auto &dataInfo = e.descriptors[LIBBNDL_TO_UNDERLYING(memoryType)];

	if (dataInfo.data == nullptr)
		return {};

	const auto &buffer = dataInfo.data;
	const auto uncompressedSize = dataInfo.uncompressedSize;

	auto uncompressedBuffer = std::make_unique_for_overwrite<uint8_t[]>(uncompressedSize);

	if (m_flags & Flags::Compressed)
	{
		uLongf uncompressedSizeLong = uncompressedSize;
		const auto ret = uncompress(uncompressedBuffer.get(), &uncompressedSizeLong, buffer.get(), static_cast<uLong>(dataInfo.onDiskSize));

		assert(ret == Z_OK);
		assert(uncompressedSize == uncompressedSizeLong);
	}
	else
	{
		assert(dataInfo.onDiskSize == dataInfo.uncompressedSize);

		std::memcpy(uncompressedBuffer.get(), buffer.get(), uncompressedSize);
	}

	return { std::move(uncompressedBuffer), uncompressedSize, dataInfo.uncompressedAlignment };
}

bool Base::AddResource(ResourceID resourceID, const Resource &resource, uint32_t resourceType)
{
	const auto it = m_entries.find(resourceID);
	if (it != m_entries.end() || resource.GetImports().size() > std::numeric_limits<uint16_t>::max())
		return false;

	auto &e = m_entries[resourceID];
	e.resourceType = resourceType;

	if (!(m_flags & Flags::Compressed))
	{
		// If we're not compressing, we need to specify the on-disk alignment.
		// It's not clear how this is determined (see below) so we'll just assume 1.
		for (const auto &memoryType : GetMemoryTypes())
		{
			auto &descriptor = e.descriptors[LIBBNDL_TO_UNDERLYING(memoryType)];
			descriptor.onDiskAlignment = 1;
		}
	}

	return ReplaceResource(resourceID, resource);
}

bool Base::AddResourceDebugInfo(ResourceID resourceID, const std::string &name, const std::string &type)
{
	const auto it = m_debugInfoEntries.find(resourceID);
	if (it != m_debugInfoEntries.end())
		return false;

	auto &debugInfo = m_debugInfoEntries[resourceID];
	debugInfo.name = name;
	debugInfo.typeName = type;

	return true;
}

bool Base::ReplaceResource(ResourceID resourceID, const Resource &resource)
{
	const auto it = m_entries.find(resourceID);
	const auto &imports = resource.GetImports();
	if (it == m_entries.end() || imports.size() > std::numeric_limits<uint16_t>::max())
		return false;

	auto &e = it->second;

	e.importHash = 0;
	e.importOffset = 0;
	e.importCount = 0;

	for (const auto &memoryType : GetMemoryTypes())
	{
		const auto &inDataInfo = resource.GetBinary(memoryType);
		auto &outDataInfo = e.descriptors[LIBBNDL_TO_UNDERLYING(memoryType)];

		if (inDataInfo == nullptr)
		{
			outDataInfo.data = nullptr;
			outDataInfo.uncompressedSize = 0;
			outDataInfo.uncompressedAlignment = 1;
			outDataInfo.onDiskSize = 0;
			// The on-disk alignment can remain set even with 0 size.
			continue;
		}

		std::unique_ptr<uint8_t[]> inBuffer;
		size_t inSize;
		std::unique_ptr<uint8_t[]> outBuffer;

		if (AppendsImportsToResource() && memoryType == MemoryType::MainMemory && !imports.empty())
		{
			binaryio::BinaryWriter writer;
			for (const auto &import : imports)
			{
				WriteImport(writer, import);
				e.importHash &= static_cast<uint64_t>(import.GetResourceID());
			}
			const auto depSize = writer.GetSize();
			auto depStream = writer.GetStream();

			auto inDataInfoSize = inDataInfo.GetSize();
			inDataInfoSize = binaryio::Align(inDataInfoSize, 16);

			inSize = inDataInfoSize + depSize;
			inBuffer = std::make_unique_for_overwrite<uint8_t[]>(inSize);
			std::memcpy(inBuffer.get(), inDataInfo.GetData(), inDataInfoSize);
			std::memcpy(inBuffer.get() + inDataInfoSize, depStream.view().data(), depSize);

			e.importOffset = static_cast<uint32_t>(inSize);
			e.importCount = static_cast<uint16_t>(imports.size());
		}
		else
		{
			inSize = inDataInfo.GetSize();
			inBuffer = std::make_unique_for_overwrite<uint8_t[]>(inSize);
			std::memcpy(inBuffer.get(), inDataInfo.GetData(), inSize);
		}

		const auto uncompressedSize = static_cast<uint32_t>(inSize);

		if (m_flags & Flags::Compressed)
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

			outDataInfo.onDiskSize = actualSize;
			outDataInfo.onDiskAlignment = 1;
		}
		else
		{
			outBuffer = std::move(inBuffer);
			outDataInfo.onDiskSize = uncompressedSize;

			// The on-disk alignment for BND2v2 this seems to always be 1. For BND2v3 it's often 16/80/80/80 but also still sometimes 1 (some GAMELOGIC).
			// We'll just leave it as is if we're replacing.
		}

		outDataInfo.uncompressedSize = uncompressedSize;
		outDataInfo.data = std::move(outBuffer);
		outDataInfo.uncompressedAlignment = inDataInfo.GetAlignment();
	}

	return true;
}

std::vector<ResourceID> Base::GetResourceIDs() const
{
	std::vector<ResourceID> entries;
	for (const auto &e : m_entries)
	{
		entries.push_back(e.first);
	}
	return entries;
}

std::map<uint32_t, std::vector<ResourceID>> Base::GetResourceIDsByType() const
{
	std::map<uint32_t, std::vector<ResourceID>> entriesByResourceType;
	for (const auto &e : m_entries)
	{
		entriesByResourceType[e.second.resourceType].push_back(e.first);
	}
	return entriesByResourceType;
}

std::vector<MemoryType> Base::GetMemoryTypes() const
{
	std::vector<MemoryType> types;
	types.reserve(m_platform == Platform::PS3 ? 3 : 2);

	types.emplace_back(MemoryType::MainMemory);
	switch (m_platform)
	{
	case Platform::PC:
		types.emplace_back(MemoryType::Disposable);
		break;
	case Platform::Xbox360:
		types.emplace_back(MemoryType::Physical);
		break;
	case Platform::PS3:
		types.emplace_back(MemoryType::GraphicsSystem);
		types.emplace_back(MemoryType::GraphicsLocal);
		break;
	}

	return types;
}

void Base::ParseDebugData(const std::string &rstXML)
{
	pugi::xml_document doc;
	if (doc.load_string(rstXML.c_str(), pugi::parse_minimal))
	{
		for (const auto &resource : doc.child("ResourceStringTable").children("Resource"))
		{
			const auto resourceID = ResourceID(std::stoull(resource.attribute("id").value(), nullptr, 16));
			auto &debugInfo = m_debugInfoEntries[resourceID];
			debugInfo.name = resource.attribute("name").value();
			debugInfo.typeName = resource.attribute("type").value();
		}
	}
}

std::string Base::GenerateDebugData() const
{
	pugi::xml_document doc;
	auto root = doc.append_child("ResourceStringTable");
	for (const auto &entry : m_debugInfoEntries)
	{
		auto entryChild = root.append_child("Resource");

		std::stringstream idStream;
		idStream << std::hex << std::setw((entry.first.GetIDType() != ResourceID::EIDType::Normal) ? 16 : 8) << std::setfill('0') << static_cast<uint64_t>(entry.first);

		entryChild.append_attribute("id").set_value(idStream.str().c_str());
		entryChild.append_attribute("type").set_value(entry.second.typeName.c_str());
		entryChild.append_attribute("name").set_value(entry.second.name.c_str());
	}

	std::stringstream out;
	doc.save(out, "\t", pugi::format_indent | pugi::format_no_declaration, pugi::encoding_utf8);
	return std::regex_replace(out.str(), std::regex(" />\n"), "/>\n");
}

ImportEntry Base::ReadImport(binaryio::BinaryReader &reader)
{
	const ImportEntry &dep = {
		ResourceID(reader.Read<uint64_t>()),
		reader.Read<uint32_t>()
	};
	reader.Skip<uint32_t>();
	return dep;
}

void Base::WriteImport(binaryio::BinaryWriter &writer, const Import &import)
{
	writer.Write<uint64_t>(import.GetResourceID());
	writer.Write(import.GetOffset());
	writer.Align(8);
}
