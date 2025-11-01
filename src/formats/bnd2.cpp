#include "bnd2.hpp"
#include <iomanip>
#include <limits>
#include <regex>
#include <pugixml.hpp>

using namespace libbndl;
using namespace libbndl::Formats;

#ifndef __has_builtin
#	define __has_builtin(x) 0
#endif
static inline unsigned long BitScanReverse(unsigned long input)
{
	unsigned long result;

#if defined(_MSC_VER)
	_BitScanReverse(&result, input);
#elif __has_builtin(__builtin_clzl) || defined(__GNUC__)
	result = static_cast<unsigned long>(std::numeric_limits<unsigned long>::digits - 1 - __builtin_clzl(input));
#else
	result = std::bit_width(input | 1U) - 1;
#endif

	return result;
}

bool BND2::Load(binaryio::BinaryReader &reader)
{
	m_revisionNumber = reader.Read<uint32_t>();

	m_platform = reader.Read<Bundle::Platform>();
	reader.SetEndian(m_platform != Bundle::PC ? std::endian::big : std::endian::little);

	if (reader.GetEndian() != std::endian::native)
		m_revisionNumber = (m_revisionNumber << 24) | (m_revisionNumber << 8 & 0xff0000) | (m_revisionNumber >> 8 & 0xff00) | (m_revisionNumber >> 24);
	// Little sanity check.
	if (m_revisionNumber != 2)
		return false;

	const auto rstOffset = reader.Read<uint32_t>();
	const auto numEntries = reader.Read<uint32_t>();

	const auto idBlockOffset = reader.Read<uint32_t>();
	uint32_t fileBlockOffsets[3] = {
		reader.Read<uint32_t>(),
		reader.Read<uint32_t>(),
		reader.Read<uint32_t>()
	};

	m_flags = reader.Read<Bundle::Flags>();

	// Last 8 bytes are padding.


	m_entries.clear();
	m_debugInfoEntries.clear();

	reader.Seek(idBlockOffset);
	for (auto i = 0U; i < numEntries; i++)
	{
		// These are stored in bundle as 64-bit (8-byte), but are really 32-bit.
		auto resourceID = static_cast<uint32_t>(reader.Read<uint64_t>());
		assert(resourceID != 0);
		auto &e = m_entries[resourceID];
		e.info.checksum = static_cast<uint32_t>(reader.Read<uint64_t>());

		// The uncompressed sizes have a high nibble that varies depending on the resource type.
		const auto uncompSize0 = reader.Read<uint32_t>();
		e.fileBlockData[0].uncompressedSize = uncompSize0 & ~(0xFU << 28);
		e.fileBlockData[0].uncompressedAlignment = 1 << (uncompSize0 >> 28);
		const auto uncompSize1 = reader.Read<uint32_t>();
		e.fileBlockData[1].uncompressedSize = uncompSize1 & ~(0xFU << 28);
		e.fileBlockData[1].uncompressedAlignment = 1 << (uncompSize1 >> 28);
		const auto uncompSize2 = reader.Read<uint32_t>();
		e.fileBlockData[2].uncompressedSize = uncompSize2 & ~(0xFU << 28);
		e.fileBlockData[2].uncompressedAlignment = 1 << (uncompSize2 >> 28);

		e.fileBlockData[0].compressedSize = reader.Read<uint32_t>();
		e.fileBlockData[1].compressedSize = reader.Read<uint32_t>();
		e.fileBlockData[2].compressedSize = reader.Read<uint32_t>();

		auto dataReader = reader.Copy();
		for (auto j = 0; j < 3; j++)
		{
			dataReader.Seek(fileBlockOffsets[j] + reader.Read<uint32_t>()); // Read offset

			auto &dataInfo = e.fileBlockData[j];

			const auto readSize = (m_flags & Bundle::Compressed) ? dataInfo.compressedSize : dataInfo.uncompressedSize;
			if (readSize == 0)
			{
				dataInfo.data = nullptr;
				continue;
			}

			dataInfo.data = std::unique_ptr<uint8_t[]>(dataReader.Read<uint8_t *>(readSize));
		}

		e.info.importsOffset = reader.Read<uint32_t>();
		e.info.resourceType = reader.Read<Bundle::ResourceType>();
		e.info.numberOfImports = reader.Read<uint16_t>();

		reader.Seek(2, std::ios::cur); // Padding
	}

	if (m_flags & Bundle::HasResourceStringTable)
	{
		reader.Seek(rstOffset, std::ios::beg);

		const auto rstXML = reader.ReadString();

		pugi::xml_document doc;
		if (doc.load_string(rstXML.c_str(), pugi::parse_minimal))
		{
			for (const auto &resource : doc.child("ResourceStringTable").children("Resource"))
			{
				const auto resourceID = std::stoul(resource.attribute("id").value(), nullptr, 16);
				auto &debugInfo = m_debugInfoEntries[resourceID];
				debugInfo.name = resource.attribute("name").value();
				debugInfo.typeName = resource.attribute("type").value();
			}
		}
	}

	return true;
};

