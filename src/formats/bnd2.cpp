#include "bnd2.hpp"
#include <algorithm>
#include <limits>
#include <ranges>

using namespace libbndl;
using namespace libbndl::Formats;

bool BND2::Load(binaryio::BinaryReader &reader)
{
	m_version = reader.Read<uint32_t>();
	if ((m_version & 0xFFFF) == 0)
	{
		reader.SwapEndian();
		reader.Seek(-4, std::ios::cur);
		m_version = reader.Read<uint32_t>();
	}
	if (m_version != 2 && m_version != 3)
		return false;

	m_platform = reader.Read<Platform>();
	if (m_platform != Platform::PC && m_platform != Platform::Xbox360 && m_platform != Platform::PS3)
		return false;

	const auto rstOffset = reader.Read<uint32_t>();
	const auto numEntries = reader.Read<uint32_t>();

	const auto idBlockOffset = reader.Read<uint32_t>();
	const auto blocks = (m_version >= 3) ? 4 : 3;
	std::array<uint32_t, 4> fileBlockOffsets = {
		reader.Read<uint32_t>(),
		reader.Read<uint32_t>(),
		reader.Read<uint32_t>(),
		(blocks == 4) ? reader.Read<uint32_t>() : 0,
	};

	m_flags = Flags(reader.Read<uint32_t>());


	m_entries.clear();
	m_debugInfoEntries.clear();

	reader.Seek(idBlockOffset);
	for (auto i = 0U; i < numEntries; i++)
	{
		auto resourceID = ResourceID(reader.Read<uint64_t>());
		assert(resourceID != 0);

		auto &e = m_entries[resourceID];
		e.importHash = reader.Read<uint64_t>();

		for (auto j = 0; j < blocks; j++)
		{
			const auto uncompSize = reader.Read<uint32_t>();
			e.descriptors[j].uncompressedSize = uncompSize & ~(0xFU << 28);
			e.descriptors[j].uncompressedAlignment = 1 << (uncompSize >> 28);
		}

		for (auto j = 0; j < blocks; j++)
		{
			const auto onDiskSize = reader.Read<uint32_t>();
			e.descriptors[j].onDiskSize = onDiskSize & ~(0xFU << 28);
			e.descriptors[j].onDiskAlignment = 1 << (onDiskSize >> 28);
		}

		auto dataReader = reader.Copy();
		for (auto j = 0; j < blocks; j++)
		{
			dataReader.Seek(fileBlockOffsets[j] + reader.Read<uint32_t>()); // Read offset

			auto &descriptor = e.descriptors[j];

			if (descriptor.onDiskSize == 0)
			{
				descriptor.data = nullptr;
				continue;
			}

			descriptor.data = std::unique_ptr<uint8_t[]>(dataReader.Read<uint8_t *>(descriptor.onDiskSize));
		}

		e.importOffset = reader.Read<uint32_t>();
		e.resourceType = reader.Read<uint32_t>();
		e.importCount = reader.Read<uint16_t>();

		reader.Verify<uint8_t>(0); // flags
		e.streamIndex = reader.Read<uint8_t>();

		reader.Align(8);
	}

	if (m_flags & Flags::HasDebugData)
	{
		reader.Seek(rstOffset, std::ios::beg);
		ParseDebugData(reader.ReadString());
	}

	return true;
};

