// The secure alternatives are non-standard - but the non-secure standard ones can still be used securely.
#define _CRT_SECURE_NO_WARNINGS

#include <libtub/bundle.h>
#include <algorithm>
#include <cassert>
#include <cstring>
#include <new>

using namespace libtub;

namespace
{
	libtub_error MapErrorCode(ErrorCode code)
	{
		switch (code)
		{
		case ErrorCode::Success:
			return LIBTUB_ERROR_SUCCESS;
		case ErrorCode::InvalidBundle:
		case ErrorCode::InvalidMagic:
		case ErrorCode::InvalidPath:
		case ErrorCode::UnsupportedFormat:
		case ErrorCode::UnsupportedPlatform:
		case ErrorCode::UnsupportedVersion:
		case ErrorCode::UnsupportedFlags:
			return LIBTUB_ERROR_INVALID_BUNDLE;
		case ErrorCode::ResourceNotFound:
			return LIBTUB_ERROR_RESOURCE_NOT_FOUND;
		case ErrorCode::DebugDataNotFound:
			return LIBTUB_ERROR_DEBUG_DATA_NOT_FOUND;
		case ErrorCode::OutOfRange:
			return LIBTUB_ERROR_OUT_OF_RANGE;
		case ErrorCode::InvalidProject:
			return LIBTUB_ERROR_INVALID_PROJECT;
		case ErrorCode::DecompressionFailed:
			return LIBTUB_ERROR_DECOMPRESSION_FAILED;
		case ErrorCode::MemoryAllocation:
			return LIBTUB_ERROR_MEMORY_ALLOCATION;
		case ErrorCode::InvalidArgument:
			return LIBTUB_ERROR_INVALID_ARGUMENT;
		default:
			return LIBTUB_ERROR_GENERIC_FAILURE;
		}
	}

	libtub_error CopyStringToBuffer(const std::string &value, char *buffer, size_t length)
	{
		if (buffer == nullptr)
			return LIBTUB_ERROR_INVALID_ARGUMENT;

		if (length == 0)
			return LIBTUB_ERROR_OUT_OF_RANGE;

		const auto copyLength = std::min(value.size(), length - 1);
		std::memcpy(buffer, value.c_str(), copyLength);
		buffer[copyLength] = '\0';
		return LIBTUB_ERROR_SUCCESS;
	}
}

struct libtub_bundle : public Bundle
{
	using Bundle::Bundle;
};

libtub_error libtub_load(libtub_bundle *LIBTUB_NULLABLE *LIBTUB_NONNULL bundle, const char *LIBTUB_NONNULL path)
{
	if (bundle == nullptr || path == nullptr)
		return LIBTUB_ERROR_INVALID_ARGUMENT;

	*bundle = nullptr;
	*bundle = new (std::nothrow) libtub_bundle;
	if (*bundle == nullptr)
		return LIBTUB_ERROR_MEMORY_ALLOCATION;

	if (!(*bundle)->Load(std::string(path)))
	{
		const auto error = MapErrorCode((*bundle)->GetLastErrorCode());
		delete *bundle;
		*bundle = nullptr;
		return (error == LIBTUB_ERROR_SUCCESS) ? LIBTUB_ERROR_INVALID_BUNDLE : error;
	}

	return LIBTUB_ERROR_SUCCESS;
}

libtub_error libtub_create(libtub_bundle *LIBTUB_NULLABLE *LIBTUB_NONNULL bundle, libtub_magic magic, uint16_t version, libtub_platform platform, libtub_flags flags)
{
	if (bundle == nullptr)
		return LIBTUB_ERROR_INVALID_ARGUMENT;

	*bundle = nullptr;
	if (magic != LIBTUB_MAGIC_BNDL && magic != LIBTUB_MAGIC_BND2)
		return LIBTUB_ERROR_INVALID_BUNDLE;

	try
	{
		*bundle = new (std::nothrow) libtub_bundle(static_cast<Magic>(magic), version, static_cast<Platform>(platform), Flags(flags));
		if (*bundle == nullptr)
			return LIBTUB_ERROR_MEMORY_ALLOCATION;
	}
	catch (const std::bad_alloc &)
	{
		return LIBTUB_ERROR_MEMORY_ALLOCATION;
	}
	catch (const std::exception &)
	{
		return LIBTUB_ERROR_INVALID_BUNDLE;
	}

	return LIBTUB_ERROR_SUCCESS;
}

