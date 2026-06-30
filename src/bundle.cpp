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

bool Bundle::IsValid() const noexcept
{
	return m_impl != nullptr;
}

ErrorCode Bundle::GetLastErrorCode() const noexcept
{
	return m_lastErrorCode;
}

const std::string &Bundle::GetLastErrorMessage() const noexcept
{
	return m_lastErrorMessage;
}

void Bundle::ClearLastError() const
{
	m_lastErrorCode = ErrorCode::Success;
	m_lastErrorMessage.clear();
}

void Bundle::SetLastError(ErrorCode code, std::string message) const
{
	m_lastErrorCode = code;
	m_lastErrorMessage = std::move(message);
}

bool Bundle::Fail(ErrorCode code, std::string message) const
{
	SetLastError(code, std::move(message));
	return false;
}

bool Bundle::Load(const std::string &name)
{
	return Load(std::filesystem::path(name));
}

bool Bundle::Load(const std::filesystem::path &path)
{
	std::ifstream stream;

	stream.open(path, std::ios::in | std::ios::binary | std::ios::ate);

	// Check if archive exists
	if (stream.fail())
		return Fail(ErrorCode::InvalidPath, "Could not open bundle file.");

	const auto fileSize = stream.tellg();
	if (fileSize < 4)
		return Fail(ErrorCode::InvalidBundle, "Bundle file is too small.");

	stream.seekg(0, std::ios::beg);

	std::vector<uint8_t> buffer(fileSize);
	stream.read(reinterpret_cast<char *>(buffer.data()), fileSize);
	stream.close();
	if (stream.fail())
		return Fail(ErrorCode::IoError, "Could not read bundle file.");

	return Load(buffer);
}

bool Bundle::Load(std::span<const uint8_t> data)
{
	if (data.size() < 4)
		return Fail(ErrorCode::InvalidBundle, "Bundle data is too small.");

	std::vector<uint8_t> buffer(data.begin(), data.end());
	auto reader = binaryio::BinaryReader(buffer, std::endian::little);

	// Check if it's a BNDL archive
	m_impl = MakeBundleImplementation(reader.ReadString(4));
	if (!m_impl)
		return Fail(ErrorCode::UnsupportedFormat, "Unsupported bundle magic.");

	try
	{
		if (!m_impl->Load(reader))
			return Fail(ErrorCode::InvalidBundle, "Bundle parser rejected the input.");
	}
	catch (const std::exception &)
	{
		return Fail(ErrorCode::InvalidBundle, "Bundle parser threw while reading the input.");
	}
	catch (const std::out_of_range *error)
	{
		std::string message = "Bundle parser seeked outside the input.";
		if (error != nullptr)
		{
			message += " ";
			message += error->what();
			delete error;
		}
		return Fail(ErrorCode::InvalidBundle, std::move(message));
	}
	catch (...)
	{
		return Fail(ErrorCode::InvalidBundle, "Bundle parser failed while reading the input.");
	}

	ClearLastError();
	return true;
}

bool Bundle::Save(const std::string &name)
{
	return Save(std::filesystem::path(name));
}

bool Bundle::Save(const std::filesystem::path &path)
{
	if (!m_impl)
		return Fail(ErrorCode::InvalidState, "Cannot save an empty bundle.");

	auto writer = binaryio::BinaryWriter();

	if (!m_impl->Save(writer))
		return Fail(ErrorCode::ValidationFailed, "Bundle failed format validation while saving.");

	const auto stream = writer.GetStream();

	std::ofstream f(path, std::ios::out | std::ios::binary);
	f << stream.rdbuf();
	f.close();
	if (f.fail())
		return Fail(ErrorCode::IoError, "Could not write bundle file.");

	ClearLastError();
	return true;
}

std::vector<uint8_t> Bundle::SaveToMemory()
{
	if (!m_impl)
	{
		SetLastError(ErrorCode::InvalidState, "Cannot save an empty bundle.");
		return {};
	}

	auto writer = binaryio::BinaryWriter();
	if (!m_impl->Save(writer))
	{
		SetLastError(ErrorCode::ValidationFailed, "Bundle failed format validation while saving.");
		return {};
	}

	const auto stream = writer.GetStream();
	const auto view = stream.view();
	ClearLastError();
	return std::vector<uint8_t>(view.begin(), view.end());
}

