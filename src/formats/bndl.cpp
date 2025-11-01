#include "bndl.hpp"
#include <iomanip>
#include <pugixml.hpp>

using namespace libbndl;
using namespace libbndl::Formats;

bool BNDL::Load(binaryio::BinaryReader &reader)
{
	m_platform = static_cast<Bundle::Platform>(0);
	auto platformReader = reader.Copy();
	for (const auto offset : { 0x4C, 0x58, 0x64 })
	{
		platformReader.Seek(offset);
		const auto platform = platformReader.Read<Bundle::Platform>();
		if (platform == Bundle::PC || platform == Bundle::Xbox360 || platform == Bundle::PS3)
		{
			m_platform = platform;
			reader.SetEndian(m_platform != Bundle::PC ? std::endian::big : std::endian::little);
			break;
		}
	}
	if (m_platform == 0)
		return false;

	m_revisionNumber = reader.Read<uint32_t>();
	if (m_revisionNumber < 3 || m_revisionNumber > 5)
		return false;

	const auto numEntries = reader.Read<uint32_t>();

	uint8_t blocks = 4;
	if (m_platform == Bundle::Xbox360)
		blocks = 5;
	else if (m_platform == Bundle::PS3)
		blocks = 6;
	uint32_t dataBlockSizes[6];
	for (uint8_t i = 0; i < blocks; i++)
	{
		dataBlockSizes[i] = reader.Read<uint32_t>();
		reader.Skip<uint32_t>(); // Alignment
	}

	reader.Seek(0x4 * blocks, std::ios::cur); // memory address stuff

	const auto idListOffset = reader.Read<uint32_t>();
	const auto idTableOffset = reader.Read<uint32_t>();
	reader.Skip<uint32_t>(); // import block
	reader.Skip<uint32_t>(); // start of data block

	reader.SetEndian(std::endian::little);
	reader.Verify<uint32_t>(m_platform);
	reader.SetEndian(m_platform != Bundle::PC ? std::endian::big : std::endian::little);

	auto compressed = 0U;
	auto uncompInfoOffset = 0U;

	if (m_revisionNumber >= 4)
	{
		compressed = reader.Read<uint32_t>();
		if (compressed)
			m_flags = Bundle::Compressed; // TODO
		else
			m_flags = static_cast<Bundle::Flags>(0);

		reader.Skip<uint32_t>(); // number of compressed resources
		uncompInfoOffset = reader.Read<uint32_t>();
	}

	if (m_revisionNumber >= 5)
	{
		reader.Skip<uint32_t>(); // main memory alignment
		reader.Skip<uint32_t>(); // graphics memory alignment
	}

	m_entries.clear();
	m_debugInfoEntries.clear();
	m_imports.clear();

	reader.Seek(idListOffset);
	std::vector<uint32_t> resourceIDs;
	for (auto i = 0U; i < numEntries; i++)
		resourceIDs.push_back(static_cast<uint32_t>(reader.Read<uint64_t>()));

	reader.Seek(idTableOffset);
	for (const auto resourceID : resourceIDs)
	{
		auto &e = m_entries[resourceID];

		reader.Skip<uint32_t>(); // unknown mem stuff
		e.info.importsOffset = reader.Read<uint32_t>();
		e.info.resourceType = reader.Read<Bundle::ResourceType>();

		if (compressed)
		{
			for (uint8_t j = 0; j < blocks; j++)
			{
				auto mappedBlock = MapBNDLBlockToBND2(j);
				if (mappedBlock == -1)
				{
					reader.Verify<uint32_t>(0); // size
					reader.Verify<uint32_t>(1); // alignment
				}
				else
				{
					e.fileBlockData[mappedBlock].compressedSize = reader.Read<uint32_t>();
					reader.Skip<uint32_t>(); // alignment
				}
			}
		}
		else
		{
			for (uint8_t j = 0; j < blocks; j++)
			{
				auto mappedBlock = MapBNDLBlockToBND2(j);
				if (mappedBlock == -1)
				{
					reader.Verify<uint32_t>(0); // size
					reader.Verify<uint32_t>(1); // alignment
				}
				else
				{
					e.fileBlockData[mappedBlock].uncompressedSize = reader.Read<uint32_t>();
					e.fileBlockData[mappedBlock].uncompressedAlignment = reader.Read<uint32_t>();
				}
			}
		}

		auto dataReader = reader.Copy();
		auto dataBlockStartOffset = 0;
		for (uint8_t j = 0; j < blocks; j++)
		{
			if (j > 0)
				dataBlockStartOffset += dataBlockSizes[j - 1];

			const auto readOffset = reader.Read<uint32_t>() + dataBlockStartOffset;
			reader.Skip<uint32_t>(); // 1

			auto mappedBlock = MapBNDLBlockToBND2(j);
			if (mappedBlock == -1)
			{
				assert(dataBlockSizes[j] == 0);
				continue;
			}

			auto &dataInfo = e.fileBlockData[mappedBlock];

			const auto readSize = compressed ? dataInfo.compressedSize : dataInfo.uncompressedSize;
			if (readSize == 0)
			{
				dataInfo.data = nullptr;
				continue;
			}

			dataReader.Seek(readOffset); // Read offset

			dataInfo.data = std::unique_ptr<uint8_t[]>(dataReader.Read<uint8_t *>(readSize));
		}

		reader.Seek(0x4 * blocks, std::ios::cur); // memory address stuff
	}

	if (compressed)
	{
		reader.Seek(uncompInfoOffset);
		for (const auto resourceID : resourceIDs)
		{
			auto &e = m_entries[resourceID];

			for (uint8_t j = 0; j < blocks; j++)
			{
				auto mappedBlock = MapBNDLBlockToBND2(j);
				if (mappedBlock == -1)
				{
					reader.Verify<uint32_t>(0); // size
					reader.Verify<uint32_t>(1); // alignment
				}
				else
				{
					e.fileBlockData[mappedBlock].uncompressedSize = reader.Read<uint32_t>();
					e.fileBlockData[mappedBlock].uncompressedAlignment = reader.Read<uint32_t>();
				}
			}
		}
	}

	for (const auto resourceID : resourceIDs)
	{
		auto &e = m_entries[resourceID];
		const auto depOffset = e.info.importsOffset;
		if (depOffset == 0)
			continue;

		reader.Seek(depOffset);
		e.info.numberOfImports = static_cast<uint16_t>(reader.Read<uint32_t>());
		reader.Verify<uint32_t>(0);
		for (auto i = 0U; i < e.info.numberOfImports; i++)
			m_imports[resourceID].emplace_back(ReadImport(reader));
	}

	auto rstFile = GetBinary(0xC039284A, Bundle::MemoryType::MainMemory);
	if (rstFile == nullptr)
		return true;

	m_flags = static_cast<Bundle::Flags>(m_flags | Bundle::HasResourceStringTable);

	auto rstReader = binaryio::BinaryReader(rstFile);

	const auto strLen = rstReader.Read<uint32_t>();
	auto rstXML = rstReader.ReadString(strLen);

	// Cover Criterion's broken XML writer.
	if (rstXML.rfind("</ResourceStringTable>", 0) == 0)
		rstXML.erase(1, 1);
	const auto pos = rstXML.find("</ResourceStringTable>\n\t");
	if (pos != std::string::npos)
		rstXML.erase(pos, 23);

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

	m_entries.erase(0xC039284A);

	return true;
};

