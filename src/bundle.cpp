#include <libtub/bundle.hpp>
#include "formats/bndl.hpp"
#include "formats/bnd2.hpp"
#include <binaryio/binaryreader.hpp>
#include <binaryio/binarywriter.hpp>
#include <algorithm>
#include <cstring>
#include <fstream>
#include <limits>
#include <locale>
#include <set>
#include <stdexcept>
#include <tuple>
#include <zlib.h>

using namespace libtub;

namespace
{
	std::unique_ptr<Formats::Base> MakeBundleImplementation(const std::string &magic)
	{
		if (magic == "bndl")
			return std::make_unique<Formats::Bndl>();
		if (magic == "bnd2")
			return std::make_unique<Formats::Bnd2>();
		return {};
	}
}

ResourceID::ResourceID(const std::string &name) noexcept
{
	std::string transformedName = name;
	std::transform(transformedName.begin(), transformedName.end(), transformedName.begin(), [](auto c) { return std::tolower(c, std::locale::classic()); });
	m_id = crc32_z(0, reinterpret_cast<const Bytef *>(transformedName.c_str()), transformedName.length());
}


Bundle::Bundle() = default;

Bundle::Bundle(Magic magic, uint16_t version, Platform platform, Flags flags)
{
	switch (magic)
	{
	case Magic::Bndl:
		m_impl = std::make_unique<Formats::Bndl>(version, platform, flags);
		break;
	case Magic::Bnd2:
		m_impl = std::make_unique<Formats::Bnd2>(version, platform, flags);
		break;
	default:
		throw std::invalid_argument("Invalid magic number");
	}
}

Bundle::Bundle(Bundle &&other) noexcept = default;

Bundle &Bundle::operator=(Bundle &&other) noexcept = default;

Bundle::~Bundle() = default;

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

	std::vector<uint8_t> buffer(fileSize);
	stream.read(reinterpret_cast<char *>(buffer.data()), fileSize);
	stream.close();
	if (stream.fail())
		return false;

	return Load(buffer);
}

bool Bundle::Load(std::span<const uint8_t> data)
{
	if (data.size() < 4)
		return false;

	std::vector<uint8_t> buffer(data.begin(), data.end());
	auto reader = binaryio::BinaryReader(buffer, std::endian::little);

	// Check if it's a BNDL archive
	m_impl = MakeBundleImplementation(reader.ReadString(4));
	if (!m_impl)
		return false;

	return m_impl->Load(reader);
}

bool Bundle::Save(const std::string &name)
{
	auto writer = binaryio::BinaryWriter();

	if (!m_impl->Save(writer))
		return false;

	const auto stream = writer.GetStream();

	std::ofstream f(name, std::ios::out | std::ios::binary);
	f << stream.rdbuf();
	f.close();

	return true;
}

std::vector<uint8_t> Bundle::SaveToMemory()
{
	if (!m_impl)
		return {};

	auto writer = binaryio::BinaryWriter();
	if (!m_impl->Save(writer))
		return {};

	const auto stream = writer.GetStream();
	const auto view = stream.view();
	return std::vector<uint8_t>(view.begin(), view.end());
}

Magic Bundle::GetMagic() const
{
	return m_impl->GetMagic();
}

uint16_t Bundle::GetVersion() const
{
	return m_impl->GetVersion();
}

Platform Bundle::GetPlatform() const
{
	return m_impl->GetPlatform();
}

Flags Bundle::GetFlags() const
{
	return m_impl->GetFlags();
}

bool Bundle::IsBurnoutEra() const
{
	return GetMagic() == Magic::Bndl || GetVersion() <= 2;
}

bool Bundle::IsNeedForSpeedEra() const
{
	return GetMagic() == Magic::Bnd2 && GetVersion() >= 3;
}

std::optional<Resource> Bundle::GetResource(ResourceID resourceID, uint8_t streamIndex) const
{
	return m_impl->GetResource({ resourceID, streamIndex });
}

Buffer Bundle::GetBinary(ResourceID resourceID, MemoryType memoryType, uint8_t streamIndex) const
{
	if (!m_impl)
		return {};

	const auto resource = m_impl->GetResource({ resourceID, streamIndex });
	if (!resource)
		return {};

	const auto &buffer = resource->GetBinary(memoryType);
	if (buffer == nullptr)
		return {};

	auto copy = std::make_unique_for_overwrite<uint8_t[]>(buffer.GetSize());
	if (buffer.GetSize() > 0)
		std::memcpy(copy.get(), buffer.GetData(), buffer.GetSize());

	return { std::move(copy), buffer.GetSize(), buffer.GetAlignment() };
}