bool BND2::Save(binaryio::BinaryWriter &writer)
{
	if (m_version != 2 && m_version != 3)
		return false;

	// For version 2, only the first 4 flags are supported
	if (m_version == 2 && BitScanReverse(static_cast<uint32_t>(m_flags)) >= 4)
		return false;

	writer.Write("bnd2", 4);
	writer.SetEndian(m_platform != Platform::PC ? std::endian::big : std::endian::little);

	writer.Write<uint32_t>(m_version);
	writer.Write(m_platform);

	auto rstPointerPos = writer.GetOffset();
	writer.Seek(4, std::ios::cur); // write later

	writer.Write(static_cast<uint32_t>(m_entries.size()));

	const uint8_t blocks = (m_version >= 3) ? 4 : 3;

	auto idBlockPointerPos = writer.GetOffset();
	writer.Seek(4, std::ios::cur); // write later
	std::array<size_t, 4> fileBlockPointerPos;
	for (auto i = 0; i < blocks; i++)
	{
		fileBlockPointerPos[i] = writer.GetOffset();
		writer.Seek(4, std::ios::cur);
	}

	writer.Write<uint32_t>(m_flags);

	writer.Align(16);


	// RESOURCE STRING TABLE (version == 2)
	if (m_version == 2)
	{
		writer.VisitAndWrite<uint32_t>(rstPointerPos, writer.GetOffset32());
		if (m_flags & Flags::HasDebugData)
		{
			writer.Write(GenerateDebugData());
			writer.Align(16);
		}
	}

	// ID BLOCK
	const auto keys = std::views::keys(m_entries);
	std::vector<ResourceID> sortedKeys{ keys.begin(), keys.end() };
	if (m_version >= 3)
	{
		std::sort(sortedKeys.begin(), sortedKeys.end(), [this](const auto &a, const auto &b) {
			const auto &entryA = m_entries.at(a);
			const auto &entryB = m_entries.at(b);

			const auto idTypeA = a.GetIDType();
			const auto idTypeB = b.GetIDType();
			const auto indexA = (idTypeA == ResourceID::EIDType::GameChanger) ? a.GetIndex() : 0;
			const auto indexB = (idTypeB == ResourceID::EIDType::GameChanger) ? b.GetIndex() : 0;
			const auto typeIDA = (idTypeA == ResourceID::EIDType::GameChanger) ? a.GetTypeID() : 0;
			const auto typeIDB = (idTypeB == ResourceID::EIDType::GameChanger) ? b.GetTypeID() : 0;
			const auto idA = a.GetGameChangerID();
			const auto idB = b.GetGameChangerID();

			return std::tie(entryA.streamIndex, indexA, typeIDA, idTypeA, idA) < std::tie(entryB.streamIndex, indexB, typeIDB, idTypeB, idB);
		});
	}

	writer.VisitAndWrite<uint32_t>(idBlockPointerPos, writer.GetOffset32());
	auto entryDataPointerPos = std::vector<std::array<size_t, 4>>(m_entries.size());
	auto entryIter = sortedKeys.begin();
	for (auto i = 0U; i < m_entries.size(); i++)
	{
		writer.Write<uint64_t>(*entryIter);

		const auto &e = m_entries.at(*entryIter);

		writer.Write(e.importHash);

		for (uint8_t j = 0; j < blocks; j++)
		{
			const auto mappedBlock = MapFileBlockToLibBlock(j);
			if (mappedBlock.has_value())
				writer.Write(e.descriptors[*mappedBlock].uncompressedSize | (BitScanReverse(e.descriptors[*mappedBlock].uncompressedAlignment) << 28));
			else
				writer.Write<uint32_t>(0);
		}

		for (uint8_t j = 0; j < blocks; j++)
		{
			const auto mappedBlock = MapFileBlockToLibBlock(j);
			if (mappedBlock.has_value())
				writer.Write(e.descriptors[*mappedBlock].onDiskSize | (BitScanReverse(e.descriptors[*mappedBlock].onDiskAlignment) << 28));
			else
				writer.Write<uint32_t>(0);
		}

		for (uint8_t j = 0; j < blocks; j++)
		{
			const auto mappedBlock = MapFileBlockToLibBlock(j);
			if (mappedBlock.has_value())
				entryDataPointerPos[i][*mappedBlock] = writer.GetOffset();

			writer.Write<uint32_t>(0);
		}

		writer.Write(e.importOffset);
		writer.Write(e.resourceType);
		writer.Write(e.importCount);

		writer.Write<uint8_t>(0); // flags
		writer.Write<uint8_t>((m_flags & Flags::MultistreamBundle) ? e.streamIndex : 0);

		writer.Align(8);

		entryIter = std::next(entryIter);
	}

	// DATA BLOCK
	for (uint8_t i = 0; i < blocks; i++)
	{
		size_t alignment = (i == 0) ? 16 : 0x80;
		writer.Align(alignment);

		const auto blockStart = writer.GetOffset32();
		writer.VisitAndWrite<uint32_t>(fileBlockPointerPos[i], blockStart);

		const auto mappedBlock = MapFileBlockToLibBlock(i);
		if (!mappedBlock.has_value())
			break;

		entryIter = sortedKeys.begin();
		for (auto j = 0U; j < m_entries.size(); j++)
		{
			const auto &e = m_entries.at(*entryIter);

			const auto &descriptor = e.descriptors[*mappedBlock];

			if (descriptor.onDiskSize > 0)
			{
				writer.Align(alignment);
				writer.VisitAndWrite<uint32_t>(entryDataPointerPos[j][*mappedBlock], writer.GetOffset32() - blockStart);
				writer.Write(descriptor.data.get(), descriptor.onDiskSize);
			}

			entryIter = std::next(entryIter);
		}
	}

	// RESOURCE STRING TABLE (version >= 3)
	if (m_version >= 3)
	{
		writer.VisitAndWrite<uint32_t>(rstPointerPos, writer.GetOffset32());
		if (m_flags & Flags::HasDebugData)
		{
			writer.Write(GenerateDebugData());
		}
	}

	return true;
}