Magic Bundle::GetMagic() const
{
	if (!m_impl)
		return static_cast<Magic>(0);

	return m_impl->GetMagic();
}

uint16_t Bundle::GetVersion() const
{
	if (!m_impl)
		return 0;

	return m_impl->GetVersion();
}

Platform Bundle::GetPlatform() const
{
	if (!m_impl)
		return static_cast<Platform>(0);

	return m_impl->GetPlatform();
}

Flags Bundle::GetFlags() const
{
	if (!m_impl)
		return {};

	return m_impl->GetFlags();
}

bool Bundle::IsBurnoutEra() const
{
	if (!m_impl)
		return false;

	return GetMagic() == Magic::Bndl || GetVersion() <= 2;
}

bool Bundle::IsNeedForSpeedEra() const
{
	if (!m_impl)
		return false;

	return GetMagic() == Magic::Bnd2 && GetVersion() >= 3;
}

std::optional<Resource> Bundle::GetResource(ResourceID resourceID, uint8_t streamIndex) const
{
	if (!m_impl)
	{
		SetLastError(ErrorCode::InvalidState, "Cannot read from an empty bundle.");
		return {};
	}

	auto resource = m_impl->GetResource({ resourceID, streamIndex });
	if (!resource)
	{
		SetLastError(ErrorCode::ResourceNotFound, "Resource was not found.");
		return {};
	}

	ClearLastError();
	return resource;
}

Buffer Bundle::GetBinary(ResourceID resourceID, MemoryType memoryType, uint8_t streamIndex) const
{
	if (!m_impl)
	{
		SetLastError(ErrorCode::InvalidState, "Cannot read from an empty bundle.");
		return {};
	}

	const auto memoryTypes = m_impl->GetMemoryTypes();
	if (std::find(memoryTypes.begin(), memoryTypes.end(), memoryType) == memoryTypes.end())
	{
		SetLastError(ErrorCode::OutOfRange, "Memory type is not valid for this bundle.");
		return {};
	}

	const auto resource = m_impl->GetResource({ resourceID, streamIndex });
	if (!resource)
	{
		SetLastError(ErrorCode::ResourceNotFound, "Resource was not found.");
		return {};
	}

	const auto &buffer = resource->GetBinary(memoryType);
	if (buffer == nullptr)
	{
		SetLastError(ErrorCode::ResourceNotFound, "Resource does not contain the requested memory block.");
		return {};
	}

	auto copy = std::make_unique_for_overwrite<uint8_t[]>(buffer.GetSize());
	if (buffer.GetSize() > 0)
		std::memcpy(copy.get(), buffer.GetData(), buffer.GetSize());

	ClearLastError();
	return { std::move(copy), buffer.GetSize(), buffer.GetAlignment() };
}

std::optional<ResourceDebugData> Bundle::GetResourceDebugData(ResourceID resourceID, uint8_t streamIndex) const
{
	if (!m_impl)
	{
		SetLastError(ErrorCode::InvalidState, "Cannot read from an empty bundle.");
		return {};
	}

	const auto &internalDebugData = m_impl->GetResourceDebugData({ resourceID, streamIndex });
	if (!internalDebugData)
	{
		SetLastError(ErrorCode::DebugDataNotFound, "Resource debug data was not found.");
		return {};
	}

	ClearLastError();
	return ResourceDebugData{ internalDebugData->name, internalDebugData->typeName };
}

std::optional<uint32_t> Bundle::GetResourceType(ResourceID resourceID, uint8_t streamIndex) const
{
	if (!m_impl)
	{
		SetLastError(ErrorCode::InvalidState, "Cannot read from an empty bundle.");
		return {};
	}

	const auto resourceType = m_impl->GetResourceType({ resourceID, streamIndex });
	if (!resourceType)
	{
		SetLastError(ErrorCode::ResourceNotFound, "Resource was not found.");
		return {};
	}

	ClearLastError();
	return resourceType;
}

