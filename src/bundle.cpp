#include <libtub/bundle.hpp>
#include "formats/bndl.hpp"
#include "formats/bnd2.hpp"
#include <binaryio/binaryreader.hpp>
#include <binaryio/binarywriter.hpp>
#include <algorithm>
#include <fstream>
#include <limits>
#include <locale>
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

	std::string DescribeFormatFailure(ErrorCode code, bool saving)
	{
		switch (code)
		{
		case ErrorCode::UnsupportedVersion:
			return "Bundle version is not supported by this format.";
		case ErrorCode::UnsupportedPlatform:
			return "Bundle platform is not supported by this format version.";
		case ErrorCode::UnsupportedFlags:
			return "Bundle flags are not supported by this format version.";
		default:
			return saving ? "Bundle failed format validation while saving." : "Bundle parser rejected the input.";
		}
	}

	// Tells a missing resource apart from one whose stored data can't be decoded.
	std::pair<ErrorCode, std::string> DescribeResourceReadFailure(const Formats::Base &impl, Formats::ResourceKey resourceKey)
	{
		if (!impl.HasResource(resourceKey))
			return { ErrorCode::ResourceNotFound, "Resource was not found." };

		return { ErrorCode::DecompressionFailed, "Resource data could not be decoded; the bundle may be corrupt." };
	}

	// Returns an empty message on success. binaryio reports overflow by throwing std::out_of_range.
	std::string SaveImplementation(Formats::Base &impl, binaryio::BinaryWriter &writer, ErrorCode &code)
	{
		code = ErrorCode::ValidationFailed;
		try
		{
			if (const auto result = impl.Save(writer); result != ErrorCode::Success)
			{
				code = result;
				return DescribeFormatFailure(result, true);
			}
		}
		catch (const std::out_of_range &error)
		{
			code = ErrorCode::OutOfRange;
			return std::string("Bundle writer exceeded its addressable range. ") + error.what();
		}
		catch (const std::exception &)
		{
			code = ErrorCode::GenericFailure;
			return "Bundle writer threw while saving.";
		}

		code = ErrorCode::Success;
		return {};
	}
}

ResourceID::ResourceID(std::string name) noexcept
{
	std::ranges::transform(name, name.begin(), [](auto c) { return std::tolower(c, std::locale::classic()); });
	m_id = crc32_z(0, reinterpret_cast<const Bytef *>(name.c_str()), name.length());
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

	// Parse into a fresh implementation so a failed load leaves the current bundle untouched.
	auto impl = MakeBundleImplementation(reader.ReadString(4));
	if (!impl)
		return Fail(ErrorCode::UnsupportedFormat, "Unsupported bundle magic.");

	try
	{
		if (const auto result = impl->Load(reader); result != ErrorCode::Success)
			return Fail(result, DescribeFormatFailure(result, false));
	}
	catch (const std::out_of_range &error)
	{
		return Fail(ErrorCode::InvalidBundle, std::string("Bundle parser seeked outside the input. ") + error.what());
	}
	catch (const std::exception &)
	{
		return Fail(ErrorCode::InvalidBundle, "Bundle parser threw while reading the input.");
	}
	catch (...)
	{
		return Fail(ErrorCode::InvalidBundle, "Bundle parser failed while reading the input.");
	}

	m_impl = std::move(impl);
	ClearLastError();
	return true;
}

bool Bundle::Save(const std::filesystem::path &path)
{
	if (!m_impl)
		return Fail(ErrorCode::InvalidState, "Cannot save an empty bundle.");

	auto writer = binaryio::BinaryWriter();
	auto code = ErrorCode::Success;
	if (auto message = SaveImplementation(*m_impl, writer, code); code != ErrorCode::Success)
		return Fail(code, std::move(message));

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
	auto code = ErrorCode::Success;
	if (auto message = SaveImplementation(*m_impl, writer, code); code != ErrorCode::Success)
	{
		SetLastError(code, std::move(message));
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

	const Formats::ResourceKey resourceKey{ resourceID, streamIndex };
	auto resource = m_impl->GetResource(resourceKey);
	if (!resource)
	{
		auto [code, message] = DescribeResourceReadFailure(*m_impl, resourceKey);
		SetLastError(code, std::move(message));
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

	// Decode only the requested block rather than the whole resource.
	const Formats::ResourceKey resourceKey{ resourceID, streamIndex };
	auto buffer = m_impl->GetResourceBinary(resourceKey, memoryType);
	if (!buffer)
	{
		auto [code, message] = DescribeResourceReadFailure(*m_impl, resourceKey);
		SetLastError(code, std::move(message));
		return {};
	}

	if (*buffer == nullptr)
	{
		SetLastError(ErrorCode::ResourceNotFound, "Resource does not contain the requested memory block.");
		return {};
	}

	ClearLastError();
	return std::move(*buffer);
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
	if (!m_impl)
		return Fail(ErrorCode::InvalidState, "Cannot set a default resource on an empty bundle.");

	if (streamIndex < 0 || streamIndex > std::numeric_limits<uint8_t>::max())
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

	const auto memoryTypes = m_impl->GetMemoryTypes();
	const auto resourceKeys = m_impl->GetResourceKeys();

	std::vector<ResourceDescriptor> resources;
	resources.reserve(resourceKeys.size());
	size_t unreadableResources = 0;
	for (const auto &resourceKey : resourceKeys)
	{
		const auto resource = m_impl->GetResource(resourceKey);
		if (!resource)
		{
			++unreadableResources;
			continue;
		}

		ResourceDescriptor descriptor;
		descriptor.resourceID = resourceKey.first;
		descriptor.streamIndex = resourceKey.second;
		descriptor.resourceType = resource->GetResourceType();
		if (const auto debugData = m_impl->GetResourceDebugData(resourceKey))
			descriptor.debugData = ResourceDebugData{ debugData->name, debugData->typeName };
		descriptor.imports = resource->GetImports();

		for (const auto &memoryType : memoryTypes)
		{
			const auto &buffer = resource->GetBinary(memoryType);
			if (buffer == nullptr)
				continue;

			descriptor.memoryBlocks.push_back({ memoryType, buffer.GetSize(), buffer.GetAlignment() });
		}

		resources.emplace_back(std::move(descriptor));
	}

	std::sort(resources.begin(), resources.end(), [](const auto &lhs, const auto &rhs) {
		return std::tie(lhs.resourceType, lhs.resourceID, lhs.streamIndex) < std::tie(rhs.resourceType, rhs.resourceID, rhs.streamIndex);
	});

	// Still describe what can be read, but don't let corrupt resources disappear silently.
	if (unreadableResources > 0)
	{
		SetLastError(ErrorCode::DecompressionFailed, std::to_string(unreadableResources) + " resource(s) could not be decoded and were left out; the bundle may be corrupt.");
		return resources;
	}

	ClearLastError();
	return resources;
}
