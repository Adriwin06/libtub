#include "bndl.hpp"
#include <cassert>
#include <cstring>

using namespace libtub;
using namespace libtub::Formats;

namespace
{
	constexpr uint64_t kResourceStringTableID = 0xC039284A;
}

ErrorCode Bndl::Load(binaryio::BinaryReader &reader)
{
	auto version = reader.Read<uint32_t>();
	if ((version & 0xFFFF) == 0)
	{
		reader.SwapEndian();
		reader.Seek(-4, std::ios::cur);
		version = reader.Read<uint32_t>();
	}
	m_version = static_cast<uint16_t>(version);
	if (version < 3 || version > 5)
		return ErrorCode::UnsupportedVersion;

	std::optional<Platform> detectedPlatform;
	auto platformReader = reader;
	for (const auto offset : { 0x4C, 0x58, 0x64 })
	{
		platformReader.Seek(offset);
		const auto platform = static_cast<Platform>(platformReader.Read<uint32_t>());
		if (platform == Platform::PC || platform == Platform::Xbox360 || platform == Platform::PS3)
		{
			detectedPlatform = platform;
			break;
		}
	}
	if (!detectedPlatform.has_value())
		return ErrorCode::UnsupportedPlatform;
	m_platform = *detectedPlatform;

	const auto numEntries = reader.Read<uint32_t>();

	uint8_t blocks = 4;
	if (m_platform == Platform::Xbox360)
		blocks = 5;
	else if (m_platform == Platform::PS3)
		blocks = 6;
	std::array<uint32_t, 6> dataBlockSizes;
	for (uint8_t i = 0; i < blocks; i++)
	{
		dataBlockSizes[i] = reader.Read<uint32_t>();
		reader.Skip<uint32_t>(); // Alignment
	}

	reader.Seek(static_cast<std::streamoff>(0x4) * blocks, std::ios::cur); // memory address stuff

	const auto idListOffset = reader.Read<uint32_t>();
	const auto idTableOffset = reader.Read<uint32_t>();
	reader.Skip<uint32_t>(); // import block
	reader.Skip<uint32_t>(); // start of data block

	if (reader.Read<uint32_t>() != static_cast<uint32_t>(m_platform))
		return ErrorCode::InvalidBundle;

	auto compressed = 0U;
	auto uncompInfoOffset = 0U;

	if (m_version >= 4)
	{
		compressed = reader.Read<uint32_t>(); // flags but compression is the only valid one
		if (compressed)
			m_flags = Flags::Compressed;
		else
			m_flags = static_cast<Flags>(0);

		reader.Skip<uint32_t>(); // number of compressed resources
		uncompInfoOffset = reader.Read<uint32_t>();
	}

	if (m_version >= 5)
	{
		reader.Skip<uint32_t>(); // main memory alignment
		reader.Skip<uint32_t>(); // graphics memory alignment
	}

	m_entries.clear();
	m_debugDataEntries.clear();
	m_imports.clear();

	reader.Seek(idListOffset);
	std::vector<ResourceID> resourceIDs;
	resourceIDs.reserve(numEntries);
	for (auto i = 0U; i < numEntries; i++)
		resourceIDs.emplace_back(reader.Read<uint64_t>());

	reader.Seek(idTableOffset);
	for (const auto resourceID : resourceIDs)
	{
		auto &e = m_entries[{ resourceID, static_cast<uint8_t>(0) }];

		reader.Skip<uint32_t>(); // runtime-only memory variable
		e.importOffset = reader.Read<uint32_t>();
		e.resourceType = reader.Read<uint32_t>();

		for (uint8_t j = 0; j < blocks; j++)
		{
			const auto mappedBlock = MapFileBlockToLibBlock(j);
			if (!mappedBlock)
			{
				reader.Skip<uint32_t>(); // size
				reader.Skip<uint32_t>(); // alignment
			}
			else
			{
				e.descriptors[*mappedBlock].onDiskSize = reader.Read<uint32_t>();
				e.descriptors[*mappedBlock].onDiskAlignment = reader.Read<uint32_t>();

				if (!compressed)
				{
					e.descriptors[*mappedBlock].uncompressedSize = e.descriptors[*mappedBlock].onDiskSize;
					e.descriptors[*mappedBlock].uncompressedAlignment = e.descriptors[*mappedBlock].onDiskAlignment;
				}
			}
		}

		auto dataReader = reader;
		uint32_t dataBlockStartOffset = 0;
		for (uint8_t j = 0; j < blocks; j++)
		{
			if (j > 0)
				dataBlockStartOffset += dataBlockSizes[j - 1];

			const auto readOffset = reader.Read<uint32_t>() + dataBlockStartOffset;
			reader.Skip<uint32_t>(); // 1

			const auto mappedBlock = MapFileBlockToLibBlock(j);
			if (!mappedBlock)
			{
				continue;
			}

			auto &descriptor = e.descriptors[*mappedBlock];

			if (descriptor.onDiskSize == 0)
			{
				descriptor.data = nullptr;
				continue;
			}

			dataReader.Seek(readOffset); // Read offset

			descriptor.data = std::unique_ptr<uint8_t[]>(dataReader.Read<uint8_t *>(descriptor.onDiskSize));
		}

		reader.Seek(static_cast<std::streamoff>(0x4) * blocks, std::ios::cur); // memory address stuff
	}

	if (compressed)
	{
		reader.Seek(uncompInfoOffset);
		for (const auto resourceID : resourceIDs)
		{
			auto &e = m_entries[{ resourceID, static_cast<uint8_t>(0) }];

			for (uint8_t j = 0; j < blocks; j++)
			{
				const auto mappedBlock = MapFileBlockToLibBlock(j);
				if (!mappedBlock)
				{
					reader.Skip<uint32_t>(); // size
					reader.Skip<uint32_t>(); // alignment
				}
				else
				{
					e.descriptors[*mappedBlock].uncompressedSize = reader.Read<uint32_t>();
					e.descriptors[*mappedBlock].uncompressedAlignment = reader.Read<uint32_t>();
				}
			}
		}
	}

	for (const auto resourceID : resourceIDs)
	{
		auto &e = m_entries[{ resourceID, static_cast<uint8_t>(0) }];
		const auto depOffset = e.importOffset;
		if (depOffset == 0)
			continue;

		reader.Seek(depOffset);
		e.importCount = static_cast<uint16_t>(reader.Read<uint32_t>());
		reader.Skip<uint32_t>(); // padding
		for (auto i = 0U; i < e.importCount; i++)
			m_imports[resourceID].emplace_back(ReadImport(reader));
	}

	// Keep an undecodable string table as an ordinary resource, so Save writes its bytes back unchanged.
	const auto rstFile = DecodeBinary({ ResourceID(kResourceStringTableID), static_cast<uint8_t>(0) }, MemoryType::MainMemory);
	if (!rstFile || *rstFile == nullptr)
		return ErrorCode::Success;

	m_flags |= Flags::HasDebugData;

	auto rstReader = binaryio::BinaryReader(*rstFile);

	const auto strLen = rstReader.Read<uint32_t>();
	auto rstXML = rstReader.ReadString(strLen);

	// Cover Criterion's broken XML writer.
	if (rstXML.starts_with("</ResourceStringTable>"))
		rstXML.erase(1, 1);
	const auto pos = rstXML.find("</ResourceStringTable>\n\t");
	if (pos != std::string::npos)
		rstXML.erase(pos, 23);

	ParseDebugData(rstXML);

	m_entries.erase({ ResourceID(kResourceStringTableID), static_cast<uint8_t>(0) });

	return ErrorCode::Success;
};