bool Bundle::AddResource(ResourceID resourceID, const Resource &resource, uint8_t streamIndex)
{
	if (!m_impl)
		return Fail(ErrorCode::InvalidState, "Cannot add a resource to an empty bundle.");

	if (!m_impl->AddResource({ resourceID, streamIndex }, resource))
		return Fail(ErrorCode::ValidationFailed, "Resource could not be added to the bundle.");

	ClearLastError();
	return true;
}

bool Bundle::AddResourceDebugData(ResourceID resourceID, const ResourceDebugData &debugData, uint8_t streamIndex)
{
	if (!m_impl)
		return Fail(ErrorCode::InvalidState, "Cannot add debug data to an empty bundle.");

	if (!m_impl->AddResourceDebugData({ resourceID, streamIndex }, debugData.GetName(), debugData.GetTypeName()))
		return Fail(ErrorCode::ValidationFailed, "Resource debug data could not be added to the bundle.");

	ClearLastError();
	return true;
}

bool Bundle::ReplaceResource(ResourceID resourceID, const Resource &resource, uint8_t streamIndex)
{
	if (!m_impl)
		return Fail(ErrorCode::InvalidState, "Cannot replace a resource in an empty bundle.");

	if (!m_impl->ReplaceResource({ resourceID, streamIndex }, resource))
		return Fail(ErrorCode::ResourceNotFound, "Resource could not be replaced.");

	ClearLastError();
	return true;
}

uint32_t Bundle::GetResourceCount() const
{
	if (!m_impl)
		return 0;

	return m_impl->GetResourceCount();
}

std::vector<ResourceID> Bundle::GetResourceIDs() const
{
	if (!m_impl)
		return {};

	return m_impl->GetResourceIDs();
}

std::map<uint32_t, std::vector<ResourceID>> Bundle::GetResourceIDsByType() const
{
	if (!m_impl)
		return {};

	return m_impl->GetResourceIDsByType();
}

std::vector<uint8_t> Bundle::GetResourceStreamIndices(ResourceID resourceID) const
{
	if (!m_impl)
		return {};

	return m_impl->GetResourceStreamIndices(resourceID);
}

ResourceID Bundle::GetDefaultResourceID() const
{
	if (!m_impl)
		return ResourceID(0);

	return m_impl->GetDefaultResourceID();
}

int32_t Bundle::GetDefaultResourceStreamIndex() const
{
	if (!m_impl)
		return -1;

	return m_impl->GetDefaultResourceStreamIndex();
}

std::string Bundle::GetStreamName(uint8_t index) const
{
	if (!m_impl)
		return "";

	return m_impl->GetStreamName(index);
}

bool Bundle::SetDefaultResource(ResourceID resourceID, int32_t streamIndex)
{
	if (!m_impl || streamIndex < 0 || streamIndex > std::numeric_limits<uint8_t>::max())
		return Fail(ErrorCode::InvalidArgument, "Default resource stream index is invalid.");

	if (!m_impl->SetDefaultResource({ resourceID, static_cast<uint8_t>(streamIndex) }))
		return Fail(ErrorCode::ValidationFailed, "Default resource could not be set.");

	ClearLastError();
	return true;
}

bool Bundle::SetStreamName(uint8_t index, std::string_view name)
{
	if (!m_impl)
		return Fail(ErrorCode::InvalidState, "Cannot set a stream name on an empty bundle.");

	if (!m_impl->SetStreamName(index, std::string(name)))
		return Fail(ErrorCode::ValidationFailed, "Stream name could not be set.");

	ClearLastError();
	return true;
}

std::vector<MemoryType> Bundle::GetMemoryTypes() const
{
	if (!m_impl)
		return {};

	return m_impl->GetMemoryTypes();
}

std::vector<ResourceDescriptor> Bundle::DescribeResources() const
{
	if (!m_impl)
	{
		SetLastError(ErrorCode::InvalidState, "Cannot describe an empty bundle.");
		return {};
	}

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

	ClearLastError();
	return resources;
}