void libtub_free(libtub_bundle *LIBTUB_NULLABLE bundle)
{
	delete bundle;
}

libtub_error libtub_save(libtub_bundle *LIBTUB_NONNULL bundle, const char *LIBTUB_NONNULL path)
{
	if (bundle == nullptr || path == nullptr)
		return LIBTUB_ERROR_INVALID_ARGUMENT;

	if (!bundle->Save(std::string(path)))
		return MapErrorCode(bundle->GetLastErrorCode());

	return LIBTUB_ERROR_SUCCESS;
}

libtub_error libtub_get_last_error_code(const libtub_bundle *LIBTUB_NONNULL bundle)
{
	if (bundle == nullptr)
		return LIBTUB_ERROR_INVALID_ARGUMENT;

	return MapErrorCode(bundle->GetLastErrorCode());
}

libtub_error libtub_get_last_error_message(const libtub_bundle *LIBTUB_NONNULL bundle, char *LIBTUB_NONNULL buffer, size_t length)
{
	if (bundle == nullptr)
		return LIBTUB_ERROR_INVALID_ARGUMENT;

	return CopyStringToBuffer(bundle->GetLastErrorMessage(), buffer, length);
}

libtub_magic libtub_get_magic(const libtub_bundle *LIBTUB_NONNULL bundle)
{
	if (bundle == nullptr)
		return 0;

	return static_cast<libtub_magic>(bundle->GetMagic());
}

uint16_t libtub_get_version(const libtub_bundle *LIBTUB_NONNULL bundle)
{
	if (bundle == nullptr)
		return 0;

	return bundle->GetVersion();
}

libtub_platform libtub_get_platform(const libtub_bundle *LIBTUB_NONNULL bundle)
{
	if (bundle == nullptr)
		return 0;

	return static_cast<libtub_platform>(bundle->GetPlatform());
}

libtub_flags libtub_get_flags(const libtub_bundle *LIBTUB_NONNULL bundle)
{
	if (bundle == nullptr)
		return 0;

	return static_cast<libtub_flags>(bundle->GetFlags());
}

bool libtub_is_burnout_era(const libtub_bundle *LIBTUB_NONNULL bundle)
{
	if (bundle == nullptr)
		return false;

	return bundle->IsBurnoutEra();
}

bool libtub_is_need_for_speed_era(const libtub_bundle *LIBTUB_NONNULL bundle)
{
	if (bundle == nullptr)
		return false;

	return bundle->IsNeedForSpeedEra();
}


/* Resource ID */
libtub_resource_id libtub_resource_id_from_name(const char *LIBTUB_NONNULL name)
{
	if (name == nullptr)
		return 0;

	return static_cast<libtub_resource_id>(ResourceID(name));
}

libtub_resource_id libtub_resource_id_from_game_changer(uint32_t id, uint16_t resourceType, uint8_t index)
{
	return static_cast<libtub_resource_id>(ResourceID(id, resourceType, index, ResourceID::IDType::GameChanger));
}

libtub_resource_id libtub_resource_id_from_components(uint32_t id, uint16_t resourceType, uint8_t index, libtub_resource_id_type idType)
{
	return static_cast<libtub_resource_id>(ResourceID(id, resourceType, index, static_cast<ResourceID::IDType>(idType)));
}

uint32_t libtub_resource_id_get_game_changer_id(libtub_resource_id resourceID)
{
	return ResourceID(resourceID).GetGameChangerID();
}

uint16_t libtub_resource_id_get_resource_type_id(libtub_resource_id resourceID)
{
	return ResourceID(resourceID).GetResourceTypeID();
}

uint8_t libtub_resource_id_get_index(libtub_resource_id resourceID)
{
	return ResourceID(resourceID).GetIndex();
}

libtub_resource_id_type libtub_resource_id_get_id_type(libtub_resource_id resourceID)
{
	return static_cast<libtub_resource_id_type>(ResourceID(resourceID).GetIDType());
}


/* Resource debug data */
struct libtub_resource_debug_data : public ResourceDebugData
{
	using ResourceDebugData::ResourceDebugData;
	constexpr libtub_resource_debug_data(ResourceDebugData &&debugData) : ResourceDebugData(std::move(debugData)) {}
};

