#include <libbndl/bundle.hpp>
#include <binaryio/binaryreader.hpp>
#include <binaryio/binarywriter.hpp>
#include <fstream>
#include <cassert>
#include <cstring>
#include <zlib.h>
#include <pugixml.hpp>
#include <regex>
#include <iomanip>
#include <array>

using namespace libbndl;

#ifndef __has_builtin
#	define __has_builtin(x) 0
#endif
inline unsigned long BitScanReverse(unsigned long input)
{
	unsigned long result;

#if defined(_MSC_VER)
	_BitScanReverse(&result, input);
#elif __has_builtin(__builtin_clzl) || defined(__GNUC__)
	result = static_cast<unsigned long>(31 - __builtin_clzl(input));
#else
#	error "Unsupported compiler."
#endif

	return result;
}

Bundle::Bundle(MagicVersion magicVersion, uint32_t revisionNumber, Platform platform, Flags flags)
{
	m_magicVersion = magicVersion;
	m_revisionNumber = revisionNumber;
	m_platform = platform;
	m_flags = flags;
}

bool Bundle::Load(const std::string &name)
{
	std::ifstream stream;

	stream.open(name, std::ios::in | std::ios::binary | std::ios::ate);

	// Check if archive exists
	if (stream.fail())
		return false;

	const auto fileSize = stream.tellg();
	if (fileSize < 4)
		return false;

	stream.seekg(0, std::ios::beg);
	const auto &buffer = std::make_shared<std::vector<uint8_t>>(fileSize);
	stream.read(reinterpret_cast<char *>(buffer->data()), fileSize);
	stream.close();
	auto reader = binaryio::BinaryReader(buffer);

	// Check if it's a BNDL archive
	auto magic = reader.ReadString(4);
	if (magic == std::string("bndl"))
		m_magicVersion = BNDL;
	else if (magic == std::string("bnd2"))
		m_magicVersion = BND2;
	else
		return false;

	return (m_magicVersion == BNDL) ? LoadBNDL(reader): LoadBND2(reader);
}

bool Bundle::LoadBND2(binaryio::BinaryReader &reader)
{
	m_revisionNumber = reader.Read<uint32_t>();

	m_platform = reader.Read<Platform>();
	reader.SetBigEndian(m_platform != PC);

	if (reader.IsBigEndian())
		m_revisionNumber = (m_revisionNumber << 24) | (m_revisionNumber << 8 & 0xff0000) | (m_revisionNumber >> 8 & 0xff00) | (m_revisionNumber >> 24);
	// Little sanity check.
	if (m_revisionNumber != 2)
		return false;

	const auto rstOffset = reader.Read<uint32_t>();
	const auto numEntries = reader.Read<uint32_t>();

	const auto idBlockOffset = reader.Read<uint32_t>();
	uint32_t fileBlockOffsets[3];
	fileBlockOffsets[0] = reader.Read<uint32_t>();
	fileBlockOffsets[1] = reader.Read<uint32_t>();
	fileBlockOffsets[2] = reader.Read<uint32_t>();

	m_flags = reader.Read<Flags>();

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

			const auto readSize = (m_flags & Compressed) ? dataInfo.compressedSize : dataInfo.uncompressedSize;
			if (readSize == 0)
			{
				dataInfo.data = nullptr;
				continue;
			}

			const auto readBuffer = dataReader.Read<uint8_t *>(readSize);
			dataInfo.data = std::make_unique<std::vector<uint8_t>>(readBuffer, readBuffer + readSize);
			delete[] readBuffer;
		}

		e.info.dependenciesOffset = reader.Read<uint32_t>();
		e.info.resourceType = reader.Read<ResourceType>();
		e.info.numberOfDependencies = reader.Read<uint16_t>();

		reader.Seek(2, std::ios::cur); // Padding
	}

	if (m_flags & HasResourceStringTable)
	{
		reader.Seek(rstOffset, std::ios::beg);

		const auto rstXML = reader.ReadString();

		pugi::xml_document doc;
		if (doc.load_string(rstXML.c_str(), pugi::parse_minimal))
		{
			for (const auto resource : doc.child("ResourceStringTable").children("Resource"))
			{
				const auto resourceID = std::stoul(resource.attribute("id").value(), nullptr, 16);
				auto &debugInfo = m_debugInfoEntries[resourceID];
				debugInfo.name = resource.attribute("name").value();
				debugInfo.typeName = resource.attribute("type").value();
			}
		}
	}

	return true;
}

