#include <libbndl/bundle.hpp>
#include "formats/bndl.hpp"
#include "formats/bnd2.hpp"
#include <binaryio/binaryreader.hpp>
#include <binaryio/binarywriter.hpp>
#include <algorithm>
#include <cassert>
#include <cstring>
#include <fstream>
#include <locale>
#include <zlib.h>

using namespace libbndl;

Bundle::Bundle() = default;

Bundle::Bundle(MagicVersion magicVersion, uint32_t revisionNumber, Platform platform, Flags flags)
{
	switch (magicVersion)
	{
	case MagicVersion::BNDL:
		m_impl = std::make_unique<Formats::BNDL>(revisionNumber, platform, flags);
		break;
	case MagicVersion::BND2:
		m_impl = std::make_unique<Formats::BND2>(revisionNumber, platform, flags);
		break;
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

	std::ofstream f(name, std::ios::out | std::ios::binary);
	f << writer.GetStream().rdbuf();
	f.close();

	return true;
}

Bundle::MagicVersion Bundle::GetMagicVersion() const
{
	return m_impl->GetMagicVersion();
}

uint32_t Bundle::GetRevisionNumber() const
{
	return m_impl->GetRevisionNumber();
}

Bundle::Platform Bundle::GetPlatform() const
{
	return m_impl->GetPlatform();
}

Bundle::Flags Bundle::GetFlags() const
{
	return m_impl->GetFlags();
}

uint32_t Bundle::HashResourceName(std::string resourceName) const
{
	std::transform(resourceName.begin(), resourceName.end(), resourceName.begin(), [](auto c) { return std::tolower(c, std::locale::classic()); });
	return crc32_z(0, reinterpret_cast<const Bytef *>(resourceName.c_str()), resourceName.length());
}

std::optional<Bundle::Resource> Bundle::GetResource(const std::string &resourceName) const
{
	return GetResource(HashResourceName(resourceName));
}

std::optional<Bundle::Resource> Bundle::GetResource(uint32_t resourceID) const
{
	return m_impl->GetResource(resourceID);
}

Bundle::Buffer Bundle::GetBinary(const std::string &resourceName, MemoryType fileBlock) const
{
	return GetBinary(HashResourceName(resourceName), fileBlock);
}

Bundle::Buffer Bundle::GetBinary(uint32_t resourceID, MemoryType fileBlock) const
{
	return m_impl->GetBinary(resourceID, fileBlock);
}

std::optional<Bundle::ResourceDebugInfo> Bundle::GetResourceDebugInfo(const std::string &resourceName) const
{
	return GetResourceDebugInfo(HashResourceName(resourceName));
}

std::optional<Bundle::ResourceDebugInfo> Bundle::GetResourceDebugInfo(uint32_t resourceID) const
{
	const auto &internalDebugInfo = m_impl->GetResourceDebugInfo(resourceID);
	if (!internalDebugInfo.has_value())
		return {};

	return Bundle::ResourceDebugInfo{ internalDebugInfo->name, internalDebugInfo->typeName };
}

std::optional<Bundle::ResourceType> Bundle::GetResourceType(const std::string &resourceName) const
{
	return GetResourceType(HashResourceName(resourceName));
}

std::optional<Bundle::ResourceType> Bundle::GetResourceType(uint32_t resourceID) const
{
	return m_impl->GetResourceType(resourceID);
}

bool Bundle::AddResource(const std::string &resourceName, const Resource &resource, Bundle::ResourceType resourceType)
{
	return AddResource(HashResourceName(resourceName), resource, resourceType);
}

bool Bundle::AddResource(uint32_t resourceID, const Resource &resource, Bundle::ResourceType resourceType)
{
	return m_impl->AddResource(resourceID, resource, resourceType);
}

bool Bundle::AddResourceDebugInfo(const std::string &resourceName, const std::string &name, const std::string &type)
{
	return AddResourceDebugInfo(HashResourceName(resourceName), name, type);
}

bool Bundle::AddResourceDebugInfo(uint32_t resourceID, const std::string &name, const std::string &type)
{
	return m_impl->AddResourceDebugInfo(resourceID, name, type);
}

bool Bundle::ReplaceResource(const std::string &resourceName, const Resource &resource)
{
	return ReplaceResource(HashResourceName(resourceName), resource);
}

bool Bundle::ReplaceResource(uint32_t resourceID, const Resource &resource)
{
	return m_impl->ReplaceResource(resourceID, resource);
}

std::vector<uint32_t> Bundle::GetResourceIDs() const
{
	return m_impl->GetResourceIDs();
}

std::map<Bundle::ResourceType, std::vector<uint32_t>> Bundle::GetResourceIDsByType() const
{
	return m_impl->GetResourceIDsByType();
}

std::vector<Bundle::MemoryType> Bundle::GetMemoryTypes() const
{
	return m_impl->GetMemoryTypes();
}