libtub_error libtub_get_resource_debug_data(const libtub_bundle *LIBTUB_NONNULL bundle, libtub_resource_debug_data *LIBTUB_NULLABLE *LIBTUB_NONNULL debugData, libtub_resource_id resourceID, uint8_t streamIndex)
{
	if (bundle == nullptr || debugData == nullptr)
		return LIBTUB_ERROR_INVALID_ARGUMENT;

	*debugData = nullptr;
	auto internalDebugData = bundle->GetResourceDebugData(ResourceID(resourceID), streamIndex);
	if (!internalDebugData)
		return MapErrorCode(bundle->GetLastErrorCode());

	*debugData = new (std::nothrow) libtub_resource_debug_data(*std::move(internalDebugData));
	if (*debugData == nullptr)
		return LIBTUB_ERROR_MEMORY_ALLOCATION;

	return LIBTUB_ERROR_SUCCESS;
}

libtub_error libtub_resource_debug_data_create(libtub_resource_debug_data *LIBTUB_NULLABLE *LIBTUB_NONNULL debugData, const char *LIBTUB_NONNULL name, const char *LIBTUB_NONNULL typeName)
{
	if (debugData == nullptr || name == nullptr || typeName == nullptr)
		return LIBTUB_ERROR_INVALID_ARGUMENT;

	*debugData = nullptr;
	*debugData = new (std::nothrow) libtub_resource_debug_data(name, typeName);
	if (*debugData == nullptr)
		return LIBTUB_ERROR_MEMORY_ALLOCATION;

	return LIBTUB_ERROR_SUCCESS;
}

void libtub_resource_debug_data_free(libtub_resource_debug_data *LIBTUB_NULLABLE debugData)
{
	delete debugData;
}

libtub_error libtub_resource_debug_data_get_name(const libtub_resource_debug_data *LIBTUB_NONNULL debugData, char *LIBTUB_NONNULL buffer, size_t length)
{
	if (debugData == nullptr)
		return LIBTUB_ERROR_INVALID_ARGUMENT;

	return CopyStringToBuffer(debugData->GetName(), buffer, length);
}

libtub_error libtub_resource_debug_data_get_type_name(const libtub_resource_debug_data *LIBTUB_NONNULL debugData, char *LIBTUB_NONNULL buffer, size_t length)
{
	if (debugData == nullptr)
		return LIBTUB_ERROR_INVALID_ARGUMENT;

	return CopyStringToBuffer(debugData->GetTypeName(), buffer, length);
}

void libtub_add_resource_debug_data(libtub_bundle *LIBTUB_NONNULL bundle, libtub_resource_id resourceID, const libtub_resource_debug_data *LIBTUB_NONNULL debugData, uint8_t streamIndex)
{
	if (bundle == nullptr || debugData == nullptr)
		return;

	bundle->AddResourceDebugData(ResourceID(resourceID), *debugData, streamIndex);
}


/* Resource type */
libtub_error libtub_get_resource_type(const libtub_bundle *LIBTUB_NONNULL bundle, libtub_resource_type *LIBTUB_NONNULL resourceType, libtub_resource_id resourceID, uint8_t streamIndex)
{
	if (bundle == nullptr || resourceType == nullptr)
		return LIBTUB_ERROR_INVALID_ARGUMENT;

	const auto internalResourceType = bundle->GetResourceType(ResourceID(resourceID), streamIndex);
	if (!internalResourceType)
		return MapErrorCode(bundle->GetLastErrorCode());

	*resourceType = *internalResourceType;

	return LIBTUB_ERROR_SUCCESS;
}


/* Buffer */
struct libtub_buffer : public Buffer
{
	using Buffer::Buffer;
	LIBTUB_DEFAULT_MOVE_CONSTEXPR libtub_buffer(Buffer &&buffer) : Buffer(std::move(buffer)) {}
};

libtub_error libtub_buffer_create(libtub_buffer *LIBTUB_NULLABLE *LIBTUB_NONNULL buffer, const void *LIBTUB_NONNULL data, size_t size, uint32_t alignment)
{
	if (buffer == nullptr || (data == nullptr && size > 0))
		return LIBTUB_ERROR_INVALID_ARGUMENT;

	*buffer = nullptr;
	auto internalPtr = new (std::nothrow) uint8_t[size];
	if (internalPtr == nullptr)
		return LIBTUB_ERROR_MEMORY_ALLOCATION;

	if (size > 0)
		std::memcpy(internalPtr, data, size);

	std::unique_ptr<uint8_t[]> internalData(internalPtr);

	*buffer = new (std::nothrow) libtub_buffer(std::move(internalData), size, alignment);
	if (*buffer == nullptr)
		return LIBTUB_ERROR_MEMORY_ALLOCATION;

	return LIBTUB_ERROR_SUCCESS;
}