std::optional<Resource> BND2::GetResource(ResourceID resourceID) const
{
	const auto it = m_entries.find(resourceID);
	if (it == m_entries.end())
		return {};

	std::array<Buffer, 4> buffers;
	for (const auto &memoryType : GetMemoryTypes())
		buffers[LIBBNDL_TO_UNDERLYING(memoryType)] = GetBinary(resourceID, memoryType);

	std::vector<Import> imports;
	const auto numImports = it->second.importCount;
	if (numImports > 0)
	{
		imports.reserve(numImports);

		binaryio::BinaryReader reader(buffers[0], m_platform != Platform::PC ? std::endian::big : std::endian::little);
		reader.Seek(it->second.importOffset);
		for (auto i = 0U; i < numImports; i++)
		{
			const auto &importEntry = ReadImport(reader);
			imports.emplace_back(importEntry.resourceID, importEntry.offset);
		}

		auto buffer = std::make_unique_for_overwrite<uint8_t[]>(buffers[0].GetSize());
		std::memcpy(buffer.get(), buffers[0].GetData(), buffers[0].GetSize());
		buffers[0] = { std::move(buffer), buffers[0].GetSize(), buffers[0].GetAlignment() };
	}

	return Resource{ std::move(buffers), std::move(imports), it->second.streamIndex };
}

std::vector<MemoryType> BND2::GetMemoryTypes() const
{
	auto types = Base::GetMemoryTypes();

	if (m_version >= 3)
	{
		switch (m_platform)
		{
		case Platform::Xbox360:
		case Platform::PS3:
			types.reserve(types.capacity() + 1);
			types.emplace_back(MemoryType::Disposable);
			break;
		}
	}

	return types;
}

std::optional<uint8_t> BND2::MapFileBlockToLibBlock(uint8_t block) const
{
	std::optional<MemoryType> mappedType = {};
	switch (block)
	{
	case 0:
		mappedType = MemoryType::MainMemory;
		break;
	case 1:
		switch (m_platform)
		{
		case Platform::PS3:
			mappedType = MemoryType::GraphicsSystem;
			break;
		case Platform::Xbox360:
			mappedType = MemoryType::Physical;
			break;
		case Platform::PC:
			mappedType = MemoryType::Disposable;
			break;
		}
		break;
	case 2:
		switch (m_platform)
		{
		case Platform::PS3:
			mappedType = MemoryType::GraphicsLocal;
			break;
		case Platform::Xbox360:
			if (m_version >= 3)
				mappedType = MemoryType::Disposable;
			break;
		}
		break;
	case 3:
		if (m_version >= 3 && m_platform == Platform::PS3)
			mappedType = MemoryType::Disposable;
		break;
	}

	if (mappedType)
		return LIBBNDL_TO_UNDERLYING(*mappedType);

	return {};
}