bool BNDL::Save(binaryio::BinaryWriter &writer)
{
	if (m_revisionNumber <= 3 && (m_flags & Bundle::Compressed) != 0)
		return false; // Invalid combination

	writer.SetEndian(m_platform != Bundle::PC ? std::endian::big : std::endian::little);

	writer.Write("bndl", 4);
	writer.Write<uint32_t>(m_revisionNumber);

	const bool writeDebugData = !m_debugInfoEntries.empty() && (m_flags & Bundle::Compressed) == 0; // TODO: is the compressed check accurate?
	auto entryCount = static_cast<uint32_t>(m_entries.size());
	if (writeDebugData)
		entryCount++;

	writer.Write<uint32_t>(entryCount);

	uint8_t blocks = 4;
	if (m_platform == Bundle::Xbox360)
		blocks = 5;
	else if (m_platform == Bundle::PS3)
		blocks = 6;

	size_t dataBlockDescriptorsPos[3];
	for (uint8_t i = 0; i < blocks; i++)
	{
		auto mappedBlock = MapBNDLBlockToBND2(i);
		if (mappedBlock != -1)
			dataBlockDescriptorsPos[mappedBlock] = writer.GetOffset();
		writer.Write<uint32_t>(0); // size
		writer.Write<uint32_t>(1); // alignment
	}

	for (auto i = 0; i < blocks; i++)
	{
		writer.Write<uint32_t>(0); // memory addresses - unsupported for now.
	}

	auto idListPointerPos = writer.GetOffset();
	writer.Seek(4, std::ios::cur);
	auto idTablePointerPos = writer.GetOffset();
	writer.Seek(4, std::ios::cur);
	auto importBlockPointerPos = writer.GetOffset();
	writer.Seek(4, std::ios::cur);
	auto dataBlockPointerPos = writer.GetOffset();
	writer.Seek(4, std::ios::cur);

	writer.SetEndian(std::endian::little);
	writer.Write<uint32_t>(m_platform);
	writer.SetEndian(m_platform != Bundle::PC ? std::endian::big : std::endian::little);

	size_t uncompInfoBlockPointerPos = 0;

	if (m_revisionNumber >= 4)
	{
		writer.Write<uint32_t>(m_flags & Bundle::Compressed);
		writer.Write<uint32_t>((m_flags & Bundle::Compressed) ? entryCount : 0);
		uncompInfoBlockPointerPos = writer.GetOffset();
		writer.Write<uint32_t>(0); // will write later, but only if needed
	}

	if (m_revisionNumber >= 5)
	{
		writer.Write<uint32_t>(0); // Main memory alignment. Setting this to 0 so we don't need to deal with memory addresses.
		writer.Write<uint32_t>(0); // Graphics memory alignment.
	}

	writer.Align(0x10);

	// ID LIST
	writer.VisitAndWrite<uint32_t>(idListPointerPos, writer.GetOffset32());
	for (const auto &entry : m_entries)
	{
		writer.Write<uint64_t>(entry.first);
	}
	if (writeDebugData)
		writer.Write<uint64_t>(0xC039284A);

	// Prepare ResourceStringTable
	if (writeDebugData)
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
		const auto outStr = out.str();

		auto debugDataWriter = binaryio::BinaryWriter();
		debugDataWriter.Write(static_cast<uint32_t>(outStr.size()));
		debugDataWriter.Write(outStr);

		const auto data = debugDataWriter.GetStream().view();
		const auto dataSize = data.size();

		auto &e = m_entries[0xFFFFFFFF]; // HACK
		e.info.resourceType = Bundle::TextFile;

		e.fileBlockData[0].data = std::make_unique_for_overwrite<uint8_t[]>(dataSize);
		std::memcpy(e.fileBlockData[0].data.get(), data.data(), dataSize);

		e.fileBlockData[0].uncompressedSize = static_cast<uint32_t>(dataSize);
		e.fileBlockData[0].uncompressedAlignment = 4;
	}

	// ID TABLE
	writer.VisitAndWrite<uint32_t>(idTablePointerPos, writer.GetOffset32());

	struct FilePointerPosHelper
	{
		size_t importPointerPos;
		size_t dataBlockPointerPos[3];
	};
	std::map<uint32_t, FilePointerPosHelper> filePointerPosMap;
	for (const auto &entry : m_entries)
	{
		writer.Write<uint32_t>(0); // Ignore

		auto &posHelper = filePointerPosMap[entry.first];

		posHelper.importPointerPos = writer.GetOffset();
		writer.Write<uint32_t>(0);

		writer.Write(entry.second.info.resourceType);

		for (uint8_t i = 0; i < blocks; i++)
		{
			auto mappedBlock = MapBNDLBlockToBND2(i);
			if (mappedBlock == -1)
			{
				writer.Write<uint32_t>(0); // size
				writer.Write<uint32_t>(1); // alignment
			}
			else
			{
				const auto &blockData = entry.second.fileBlockData[mappedBlock];
				const auto size = (m_flags & Bundle::Compressed) ? blockData.compressedSize : blockData.uncompressedSize;
				writer.Write<uint32_t>(size);
				writer.Write<uint32_t>((size == 0) ? 1 : blockData.uncompressedAlignment);
			}
		}

		for (uint8_t i = 0; i < blocks; i++)
		{
			auto mappedBlock = MapBNDLBlockToBND2(i);
			if (mappedBlock != -1)
				posHelper.dataBlockPointerPos[mappedBlock] = writer.GetOffset();

			writer.Write<uint32_t>(0);
			writer.Write<uint32_t>(1); // constant
		}

		// Memory stuff - not supported for now
		for (auto i = 0; i < blocks; i++)
			writer.Write<uint32_t>(0);
	}

	// UNCOMPRESSED SIZE INFO
	if (m_flags & Bundle::Compressed)
	{
		writer.VisitAndWrite<uint32_t>(uncompInfoBlockPointerPos, writer.GetOffset32());
		for (const auto &entry : m_entries)
		{
			for (uint8_t i = 0; i < blocks; i++)
			{
				auto mappedBlock = MapBNDLBlockToBND2(i);
				if (mappedBlock == -1)
				{
					writer.Write<uint32_t>(0); // size
					writer.Write<uint32_t>(1); // alignment
				}
				else
				{
					const auto &blockData = entry.second.fileBlockData[mappedBlock];
					writer.Write<uint32_t>(blockData.uncompressedSize);
					writer.Write<uint32_t>((blockData.uncompressedSize == 0) ? 1 : blockData.uncompressedAlignment);
				}
			}
		}
	}

	// IMPORTS
	writer.VisitAndWrite<uint32_t>(importBlockPointerPos, writer.GetOffset32());
	for (const auto &entry : m_entries)
	{
		const auto &imports = m_imports[entry.first];
		if (imports.empty())
			continue;

		writer.VisitAndWrite<uint32_t>(filePointerPosMap.at(entry.first).importPointerPos, writer.GetOffset32());

		writer.Write(static_cast<uint32_t>(imports.size()));
		writer.Write<uint32_t>(0); // padding
		for (const auto &import : imports)
		{
			writer.Write<uint64_t>(import.resourceID);
			writer.Write<uint32_t>(import.offset);
			writer.Align(8);
		}
	}

	// DATA
	writer.VisitAndWrite<uint32_t>(dataBlockPointerPos, writer.GetOffset32());
	uint32_t blockStartOffset = 0;
	for (auto i = 0; i < 3; i++)
	{
		for (const auto &entry : m_entries)
		{
			const auto &e = entry.second;

			const auto &dataInfo = e.fileBlockData[i];
			const auto readSize = (m_flags & Bundle::Compressed) ? dataInfo.compressedSize : dataInfo.uncompressedSize;

			if (readSize > 0)
			{
				writer.VisitAndWrite<uint32_t>(filePointerPosMap.at(entry.first).dataBlockPointerPos[i], writer.GetOffset32() - blockStartOffset);
				writer.Write(dataInfo.data.get(), readSize);
			}
		}

		const auto size = writer.GetOffset32() - blockStartOffset;
		writer.VisitAndWrite<uint32_t>(dataBlockDescriptorsPos[i], size);
		writer.VisitAndWrite<uint32_t>(dataBlockDescriptorsPos[i], (size == 0) ? 1 : ((i >= 1) ? 4096 : 1024)); // TODO: This changes and I don't know the pattern.
		blockStartOffset = writer.GetOffset32();
	}

	m_entries.erase(0xFFFFFFFF);

	return true;
}