void libtub_buffer_free(libtub_buffer *LIBTUB_NULLABLE buffer)
{
	delete buffer;
}

libtub_error libtub_copy_binary(const libtub_bundle *LIBTUB_NONNULL bundle, libtub_buffer *LIBTUB_NULLABLE *LIBTUB_NONNULL buffer, libtub_resource_id resourceID, libtub_memory_type memoryType, uint8_t streamIndex)
{
	if (bundle == nullptr || buffer == nullptr)
		return LIBTUB_ERROR_INVALID_ARGUMENT;

	*buffer = nullptr;
	if (memoryType >= 4)
		return LIBTUB_ERROR_OUT_OF_RANGE;

	auto binary = bundle->GetBinary(ResourceID(resourceID), static_cast<MemoryType>(memoryType), streamIndex);
	if (binary == nullptr)
	{
		const auto error = MapErrorCode(bundle->GetLastErrorCode());
		return (error == LIBTUB_ERROR_SUCCESS) ? LIBTUB_ERROR_RESOURCE_NOT_FOUND : error;
	}

	*buffer = new (std::nothrow) libtub_buffer(std::move(binary));
	if (*buffer == nullptr)
		return LIBTUB_ERROR_MEMORY_ALLOCATION;

	return LIBTUB_ERROR_SUCCESS;
}

void *libtub_buffer_get_data(const libtub_buffer *LIBTUB_NONNULL buffer)
{
	if (buffer == nullptr)
		return nullptr;

	return buffer->GetData();
}

size_t libtub_buffer_get_size(const libtub_buffer *LIBTUB_NONNULL buffer)
{
	if (buffer == nullptr)
		return 0;

	return buffer->GetSize();
}

uint32_t libtub_buffer_get_alignment(const libtub_buffer *LIBTUB_NONNULL buffer)
{
	if (buffer == nullptr)
		return 0;

	return buffer->GetAlignment();
}


/* Import */
struct libtub_import : public Import
{
	using Import::Import;
	constexpr libtub_import(Import &&import) : Import(std::move(import)) {}
};

libtub_error libtub_import_create(libtub_import *LIBTUB_NULLABLE *LIBTUB_NONNULL import, libtub_resource_id resourceID, uint32_t offset, libtub_import_type importType)
{
	if (import == nullptr)
		return LIBTUB_ERROR_INVALID_ARGUMENT;

	*import = nullptr;
	if (importType > LIBTUB_IMPORT_TYPE_RESOURCE_HANDLE)
		return LIBTUB_ERROR_OUT_OF_RANGE;

	*import = new (std::nothrow) libtub_import(ResourceID(resourceID), offset, static_cast<Import::ImportType>(importType));
	if (*import == nullptr)
		return LIBTUB_ERROR_MEMORY_ALLOCATION;

	return LIBTUB_ERROR_SUCCESS;
}

void libtub_import_free(libtub_import *LIBTUB_NULLABLE import)
{
	delete import;
}

libtub_resource_id libtub_import_get_resource_id(const libtub_import *LIBTUB_NONNULL import)
{
	if (import == nullptr)
		return 0;

	return static_cast<libtub_resource_id>(import->GetResourceID());
}

uint32_t libtub_import_get_offset(const libtub_import *LIBTUB_NONNULL import)
{
	if (import == nullptr)
		return 0;

	return import->GetOffset();
}

libtub_import_type libtub_import_get_import_type(const libtub_import *LIBTUB_NONNULL import)
{
	if (import == nullptr)
		return 0;

	return static_cast<libtub_import_type>(import->GetImportType());
}


/* Resource */
struct libtub_resource : public Resource
{
	using Resource::Resource;
	LIBTUB_DEFAULT_MOVE_CONSTEXPR libtub_resource(Resource &&resource) : Resource(std::move(resource)) {}
};

