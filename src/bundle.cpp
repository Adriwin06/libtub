#include <libbndl/bundle.hpp>
#include "formats/bndl.hpp"
#include "formats/bnd2.hpp"
#include <binaryio/binaryreader.hpp>
#include <binaryio/binarywriter.hpp>
#include <algorithm>
#include <fstream>
#include <locale>
#include <zlib.h>

using namespace libbndl;

Bundle::Bundle() = default;

Bundle::Bundle(MagicNumber magicNumber, uint16_t version, Platform platform, Flags flags)
{
	switch (magicNumber)
	{
	case MagicNumber::BNDL:
		m_impl = std::make_unique<Formats::BNDL>(version, platform, flags);
		break;
	case MagicNumber::BND2:
		m_impl = std::make_unique<Formats::BND2>(version, platform, flags);
		break;
	default:
		throw new std::invalid_argument("Invalid magic number");
	}
}

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

	auto reader = binaryio::BinaryReader(buffer, std::endian::little);

	// Check if it's a BNDL archive
	auto magic = reader.ReadString(4);
	if (magic == std::string("bndl"))
		m_impl = std::make_unique<Formats::BNDL>();
	else if (magic == std::string("bnd2"))
		m_impl = std::make_unique<Formats::BND2>();
	else
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

MagicNumber Bundle::GetMagicNumber() const
{
	return m_impl->GetMagicNumber();
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
	return GetMagicNumber() == MagicNumber::BNDL || GetVersion() <= 2;
}

bool Bundle::IsNeedForSpeedEra() const
{
	return GetMagicNumber() == MagicNumber::BND2 && GetVersion() >= 3;
}

ResourceID Bundle::HashResourceName(std::string resourceName) const
{
	std::transform(resourceName.begin(), resourceName.end(), resourceName.begin(), [](auto c) { return std::tolower(c, std::locale::classic()); });
	return ResourceID(crc32_z(0, reinterpret_cast<const Bytef *>(resourceName.c_str()), resourceName.length()));
}

std::optional<Resource> Bundle::GetResource(const std::string &resourceName, uint8_t streamIndex) const
{
	return GetResource(HashResourceName(resourceName), streamIndex);
}

std::optional<Resource> Bundle::GetResource(ResourceID resourceID, uint8_t streamIndex) const
{
	return m_impl->GetResource({ resourceID, streamIndex });
}

Buffer Bundle::GetBinary(const std::string &resourceName, MemoryType memoryType, uint8_t streamIndex) const
{
	return GetBinary(HashResourceName(resourceName), memoryType, streamIndex);
}

Buffer Bundle::GetBinary(ResourceID resourceID, MemoryType memoryType, uint8_t streamIndex) const
{
	return m_impl->GetBinary({ resourceID, streamIndex }, memoryType);
}

std::optional<ResourceDebugInfo> Bundle::GetResourceDebugInfo(const std::string &resourceName, uint8_t streamIndex) const
{
	return GetResourceDebugInfo(HashResourceName(resourceName), streamIndex);
}

std::optional<ResourceDebugInfo> Bundle::GetResourceDebugInfo(ResourceID resourceID, uint8_t streamIndex) const
{
	const auto &internalDebugInfo = m_impl->GetResourceDebugInfo({ resourceID, streamIndex });
	if (!internalDebugInfo)
		return {};

	return ResourceDebugInfo{ internalDebugInfo->name, internalDebugInfo->typeName };
}

std::optional<uint32_t> Bundle::GetResourceType(const std::string &resourceName, uint8_t streamIndex) const
{
	return GetResourceType(HashResourceName(resourceName), streamIndex);
}

std::optional<uint32_t> Bundle::GetResourceType(ResourceID resourceID, uint8_t streamIndex) const
{
	return m_impl->GetResourceType({ resourceID, streamIndex });
}

bool Bundle::AddResource(const std::string &resourceName, const Resource &resource, uint32_t resourceType, uint8_t streamIndex)
{
	return AddResource(HashResourceName(resourceName), resource, resourceType, streamIndex);
}

bool Bundle::AddResource(ResourceID resourceID, const Resource &resource, uint32_t resourceType, uint8_t streamIndex)
{
	return m_impl->AddResource({ resourceID, streamIndex }, resource, resourceType);
}

bool Bundle::AddResourceDebugInfo(const std::string &resourceName, const std::string &name, const std::string &type, uint8_t streamIndex)
{
	return AddResourceDebugInfo(HashResourceName(resourceName), name, type, streamIndex);
}

bool Bundle::AddResourceDebugInfo(ResourceID resourceID, const std::string &name, const std::string &type, uint8_t streamIndex)
{
	return m_impl->AddResourceDebugInfo({ resourceID, streamIndex }, name, type);
}

bool Bundle::ReplaceResource(const std::string &resourceName, const Resource &resource, uint8_t streamIndex)
{
	return ReplaceResource(HashResourceName(resourceName), resource, streamIndex);
}

bool Bundle::ReplaceResource(ResourceID resourceID, const Resource &resource, uint8_t streamIndex)
{
	return m_impl->ReplaceResource({ resourceID, streamIndex }, resource);
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

std::vector<MemoryType> Bundle::GetMemoryTypes() const
{
	return m_impl->GetMemoryTypes();
}