bool Bundle::LoadBNDL(binaryio::BinaryReader &reader)
{
	m_platform = static_cast<Platform>(0);
	auto platformReader = reader.Copy();
	for (const auto offset : { 0x4C, 0x58, 0x64 })
	{
		platformReader.Seek(offset);
		const auto platform = platformReader.Read<Platform>();
		if (platform == PC || platform == Xbox360 || platform == PS3)
		{
			m_platform = platform;
			reader.SetBigEndian(m_platform != PC);
			break;
		}
	}
	if (m_platform == 0)
		return false;

	m_revisionNumber = reader.Read<uint32_t>();
	if (m_revisionNumber < 3 || m_revisionNumber > 5)
		return false;

	const auto numEntries = reader.Read<uint32_t>();

	auto blocks = 4;
	if (m_platform == Xbox360)
		blocks = 5;
	else if (m_platform == PS3)
		blocks = 6;
	uint32_t dataBlockSizes[6];
	for (auto i = 0; i < blocks; i++)
	{
		dataBlockSizes[i] = reader.Read<uint32_t>();
		reader.Skip<uint32_t>(); // Alignment
	}

	reader.Seek(0x4 * blocks, std::ios::cur); // memory address stuff

	const auto idListOffset = reader.Read<uint32_t>();
	const auto idTableOffset = reader.Read<uint32_t>();
	reader.Skip<uint32_t>(); // dependency block
	reader.Skip<uint32_t>(); // start of data block

	reader.SetBigEndian(false);
	reader.Verify<uint32_t>(m_platform);
	reader.SetBigEndian(m_platform != PC);

	auto compressed = 0U;
	auto uncompInfoOffset = 0U;

	if (m_revisionNumber >= 4)
	{
		compressed = reader.Read<uint32_t>();
		if (compressed)
			m_flags = Compressed; // TODO
		else
			m_flags = static_cast<Flags>(0);

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
	m_dependencies.clear();

	reader.Seek(idListOffset);
	std::vector<uint32_t> resourceIDs;
	for (auto i = 0U; i < numEntries; i++)
		resourceIDs.push_back(static_cast<uint32_t>(reader.Read<uint64_t>()));

	reader.Seek(idTableOffset);
	for (const auto resourceID : resourceIDs)
	{
		auto &e = m_entries[resourceID];

		reader.Skip<uint32_t>(); // unknown mem stuff
		e.info.dependenciesOffset = reader.Read<uint32_t>();
		e.info.resourceType = reader.Read<ResourceType>();

		if (compressed)
		{
			for (auto j = 0; j < blocks; j++)
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
			for (auto j = 0; j < blocks; j++)
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
		for (auto j = 0; j < blocks; j++)
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

			const auto readBuffer = dataReader.Read<uint8_t *>(readSize);
			dataInfo.data = std::make_unique<std::vector<uint8_t>>(readBuffer, readBuffer + readSize);
			delete[] readBuffer;
		}

		reader.Seek(0x4 * blocks, std::ios::cur); // memory address stuff
	}

	if (compressed)
	{
		reader.Seek(uncompInfoOffset);
		for (const auto resourceID : resourceIDs)
		{
			auto &e = m_entries[resourceID];

			for (auto j = 0; j < blocks; j++)
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
		const auto depOffset = e.info.dependenciesOffset;
		if (depOffset == 0)
			continue;

		reader.Seek(depOffset);
		e.info.numberOfDependencies = static_cast<uint16_t>(reader.Read<uint32_t>());
		reader.Verify<uint32_t>(0);
		for (auto i = 0U; i < e.info.numberOfDependencies; i++)
			m_dependencies[resourceID].emplace_back(ReadDependency(reader));
	}

	auto rstFile = GetBinary(0xC039284A, 0);
	if (rstFile == nullptr)
		return true;

	m_flags = static_cast<Flags>(m_flags | HasResourceStringTable);

	auto rstReader = binaryio::BinaryReader(std::move(rstFile));

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
		for (const auto resource : doc.child("ResourceStringTable").children("Resource"))
		{
			const auto resourceID = std::stoul(resource.attribute("id").value(), nullptr, 16);
			auto &debugInfo = m_debugInfoEntries[resourceID];
			debugInfo.name = resource.attribute("name").value();
			debugInfo.typeName = resource.attribute("type").value();
		}
	}

	m_entries.erase(0xC039284A);

	return true;
}

int8_t Bundle::MapBNDLBlockToBND2(uint8_t block) const
{
	auto mappedBlock = block;
	switch (m_platform)
	{
	case PC:
		if (block >= 3)
			mappedBlock = -1;
		break;
	case Xbox360:
		if (block == 1 || block >= 4)
			mappedBlock = -1;
		else if (block != 0)
			mappedBlock = block - 1;
		break;
	case PS3:
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

bool Bundle::Save(const std::string &name)
{
	auto writer = binaryio::BinaryWriter();

	switch (m_magicVersion)
	{
	case BNDL:
		if (!SaveBNDL(writer))
			return false;
		break;

	case BND2:
		if (!SaveBND2(writer))
			return false;
		break;

	default:
		return false;
	}

	std::ofstream f(name, std::ios::out | std::ios::binary);
	f << writer.GetStream().rdbuf();
	f.close();

	return true;
}


bool Bundle::SaveBND2(binaryio::BinaryWriter &writer)
{
	writer.Write("bnd2", 4);
	writer.Write<uint32_t>(2); // Bundle version
	writer.Write(PC); // Only PC writing supported for now.

	auto rstPointerPos = writer.GetOffset();
	writer.Seek(4, std::ios::cur); // write later

	writer.Write(static_cast<uint32_t>(m_entries.size()));

	auto idBlockPointerPos = writer.GetOffset();
	writer.Seek(4, std::ios::cur); // write later
	off_t fileBlockPointerPos[3];
	for (auto &pointerPos : fileBlockPointerPos)
	{
		pointerPos = writer.GetOffset();
		writer.Seek(4, std::ios::cur);
	}

	writer.Write(m_flags);

	writer.Align(16);


	// RESOURCE STRING TABLE
	writer.VisitAndWrite<uint32_t>(rstPointerPos, writer.GetOffset());
	if (m_flags & HasResourceStringTable)
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
	writer.VisitAndWrite<uint32_t>(idBlockPointerPos, writer.GetOffset());
	auto entryDataPointerPos = std::vector<std::array<off_t, 3>>(m_entries.size());
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

		writer.Write(e.info.dependenciesOffset);
		writer.Write(e.info.resourceType);
		writer.Write(e.info.numberOfDependencies);

		writer.Seek(2, std::ios::cur); // padding

		entryIter = std::next(entryIter);
	}

	// DATA BLOCK
	for (auto i = 0; i < 3; i++)
	{
		const auto blockStart = writer.GetOffset();
		writer.VisitAndWrite<uint32_t>(fileBlockPointerPos[i], blockStart);

		entryIter = m_entries.begin();
		for (auto j = 0U; j < m_entries.size(); j++)
		{
			const auto &e = entryIter->second;

			const auto &dataInfo = e.fileBlockData[i];
			const auto readSize = (m_flags & Compressed) ? dataInfo.compressedSize : dataInfo.uncompressedSize;

			if (readSize > 0)
			{
				writer.VisitAndWrite<uint32_t>(entryDataPointerPos[j][i], writer.GetOffset() - blockStart);
				writer.Write(dataInfo.data->data(), readSize);
				writer.Align((i != 0 && j != m_entries.size() - 1) ? 0x80 : 16);
			}

			entryIter = std::next(entryIter);
		}

		if (i != 2)
			writer.Align(0x80);
	}

	return true;
}

bool Bundle::SaveBNDL(binaryio::BinaryWriter &writer)
{
	if (m_revisionNumber <= 3 && (m_flags & Compressed) != 0)
		return false; // Invalid combination

	writer.SetBigEndian(m_platform != PC);

	writer.Write("bndl", 4);
	writer.Write<uint32_t>(m_revisionNumber);

	const bool writeDebugData = !m_debugInfoEntries.empty() && (m_flags & Compressed) == 0; // TODO: is the compressed check accurate?
	auto entryCount = static_cast<uint32_t>(m_entries.size());
	if (writeDebugData)
		entryCount++;

	writer.Write<uint32_t>(entryCount);

	auto blocks = 4;
	if (m_platform == Xbox360)
		blocks = 5;
	else if (m_platform == PS3)
		blocks = 6;

	off_t dataBlockDescriptorsPos[3];
	for (auto i = 0; i < blocks; i++)
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

	writer.SetBigEndian(false);
	writer.Write<uint32_t>(m_platform);
	writer.SetBigEndian(m_platform != PC);

	off_t uncompInfoBlockPointerPos = 0;

	if (m_revisionNumber >= 4)
	{
		writer.Write<uint32_t>(m_flags & Compressed);
		writer.Write<uint32_t>((m_flags & Compressed) ? entryCount : 0);
		auto uncompInfoBlockPointerPos = writer.GetOffset();
		writer.Write<uint32_t>(0); // will write later, but only if needed
	}

	if (m_revisionNumber >= 5)
	{
		writer.Write<uint32_t>(0); // Main memory alignment. Setting this to 0 so we don't need to deal with memory addresses.
		writer.Write<uint32_t>(0); // Graphics memory alignment.
	}

	writer.Align(0x10);

	// ID LIST
	writer.VisitAndWrite<uint32_t>(idListPointerPos, writer.GetOffset());
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

		const auto data = debugDataWriter.GetStream().str();

		auto &e = m_entries[0xFFFFFFFF]; // HACK
		e.info.resourceType = TextFile;
		e.fileBlockData[0].data = std::make_unique<std::vector<uint8_t>>(data.begin(), data.end());
		e.fileBlockData[0].uncompressedSize = static_cast<uint32_t>(data.size());
		e.fileBlockData[0].uncompressedAlignment = 4;
	}

	// ID TABLE
	writer.VisitAndWrite<uint32_t>(idTablePointerPos, writer.GetOffset());

	struct FilePointerPosHelper
	{
		off_t importPointerPos;
		off_t dataBlockPointerPos[3];
	};
	std::map<uint32_t, FilePointerPosHelper> filePointerPosMap;
	for (const auto &entry : m_entries)
	{
		writer.Write<uint32_t>(0); // Ignore

		auto &posHelper = filePointerPosMap[entry.first];

		posHelper.importPointerPos = writer.GetOffset();
		writer.Write<uint32_t>(0);

		writer.Write(entry.second.info.resourceType);

		for (auto i = 0; i < blocks; i++)
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
				const auto size = (m_flags & Compressed) ? blockData.compressedSize : blockData.uncompressedSize;
				writer.Write<uint32_t>(size);
				writer.Write<uint32_t>((size == 0) ? 1 : blockData.uncompressedAlignment);
			}
		}

		for (auto i = 0; i < blocks; i++)
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
	if (m_flags & Compressed)
	{
		writer.VisitAndWrite<uint32_t>(uncompInfoBlockPointerPos, writer.GetOffset());
		for (const auto &entry : m_entries)
		{
			for (auto i = 0; i < blocks; i++)
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
	writer.VisitAndWrite<uint32_t>(importBlockPointerPos, writer.GetOffset());
	for (const auto &entry : m_entries)
	{
		const auto &imports = m_dependencies[entry.first];
		if (imports.empty())
			continue;

		writer.VisitAndWrite<uint32_t>(filePointerPosMap.at(entry.first).importPointerPos, writer.GetOffset());

		writer.Write(static_cast<uint32_t>(imports.size()));
		writer.Write<uint32_t>(0); // padding
		for (const auto &import : imports)
		{
			writer.Write<uint64_t>(import.resourceID);
			writer.Write<uint32_t>(import.internalOffset);
			writer.Align(8);
		}
	}

	// DATA
	writer.VisitAndWrite<uint32_t>(dataBlockPointerPos, writer.GetOffset());
	off_t blockStartOffset = 0;
	for (auto i = 0; i < 3; i++)
	{
		for (const auto &entry : m_entries)
		{
			const auto &e = entry.second;

			const auto &dataInfo = e.fileBlockData[i];
			const auto readSize = (m_flags & Compressed) ? dataInfo.compressedSize : dataInfo.uncompressedSize;

			if (readSize > 0)
			{
				writer.VisitAndWrite<uint32_t>(filePointerPosMap.at(entry.first).dataBlockPointerPos[i], writer.GetOffset() - blockStartOffset);
				writer.Write(dataInfo.data->data(), readSize);
			}
		}

		const auto size = writer.GetOffset() - blockStartOffset;
		writer.VisitAndWrite<uint32_t>(dataBlockDescriptorsPos[i], size);
		writer.VisitAndWrite<uint32_t>(dataBlockDescriptorsPos[i], (size == 0) ? 1 : ((i >= 1) ? 4096 : 1024)); // TODO: This changes and I don't know the pattern.
		blockStartOffset = writer.GetOffset();
	}

	m_entries.erase(0xFFFFFFFF);

	return true;
}

uint32_t Bundle::HashResourceName(std::string resourceName) const
{
	std::transform(resourceName.begin(), resourceName.end(), resourceName.begin(), tolower);
	return crc32_z(0, reinterpret_cast<const Bytef *>(resourceName.c_str()), resourceName.length());
}

Bundle::Dependency Bundle::ReadDependency(binaryio::BinaryReader &reader)
{
	const Dependency &dep = {
		static_cast<uint32_t>(reader.Read<uint64_t>()),
		reader.Read<uint32_t>()
	};
	reader.Skip<uint32_t>();
	return dep;
}

std::optional<Bundle::EntryData> Bundle::GetData(const std::string &resourceName) const
{
	return GetData(HashResourceName(resourceName));
}

std::optional<Bundle::EntryData> Bundle::GetData(uint32_t resourceID) const
{
	const auto it = m_entries.find(resourceID);
	if (it == m_entries.end())
		return {};

	EntryData data;
	for (auto i = 0; i < 3; i++)
	{
		data.fileBlockData[i] = GetBinary(resourceID, i);
		data.alignments[i] = it->second.fileBlockData[i].uncompressedAlignment;
	}

	const auto numDependencies = it->second.info.numberOfDependencies;
	if (numDependencies > 0)
	{
		if (m_magicVersion == BNDL)
		{
			data.dependencies = m_dependencies.at(resourceID);
		}
		else
		{
			const auto buffer = std::make_shared<std::vector<uint8_t>>(data.fileBlockData[0]->begin() + it->second.info.dependenciesOffset, data.fileBlockData[0]->end());
			binaryio::BinaryReader reader(buffer, m_platform != PC);
			for (auto i = 0U; i < numDependencies; i++)
				data.dependencies.emplace_back(ReadDependency(reader));
			data.fileBlockData[0]->resize(data.fileBlockData[0]->size() - buffer->size());
		}
	}

	return std::move(data);
}

std::unique_ptr<std::vector<uint8_t>> Bundle::GetBinary(const std::string &resourceName, uint32_t fileBlock) const
{
	return GetBinary(HashResourceName(resourceName), fileBlock);
}

std::unique_ptr<std::vector<uint8_t>> Bundle::GetBinary(uint32_t resourceID, uint32_t fileBlock) const
{
	const auto it = m_entries.find(resourceID);
	if (it == m_entries.end())
		return {};

	const auto &e = it->second;

	const auto &dataInfo = e.fileBlockData[fileBlock];

	if (dataInfo.data == nullptr)
		return {};

	const auto &buffer = dataInfo.data;
	const auto uncompressedSize = dataInfo.uncompressedSize;

	auto uncompressedBuffer = std::make_unique<std::vector<uint8_t>>(uncompressedSize);

	if (dataInfo.compressedSize > 0)
	{
		assert(m_flags & Compressed);

		uLongf uncompressedSizeLong = uncompressedSize;
		const auto ret = uncompress(uncompressedBuffer->data(), &uncompressedSizeLong, buffer->data(), static_cast<uLong>(dataInfo.compressedSize));

		assert(ret == Z_OK);
		assert(uncompressedSize == uncompressedSizeLong);
	}
	else
	{
		std::memcpy(uncompressedBuffer->data(), buffer->data(), uncompressedSize);
	}

	return uncompressedBuffer;
}

std::optional<Bundle::EntryDebugInfo> Bundle::GetDebugInfo(const std::string &resourceName) const
{
	return GetDebugInfo(HashResourceName(resourceName));
}

std::optional<Bundle::EntryDebugInfo> Bundle::GetDebugInfo(uint32_t resourceID) const
{
	const auto it = m_debugInfoEntries.find(resourceID);
	if (it == m_debugInfoEntries.end())
		return {};
	
	return it->second;
}

std::optional<Bundle::ResourceType> Bundle::GetResourceType(const std::string &resourceName) const
{
	return GetResourceType(HashResourceName(resourceName));
}

std::optional<Bundle::ResourceType> Bundle::GetResourceType(uint32_t resourceID) const
{
	const auto it = m_entries.find(resourceID);
	if (it == m_entries.end())
		return {};

	return it->second.info.resourceType;
}

bool Bundle::AddResource(const std::string &resourceName, const EntryData &data, Bundle::ResourceType resourceType)
{
	return AddResource(HashResourceName(resourceName), data, resourceType);
}

bool Bundle::AddResource(uint32_t resourceID, const EntryData &data, Bundle::ResourceType resourceType)
{
	const auto it = m_entries.find(resourceID);
	if (it != m_entries.end() || data.dependencies.size() > std::numeric_limits<uint16_t>::max())
		return false;

	Entry &e = m_entries[resourceID];
	e.info.resourceType = resourceType;

	return ReplaceResource(resourceID, data);
}

bool Bundle::AddDebugInfo(const std::string &resourceName, const std::string &name, const std::string &type)
{
	return AddDebugInfo(HashResourceName(resourceName), name, type);
}

bool Bundle::AddDebugInfo(uint32_t resourceID, const std::string &name, const std::string &type)
{
	const auto it = m_debugInfoEntries.find(resourceID);
	if (it != m_debugInfoEntries.end())
		return false;

	EntryDebugInfo &debugInfo = m_debugInfoEntries[resourceID];
	debugInfo.name = name;
	debugInfo.typeName = type;

	return true;
}

bool Bundle::ReplaceResource(const std::string &resourceName, const EntryData &data)
{
	return ReplaceResource(HashResourceName(resourceName), data);
}

bool Bundle::ReplaceResource(uint32_t resourceID, const EntryData &data)
{
	const auto it = m_entries.find(resourceID);
	if (it == m_entries.end() || data.dependencies.size() > std::numeric_limits<uint16_t>::max())
		return false;

	Entry &e = it->second;

	e.info.checksum = 0;
	e.info.dependenciesOffset = 0;
	e.info.numberOfDependencies = 0;

	for (auto i = 0; i < 3; i++)
	{
		const auto &inDataInfo = data.fileBlockData[i];
		auto &outDataInfo = e.fileBlockData[i];

		if (inDataInfo == nullptr || inDataInfo->empty())
		{
			outDataInfo.data = nullptr;
			outDataInfo.uncompressedSize = 0;
			outDataInfo.compressedSize = 0;
			continue;
		}

		std::unique_ptr<std::vector<uint8_t>> inBuffer;
		std::unique_ptr<std::vector<uint8_t>> outBuffer;

		if (m_magicVersion == BND2 && i == 0 && !data.dependencies.empty())
		{
			binaryio::BinaryWriter writer;
			for (const auto &dependency : data.dependencies)
			{
				WriteDependency(writer, dependency);
				e.info.checksum &= dependency.resourceID;
			}
			const auto depSize = writer.GetSize();
			auto depStream = writer.GetStream();

			const auto inSize = inDataInfo->size();
			binaryio::Align(inSize, 16);
			inBuffer = std::make_unique<std::vector<uint8_t>>(inSize + depSize);
			inBuffer->assign(inDataInfo->begin(), inDataInfo->end());
			inBuffer->resize(inSize);
			inBuffer->insert(inBuffer->end(), std::istreambuf_iterator<char>(depStream), std::istreambuf_iterator<char>());

			e.info.dependenciesOffset = static_cast<uint32_t>(inSize);
			e.info.numberOfDependencies = static_cast<uint16_t>(data.dependencies.size());
		}
		else
		{
			inBuffer = std::make_unique<std::vector<uint8_t>>(inDataInfo->begin(), inDataInfo->end());
		}

		const auto uncompressedSize = static_cast<uint32_t>(inBuffer->size());

		if (m_flags & Compressed)
		{
			const auto compBufferSize = compressBound(static_cast<uLong>(inBuffer->size()));
			outBuffer = std::make_unique<std::vector<uint8_t>>(compBufferSize);
			uLongf actualSize = compBufferSize;
			const auto ret = compress2(outBuffer->data(), &actualSize, inBuffer->data(), static_cast<uLong>(inBuffer->size()), Z_BEST_COMPRESSION);

			if (ret != Z_OK)
			{
				assert(0);
				return false;
			}

			outBuffer->shrink_to_fit();
			outDataInfo.compressedSize = actualSize;
		}
		else
		{
			outBuffer = std::move(inBuffer);
			outDataInfo.compressedSize = 0;
		}

		outDataInfo.uncompressedSize = uncompressedSize;
		outDataInfo.data = std::move(outBuffer);
		outDataInfo.uncompressedAlignment = data.alignments[i];
	}

	return true;
}

void Bundle::WriteDependency(binaryio::BinaryWriter &writer, const Dependency &dependency)
{
	writer.Write<uint64_t>(dependency.resourceID);
	writer.Write(dependency.internalOffset);
	writer.Align(8);
}

std::vector<uint32_t> Bundle::ListResourceIDs() const
{
	std::vector<uint32_t> entries;
	for (const auto &e : m_entries)
	{
		entries.push_back(e.first);
	}
	return entries;
}

std::map<Bundle::ResourceType, std::vector<uint32_t>> Bundle::ListResourceIDsByType() const
{
	std::map<ResourceType, std::vector<uint32_t>> entriesByResourceType;
	for (const auto &e : m_entries)
	{
		entriesByResourceType[e.second.info.resourceType].push_back(e.first);
	}
	return entriesByResourceType;
}