libtub_error libtub_resource_create(libtub_resource *LIBTUB_NULLABLE *LIBTUB_NONNULL resource, libtub_resource_type resourceType)
{
	if (resource == nullptr)
		return LIBTUB_ERROR_INVALID_ARGUMENT;

	*resource = nullptr;
	*resource = new (std::nothrow) libtub_resource(resourceType);
	if (*resource == nullptr)
		return LIBTUB_ERROR_MEMORY_ALLOCATION;

	return LIBTUB_ERROR_SUCCESS;
}

void libtub_resource_free(libtub_resource *LIBTUB_NULLABLE resource)
{
	delete resource;
}

libtub_error libtub_copy_resource(const libtub_bundle *LIBTUB_NONNULL bundle, libtub_resource *LIBTUB_NULLABLE *LIBTUB_NONNULL resource, libtub_resource_id resourceID, uint8_t streamIndex)
{
	if (bundle == nullptr || resource == nullptr)
		return LIBTUB_ERROR_INVALID_ARGUMENT;

	*resource = nullptr;
	auto internalResource = bundle->GetResource(ResourceID(resourceID), streamIndex);
	if (!internalResource)
		return MapErrorCode(bundle->GetLastErrorCode());

	*resource = new (std::nothrow) libtub_resource(*std::move(internalResource));
	if (*resource == nullptr)
		return LIBTUB_ERROR_MEMORY_ALLOCATION;

	return LIBTUB_ERROR_SUCCESS;
}

libtub_error libtub_resource_get_binary_mut(libtub_resource *LIBTUB_NONNULL resource, libtub_buffer *LIBTUB_NULLABLE *LIBTUB_NONNULL buffer, libtub_memory_type memoryType)
{
	if (resource == nullptr || buffer == nullptr)
		return LIBTUB_ERROR_INVALID_ARGUMENT;

	*buffer = nullptr;
	if (memoryType >= 4)
		return LIBTUB_ERROR_OUT_OF_RANGE;

	*buffer = &reinterpret_cast<libtub_buffer &>(resource->GetBinary(static_cast<libtub::MemoryType>(memoryType)));
	return LIBTUB_ERROR_SUCCESS;
}

libtub_error libtub_resource_get_binary_const(const libtub_resource *LIBTUB_NONNULL resource, const libtub_buffer *LIBTUB_NULLABLE *LIBTUB_NONNULL buffer, libtub_memory_type memoryType)
{
	if (resource == nullptr || buffer == nullptr)
		return LIBTUB_ERROR_INVALID_ARGUMENT;

	*buffer = nullptr;
	if (memoryType >= 4)
		return LIBTUB_ERROR_OUT_OF_RANGE;

	*buffer = &reinterpret_cast<const libtub_buffer &>(resource->GetBinary(static_cast<libtub::MemoryType>(memoryType)));
	return LIBTUB_ERROR_SUCCESS;
}

size_t libtub_resource_get_import_count(const libtub_resource *LIBTUB_NONNULL resource)
{
	if (resource == nullptr)
		return 0;

	return resource->GetImports().size();
}

libtub_error libtub_resource_copy_import(const libtub_resource *LIBTUB_NONNULL resource, libtub_import *LIBTUB_NULLABLE *LIBTUB_NONNULL import, size_t index)
{
	if (resource == nullptr || import == nullptr)
		return LIBTUB_ERROR_INVALID_ARGUMENT;

	*import = nullptr;
	auto imports = resource->GetImports();
	if (index >= imports.size())
		return LIBTUB_ERROR_OUT_OF_RANGE;

	*import = new (std::nothrow) libtub_import(std::move(imports[index]));
	if (*import == nullptr)
		return LIBTUB_ERROR_MEMORY_ALLOCATION;

	return LIBTUB_ERROR_SUCCESS;
}

libtub_error libtub_resource_replace_binary(libtub_resource *LIBTUB_NONNULL resource, const libtub_buffer *LIBTUB_NONNULL buffer, libtub_memory_type memoryType)
{
	if (resource == nullptr || buffer == nullptr)
		return LIBTUB_ERROR_INVALID_ARGUMENT;

	if (memoryType >= 4)
		return LIBTUB_ERROR_OUT_OF_RANGE;

	auto size = buffer->GetSize();
	auto newData = new (std::nothrow) uint8_t[size];
	if (newData == nullptr)
		return LIBTUB_ERROR_MEMORY_ALLOCATION;

	if (size > 0)
		std::memcpy(newData, buffer->GetData(), size);

	std::unique_ptr<uint8_t[]> newPtr(newData);
	Buffer newBuffer(std::move(newPtr), size, buffer->GetAlignment());
	resource->ReplaceBinary(static_cast<MemoryType>(memoryType), std::move(newBuffer));

	return LIBTUB_ERROR_SUCCESS;
}