std::optional<ResourceDebugData> Bundle::GetResourceDebugData(ResourceID resourceID, uint8_t streamIndex) const
{
	const auto &internalDebugData = m_impl->GetResourceDebugData({ resourceID, streamIndex });
	if (!internalDebugData)
		return {};

	return ResourceDebugData{ internalDebugData->name, internalDebugData->typeName };
}

std::optional<uint32_t> Bundle::GetResourceType(ResourceID resourceID, uint8_t streamIndex) const
{
	return m_impl->GetResourceType({ resourceID, streamIndex });
}

bool Bundle::AddResource(ResourceID resourceID, const Resource &resource, uint8_t streamIndex)
{
	return m_impl->AddResource({ resourceID, streamIndex }, resource);
}

bool Bundle::AddResourceDebugData(ResourceID resourceID, const ResourceDebugData &debugData, uint8_t streamIndex)
{
	return m_impl->AddResourceDebugData({ resourceID, streamIndex }, debugData.GetName(), debugData.GetTypeName());
}

bool Bundle::ReplaceResource(ResourceID resourceID, const Resource &resource, uint8_t streamIndex)
{
	return m_impl->ReplaceResource({ resourceID, streamIndex }, resource);
}

uint32_t Bundle::GetResourceCount() const
{
	return m_impl->GetResourceCount();
}

std::vector<ResourceID> Bundle::GetResourceIDs() const
{
	return m_impl->GetResourceIDs();
}

std::map<uint32_t, std::vector<ResourceID>> Bundle::GetResourceIDsByType() const
{
	return m_impl->GetResourceIDsByType();
}

std::vector<uint8_t> Bundle::GetResourceStreamIndices(ResourceID resourceID) const
{
	return m_impl->GetResourceStreamIndices(resourceID);
}

ResourceID Bundle::GetDefaultResourceID() const
{
	return m_impl->GetDefaultResourceID();
}

int32_t Bundle::GetDefaultResourceStreamIndex() const
{
	return m_impl->GetDefaultResourceStreamIndex();
}

std::string Bundle::GetStreamName(uint8_t index) const
{
	return m_impl->GetStreamName(index);
}

bool Bundle::SetDefaultResource(ResourceID resourceID, int32_t streamIndex)
{
	if (!m_impl || streamIndex < 0 || streamIndex > std::numeric_limits<uint8_t>::max())
		return false;

	return m_impl->SetDefaultResource({ resourceID, static_cast<uint8_t>(streamIndex) });
}

bool Bundle::SetStreamName(uint8_t index, std::string_view name)
{
	if (!m_impl)
		return false;

	return m_impl->SetStreamName(index, std::string(name));
}

std::vector<MemoryType> Bundle::GetMemoryTypes() const
{
	return m_impl->GetMemoryTypes();
}

std::vector<ResourceDescriptor> Bundle::DescribeResources() const
{
	if (!m_impl)
		return {};

	std::set<ResourceID> uniqueIDs;
	for (const auto &resourceID : GetResourceIDs())
		uniqueIDs.insert(resourceID);

	std::vector<ResourceDescriptor> resources;
	for (const auto &resourceID : uniqueIDs)
	{
		for (const auto streamIndex : GetResourceStreamIndices(resourceID))
		{
			const auto resource = GetResource(resourceID, streamIndex);
			if (!resource)
				continue;

			ResourceDescriptor descriptor;
			descriptor.resourceID = resourceID;
			descriptor.streamIndex = streamIndex;
			descriptor.resourceType = resource->GetResourceType();
			descriptor.debugData = GetResourceDebugData(resourceID, streamIndex);
			descriptor.imports = resource->GetImports();

			for (const auto &memoryType : GetMemoryTypes())
			{
				const auto &buffer = resource->GetBinary(memoryType);
				if (buffer == nullptr)
					continue;

				descriptor.memoryBlocks.push_back({ memoryType, buffer.GetSize(), buffer.GetAlignment() });
			}

			resources.emplace_back(std::move(descriptor));
		}
	}

	std::sort(resources.begin(), resources.end(), [](const auto &lhs, const auto &rhs) {
		return std::tie(lhs.resourceType, lhs.resourceID, lhs.streamIndex) < std::tie(rhs.resourceType, rhs.resourceID, rhs.streamIndex);
	});

	return resources;
}