ErrorCode Bndl::Save(binaryio::BinaryWriter &writer)
{
	if (m_version < 3 || m_version > 5)
		return ErrorCode::UnsupportedVersion;

	// Only one flag is supported. Allow HasDebugData since we simulate it ourselves here.
	if (BitScanReverse(static_cast<uint32_t>(m_flags & ~Flags::HasDebugData)) >= 1)
		return ErrorCode::UnsupportedFlags;

	if (m_version <= 3 && (m_flags & Flags::Compressed))
		return ErrorCode::UnsupportedFlags; // Invalid combination

	if (!IsValidPlatform())
		return ErrorCode::UnsupportedPlatform;

	writer.SetEndian(GetPlatformEndian());

	writer.Write("bndl", 4);
	writer.Write<uint32_t>(m_version);

	const bool writeDebugData = !m_debugDataEntries.empty() && !(m_flags & Flags::Compressed); // TODO: is the compressed check accurate?
	auto entryCount = static_cast<uint32_t>(m_entries.size());
	if (writeDebugData)
		entryCount++;

	writer.Write<uint32_t>(entryCount);

	uint8_t blocks = 4;
	if (m_platform == Platform::Xbox360)
		blocks = 5;
	else if (m_platform == Platform::PS3)
		blocks = 6;

	std::array<size_t, 4> dataBlockDescriptorsPos;
	for (uint8_t i = 0; i < blocks; i++)
	{
		const auto mappedBlock = MapFileBlockToLibBlock(i);
		if (mappedBlock)
			dataBlockDescriptorsPos[*mappedBlock] = writer.GetOffset();
		writer.Write<uint32_t>(0); // size
		writer.Write<uint32_t>(1); // alignment
	}

	for (auto i = 0; i < blocks; i++)
	{
		writer.Write<uint32_t>(0); // memory addresses
	}

	auto idListPointerPos = writer.GetOffset();
	writer.Seek(4, std::ios::cur);
	auto idTablePointerPos = writer.GetOffset();
	writer.Seek(4, std::ios::cur);
	auto importBlockPointerPos = writer.GetOffset();
	writer.Seek(4, std::ios::cur);
	auto dataBlockPointerPos = writer.GetOffset();
	writer.Seek(4, std::ios::cur);

	writer.Write<uint32_t>(m_platform);

	size_t uncompInfoBlockPointerPos = 0;

	if (m_version >= 4)
	{
		writer.Write<uint32_t>(m_flags & ~Flags::HasDebugData);
		writer.Write<uint32_t>((m_flags & Flags::Compressed) ? entryCount : 0);
		uncompInfoBlockPointerPos = writer.GetOffset();
		writer.Write<uint32_t>(0); // will write later, but only if needed
	}

	if (m_version >= 5)
	{
		writer.Write<uint32_t>(0); // Main memory alignment. Setting this to 0 so we don't need to deal with memory addresses.
		writer.Write<uint32_t>(0); // Graphics memory alignment.
	}

	writer.Align(0x10);

	// Prepare ResourceStringTable
	const ResourceKey debugDataKey{ ResourceID(kResourceStringTableID), static_cast<uint8_t>(0) };
	if (writeDebugData)
	{
		const auto outStr = GenerateDebugData();

		auto debugDataWriter = binaryio::BinaryWriter();
		debugDataWriter.Write(static_cast<uint32_t>(outStr.size()));
		// A std::string argument would pick binaryio's element-wise Write(T) overload instead.
		debugDataWriter.Write(std::string_view(outStr));

		const auto stream = debugDataWriter.GetStream();
		const auto data = stream.view();
		const auto dataSize = data.size();

		auto &e = m_entries[debugDataKey];
		e = ResourceEntry{};
		e.resourceType = ResourceType::Burnout::TextFile;

		e.descriptors[0].data = std::make_unique_for_overwrite<uint8_t[]>(dataSize);
		std::memcpy(e.descriptors[0].data.get(), data.data(), dataSize);

		// Save only writes debug data into uncompressed bundles, so the on-disk size and alignment equal the uncompressed ones.
		e.descriptors[0].uncompressedSize = static_cast<uint32_t>(dataSize);
		e.descriptors[0].uncompressedAlignment = 4;
		e.descriptors[0].onDiskSize = e.descriptors[0].uncompressedSize;
		e.descriptors[0].onDiskAlignment = e.descriptors[0].uncompressedAlignment;
	}

	// The loader pairs the ID list with the ID table by position, so the tables below share one entry order.
	// The string table goes last wherever its ID would sort.
	std::vector<const std::pair<const ResourceKey, ResourceEntry> *> orderedEntries;
	orderedEntries.reserve(m_entries.size());
	for (const auto &entry : m_entries)
	{
		if (!writeDebugData || entry.first != debugDataKey)
			orderedEntries.push_back(&entry);
	}
	if (writeDebugData)
		orderedEntries.push_back(&*m_entries.find(debugDataKey));

	// ID LIST
	writer.VisitAndWrite<uint32_t>(idListPointerPos, writer.GetOffset32());
	for (const auto *entry : orderedEntries)
	{
		writer.Write<uint64_t>(entry->first.first);
	}

	// ID TABLE
	writer.VisitAndWrite<uint32_t>(idTablePointerPos, writer.GetOffset32());

	struct FilePointerPosHelper
	{
		size_t importPointerPos;
		std::array<size_t, 4> dataBlockPointerPos;
	};
	std::map<ResourceID, FilePointerPosHelper> filePointerPosMap;
	for (const auto *entry : orderedEntries)
	{
		writer.Write<uint32_t>(0); // Ignore

		auto &posHelper = filePointerPosMap[entry->first.first];

		posHelper.importPointerPos = writer.GetOffset();
		writer.Write<uint32_t>(0);

		writer.Write(entry->second.resourceType);

		for (uint8_t i = 0; i < blocks; i++)
		{
			const auto mappedBlock = MapFileBlockToLibBlock(i);
			if (!mappedBlock)
			{
				writer.Write<uint32_t>(0); // size
				writer.Write<uint32_t>(1); // alignment
			}
			else
			{
				const auto &descriptor = entry->second.descriptors[*mappedBlock];
				writer.Write<uint32_t>(descriptor.onDiskSize);
				writer.Write<uint32_t>((descriptor.onDiskSize == 0) ? 1 : descriptor.onDiskAlignment);
			}
		}

		for (uint8_t i = 0; i < blocks; i++)
		{
			const auto mappedBlock = MapFileBlockToLibBlock(i);
			if (mappedBlock)
				posHelper.dataBlockPointerPos[*mappedBlock] = writer.GetOffset();

			writer.Write<uint32_t>(0);
			writer.Write<uint32_t>(1); // constant
		}

		// Memory stuff
		for (auto i = 0; i < blocks; i++)
			writer.Write<uint32_t>(0);
	}

	// UNCOMPRESSED SIZE INFO
	if (m_flags & Flags::Compressed)
	{
		writer.VisitAndWrite<uint32_t>(uncompInfoBlockPointerPos, writer.GetOffset32());
		for (const auto *entry : orderedEntries)
		{
			for (uint8_t i = 0; i < blocks; i++)
			{
				const auto mappedBlock = MapFileBlockToLibBlock(i);
				if (!mappedBlock)
				{
					writer.Write<uint32_t>(0); // size
					writer.Write<uint32_t>(1); // alignment
				}
				else
				{
					const auto &descriptor = entry->second.descriptors[*mappedBlock];
					writer.Write<uint32_t>(descriptor.uncompressedSize);
					writer.Write<uint32_t>((descriptor.uncompressedSize == 0) ? 1 : descriptor.uncompressedAlignment);
				}
			}
		}
	}

	// IMPORTS
	writer.VisitAndWrite<uint32_t>(importBlockPointerPos, writer.GetOffset32());
	for (const auto *entry : orderedEntries)
	{
		const auto importsIt = m_imports.find(entry->first.first);
		if (importsIt == m_imports.end() || importsIt->second.empty())
			continue;

		const auto &imports = importsIt->second;
		writer.VisitAndWrite<uint32_t>(filePointerPosMap.at(entry->first.first).importPointerPos, writer.GetOffset32());

		writer.Write(static_cast<uint32_t>(imports.size()));
		writer.Write<uint32_t>(0); // padding
		for (const auto &import : imports)
			WriteImport(writer, Import(import.resourceID, import.offset, import.type));
	}

	// DATA
	writer.VisitAndWrite<uint32_t>(dataBlockPointerPos, writer.GetOffset32());
	uint32_t blockStartOffset = 0;
	for (uint8_t i = 0; i < blocks; i++)
	{
		const auto mappedBlock = MapFileBlockToLibBlock(i);
		if (!mappedBlock)
			continue;

		for (const auto *entry : orderedEntries)
		{
			const auto &e = entry->second;

			const auto &descriptor = e.descriptors[*mappedBlock];

			if (descriptor.onDiskSize > 0)
			{
				writer.VisitAndWrite<uint32_t>(filePointerPosMap.at(entry->first.first).dataBlockPointerPos[*mappedBlock], writer.GetOffset32() - blockStartOffset);
				writer.Write(descriptor.data.get(), descriptor.onDiskSize);
			}
		}

		const auto size = writer.GetOffset32() - blockStartOffset;
		auto dataBlockSizePos = dataBlockDescriptorsPos[*mappedBlock];
		auto dataBlockAlignmentPos = dataBlockSizePos + sizeof(uint32_t);
		writer.VisitAndWrite<uint32_t>(dataBlockSizePos, size);
		writer.VisitAndWrite<uint32_t>(dataBlockAlignmentPos, (size == 0) ? 1 : ((*mappedBlock >= 1) ? 4096 : 1024)); // TODO: This changes and I don't know the pattern.
		blockStartOffset = writer.GetOffset32();
	}

	if (writeDebugData)
		m_entries.erase(debugDataKey);

	return ErrorCode::Success;
}