bool BND2::Save(binaryio::BinaryWriter &writer)
{
	writer.Write("bnd2", 4);
	writer.Write<uint32_t>(2); // Bundle version
	writer.Write(Bundle::PC); // Only PC writing supported for now.

	auto rstPointerPos = writer.GetOffset();
	writer.Seek(4, std::ios::cur); // write later

	writer.Write(static_cast<uint32_t>(m_entries.size()));

	auto idBlockPointerPos = writer.GetOffset();
	writer.Seek(4, std::ios::cur); // write later
	size_t fileBlockPointerPos[3];
	for (auto &pointerPos : fileBlockPointerPos)
	{
		pointerPos = writer.GetOffset();
		writer.Seek(4, std::ios::cur);
	}

	writer.Write(m_flags);

	writer.Align(16);


	// RESOURCE STRING TABLE
	writer.VisitAndWrite<uint32_t>(rstPointerPos, writer.GetOffset32());
	if (m_flags & Bundle::HasResourceStringTable)
	{
		pugi::xml_document doc;
		auto root = doc.append_child("ResourceStringTable");
		for (const auto &entry : m_debugInfoEntries)
		{
			auto entryChild = root.append_child("Resource");

			std::stringstream idStream;
			idStream << std::hex << std::setw(8) << std::setfill('0') << entry.first;

			entryChild.append_attribute("id").set_value(idStream.str().c_str());
			entryChild.append_attribute("type").set_value(entry.second.typeName.c_str());
			entryChild.append_attribute("name").set_value(entry.second.name.c_str());
		}

		std::stringstream out;
		doc.save(out, "\t", pugi::format_indent | pugi::format_no_declaration, pugi::encoding_utf8);
		const auto outStr = std::regex_replace(out.str(), std::regex(" />\n"), "/>\n");
		writer.Write(outStr);

		writer.Align(16);
	}


	// ID BLOCK
	writer.VisitAndWrite<uint32_t>(idBlockPointerPos, writer.GetOffset32());
	auto entryDataPointerPos = std::vector<std::array<size_t, 3>>(m_entries.size());
	auto entryIter = m_entries.begin();
	for (auto i = 0U; i < m_entries.size(); i++)
	{
		writer.Write<uint64_t>(entryIter->first);

		const auto &e = entryIter->second;

		writer.Write<uint64_t>(e.info.checksum);

		for (auto &dataInfo : e.fileBlockData)
			writer.Write(dataInfo.uncompressedSize | (BitScanReverse(dataInfo.uncompressedAlignment) << 28));
		for (auto &dataInfo : e.fileBlockData)
			writer.Write(dataInfo.compressedSize);
		for (auto j = 0; j < 3; j++)
		{
			entryDataPointerPos[i][j] = writer.GetOffset();
			writer.Seek(4, std::ios::cur);
		}

		writer.Write(e.info.importsOffset);
		writer.Write(e.info.resourceType);
		writer.Write(e.info.numberOfImports);

		writer.Seek(2, std::ios::cur); // padding

		entryIter = std::next(entryIter);
	}

	// DATA BLOCK
	for (auto i = 0; i < 3; i++)
	{
		const auto blockStart = writer.GetOffset32();
		writer.VisitAndWrite<uint32_t>(fileBlockPointerPos[i], blockStart);

		entryIter = m_entries.begin();
		for (auto j = 0U; j < m_entries.size(); j++)
		{
			const auto &e = entryIter->second;

			const auto &dataInfo = e.fileBlockData[i];
			const auto readSize = (m_flags & Bundle::Compressed) ? dataInfo.compressedSize : dataInfo.uncompressedSize;

			if (readSize > 0)
			{
				writer.VisitAndWrite<uint32_t>(entryDataPointerPos[j][i], writer.GetOffset32() - blockStart);
				writer.Write(dataInfo.data.get(), readSize);
				writer.Align((i != 0 && j != m_entries.size() - 1) ? 0x80 : 16);
			}

			entryIter = std::next(entryIter);
		}

		if (i != 2)
			writer.Align(0x80);
	}

	return true;
}

std::optional<Bundle::Resource> BND2::GetResource(uint32_t resourceID) const
{
	const auto it = m_entries.find(resourceID);
	if (it == m_entries.end())
		return {};

	std::array<Bundle::Buffer, 3> buffers;
	for (const auto &memoryType : GetMemoryTypes())
		buffers[LIBBNDL_TO_UNDERLYING(memoryType)] = GetBinary(resourceID, memoryType);

	std::vector<Bundle::Import> imports;
	const auto numImports = it->second.info.numberOfImports;
	if (numImports > 0)
	{
		imports.reserve(numImports);

		binaryio::BinaryReader reader(buffers[0], m_platform != Bundle::PC ? std::endian::big : std::endian::little);
		reader.Seek(it->second.info.importsOffset);
		for (auto i = 0U; i < numImports; i++)
		{
			const auto &importEntry = ReadImport(reader);
			imports.emplace_back(importEntry.resourceID, importEntry.offset);
		}

		auto buffer = std::make_unique_for_overwrite<uint8_t[]>(buffers[0].GetSize());
		std::memcpy(buffer.get(), buffers[0].GetData(), buffers[0].GetSize());
		buffers[0] = { std::move(buffer), buffers[0].GetSize(), buffers[0].GetAlignment() };
	}

	return Bundle::Resource{ std::move(buffers), std::move(imports) };
}