int8_t BNDL::MapBNDLBlockToBND2(uint8_t block) const
{
	auto mappedBlock = static_cast<int8_t>(block);
	switch (m_platform)
	{
	case Bundle::PC:
		if (block >= 3)
			mappedBlock = -1;
		break;
	case Bundle::Xbox360:
		if (block == 1 || block >= 4)
			mappedBlock = -1;
		else if (block != 0)
			mappedBlock = block - 1;
		break;
	case Bundle::PS3:
		if ((block >= 1 && block <= 3) || block >= 6)
			mappedBlock = -1;
		else if (block != 0)
			mappedBlock = block - 3;
		break;
	default:
		mappedBlock = -1;
		break;
	}
	return mappedBlock;
}

std::optional<Bundle::Resource> BNDL::GetResource(uint32_t resourceID) const
{
	const auto it = m_entries.find(resourceID);
	if (it == m_entries.end())
		return {};

	std::array<Bundle::Buffer, 3> buffers;
	for (const auto &memoryType : GetMemoryTypes())
		buffers[LIBBNDL_TO_UNDERLYING(memoryType)] = GetBinary(resourceID, memoryType);

	std::vector<Bundle::Import> imports;
	if (it->second.info.numberOfImports > 0)
	{
		const auto &importEntries = m_imports.at(resourceID);
		for (const auto &importEntry : importEntries)
			imports.emplace_back(importEntry.resourceID, importEntry.offset);
	}

	return Bundle::Resource{ std::move(buffers), std::move(imports) };
}