void Bndl::StoreSeparateImports(ResourceKey resourceKey, const std::vector<Import> &imports)
{
	m_entries.at(resourceKey).importCount = static_cast<uint16_t>(imports.size());

	if (imports.empty())
	{
		m_imports.erase(resourceKey.first);
		return;
	}

	auto &importEntries = m_imports[resourceKey.first];
	importEntries.clear();
	importEntries.reserve(imports.size());
	for (const auto &import : imports)
		importEntries.push_back({ import.GetResourceID(), import.GetOffset(), import.GetImportType() });
}

std::optional<uint8_t> Bndl::MapFileBlockToLibBlock(uint8_t block) const
{
	std::optional<MemoryType> mappedType = {};
	switch (block)
	{
	case 0:
		mappedType = MemoryType::MainMemory;
		break;
	case 1:
		mappedType = MemoryType::Disposable;
		break;
	case 2:
		if (m_platform == Platform::Xbox360)
			mappedType = MemoryType::Physical;
		break;
	case 3:
		break;
	case 4:
		if (m_platform == Platform::PS3)
			mappedType = MemoryType::GraphicsSystem;
		break;
	case 5:
		if (m_platform == Platform::PS3)
			mappedType = MemoryType::GraphicsLocal;
		break;
	default:
		assert(false);
		break;
	}

	if (mappedType)
		return LIBTUB_TO_UNDERLYING(*mappedType);

	return {};
}

std::optional<Resource> Bndl::GetResource(ResourceKey resourceKey) const
{
	const auto it = m_entries.find(resourceKey);
	if (it == m_entries.end())
		return {};

	std::array<Buffer, 4> buffers;
	for (const auto &memoryType : GetMemoryTypes())
	{
		auto buffer = DecodeBinary(resourceKey, memoryType);
		if (!buffer)
			return {};

		buffers[LIBTUB_TO_UNDERLYING(memoryType)] = std::move(*buffer);
	}

	std::vector<Import> imports;
	const auto importsIt = m_imports.find(resourceKey.first);
	if (it->second.importCount > 0 && importsIt != m_imports.end())
	{
		for (const auto &importEntry : importsIt->second)
			imports.emplace_back(importEntry.resourceID, importEntry.offset, importEntry.type);
	}

	return Resource{ std::move(buffers), std::move(imports), it->second.resourceType };
}