void libtub_resource_add_import(libtub_resource *LIBTUB_NONNULL resource, libtub_import *LIBTUB_NONNULL import)
{
	if (resource == nullptr || import == nullptr)
		return;

	resource->AddImport(*import);
}

libtub_error libtub_add_resource(libtub_bundle *LIBTUB_NONNULL bundle, libtub_resource_id resourceID, const libtub_resource *LIBTUB_NONNULL resource, uint8_t streamIndex)
{
	if (bundle == nullptr || resource == nullptr)
		return LIBTUB_ERROR_INVALID_ARGUMENT;

	if (!bundle->AddResource(ResourceID(resourceID), *resource, streamIndex))
		return MapErrorCode(bundle->GetLastErrorCode());

	return LIBTUB_ERROR_SUCCESS;
}

libtub_error libtub_replace_resource(libtub_bundle *LIBTUB_NONNULL bundle, libtub_resource_id resourceID, const libtub_resource *LIBTUB_NONNULL resource, uint8_t streamIndex)
{
	if (bundle == nullptr || resource == nullptr)
		return LIBTUB_ERROR_INVALID_ARGUMENT;

	if (!bundle->ReplaceResource(ResourceID(resourceID), *resource, streamIndex))
		return MapErrorCode(bundle->GetLastErrorCode());

	return LIBTUB_ERROR_SUCCESS;
}


/* Bundle helpers */
uint32_t libtub_get_resource_count(const libtub_bundle *LIBTUB_NONNULL bundle)
{
	if (bundle == nullptr)
		return 0;

	return bundle->GetResourceCount();
}

libtub_error libtub_get_resource_id_at_index(const libtub_bundle *LIBTUB_NONNULL bundle, libtub_resource_id *LIBTUB_NONNULL resourceID, uint32_t index)
{
	if (bundle == nullptr || resourceID == nullptr)
		return LIBTUB_ERROR_INVALID_ARGUMENT;

	const auto resourceIDs = bundle->GetResourceIDs();
	if (index >= resourceIDs.size())
		return LIBTUB_ERROR_OUT_OF_RANGE;

	*resourceID = static_cast<libtub_resource_id>(resourceIDs[index]);
	return LIBTUB_ERROR_SUCCESS;
}

bool libtub_is_populated_resource_stream_index(const libtub_bundle *LIBTUB_NONNULL bundle, libtub_resource_id resourceID, uint8_t streamIndex)
{
	if (bundle == nullptr)
		return false;

	const auto indices = bundle->GetResourceStreamIndices(ResourceID(resourceID));
	return std::find(indices.begin(), indices.end(), streamIndex) != indices.end();
}

libtub_resource_id libtub_get_default_resource_id(const libtub_bundle *LIBTUB_NONNULL bundle)
{
	if (bundle == nullptr)
		return 0;

	return static_cast<libtub_resource_id>(bundle->GetDefaultResourceID());
}

int32_t libtub_get_default_resource_stream_index(const libtub_bundle *LIBTUB_NONNULL bundle)
{
	if (bundle == nullptr)
		return -1;

	return bundle->GetDefaultResourceStreamIndex();
}

libtub_error libtub_get_stream_name(const libtub_bundle *LIBTUB_NONNULL bundle, char *LIBTUB_NONNULL buffer, size_t length, uint8_t streamIndex)
{
	if (bundle == nullptr)
		return LIBTUB_ERROR_INVALID_ARGUMENT;

	return CopyStringToBuffer(bundle->GetStreamName(streamIndex), buffer, length);
}

bool libtub_is_valid_memory_type(const libtub_bundle *LIBTUB_NONNULL bundle, libtub_memory_type memoryType)
{
	if (bundle == nullptr)
		return false;

	const auto memoryTypes = bundle->GetMemoryTypes();
	return std::find(memoryTypes.begin(), memoryTypes.end(), static_cast<MemoryType>(memoryType)) != memoryTypes.end();
}
