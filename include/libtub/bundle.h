#pragma once
#ifdef __cplusplus
#include <libtub/bundle.hpp>
#endif
#ifndef LIBTUB_EXPORT
#include <libtub/internal/export.h>
#endif
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __has_feature
#	define LIBTUB_HAS_FEATURE(feat) __has_feature(feat)
#else
#	define LIBTUB_HAS_FEATURE(feat) 0
#endif

#if LIBTUB_HAS_FEATURE(nullability)
#	define LIBTUB_NONNULL _Nonnull
#	define LIBTUB_NULLABLE _Nullable
#else
#	define LIBTUB_NONNULL
#	define LIBTUB_NULLABLE
#endif

#ifdef __cplusplus
extern "C"
{
#endif
	typedef struct libtub_bundle libtub_bundle;

	typedef enum
	{
		LIBTUB_ERROR_SUCCESS = 0,

		LIBTUB_ERROR_INVALID_BUNDLE = 1,
		LIBTUB_ERROR_GENERIC_FAILURE = 2,
		LIBTUB_ERROR_RESOURCE_NOT_FOUND = 3,
		LIBTUB_ERROR_DEBUG_DATA_NOT_FOUND = 4,
		LIBTUB_ERROR_OUT_OF_RANGE = 5,

		LIBTUB_ERROR_MEMORY_ALLOCATION = -1,
	} libtub_error;

	enum {
#define LIBTUB_ENUM_MAGIC(_, name, value) LIBTUB_MAGIC_##name = value,
#include <libtub/internal/enum.inc>
#undef LIBTUB_ENUM_MAGIC
	};
	typedef uint8_t libtub_magic;

	enum
	{
#define LIBTUB_ENUM_PLATFORM(_, name, value) LIBTUB_PLATFORM_##name = value,
#include <libtub/internal/enum.inc>
#undef LIBTUB_ENUM_PLATFORM
	};
	typedef uint16_t libtub_platform;

	enum
	{
#define LIBTUB_ENUM_FLAGS(_, name, value) LIBTUB_FLAGS_##name = value,
#include <libtub/internal/enum.inc>
#undef LIBTUB_ENUM_FLAGS
	};
	typedef uint32_t libtub_flags;


	LIBTUB_EXPORT libtub_error libtub_load(libtub_bundle *LIBTUB_NULLABLE *LIBTUB_NONNULL bundle, const char *LIBTUB_NONNULL path);
	LIBTUB_EXPORT libtub_error libtub_create(libtub_bundle *LIBTUB_NULLABLE *LIBTUB_NONNULL bundle, libtub_magic magic, uint16_t version, libtub_platform platform, libtub_flags flags);
	LIBTUB_EXPORT void libtub_free(libtub_bundle *LIBTUB_NULLABLE bundle);

	LIBTUB_EXPORT libtub_error libtub_save(libtub_bundle *LIBTUB_NONNULL bundle, const char *LIBTUB_NONNULL path);

	LIBTUB_EXPORT libtub_magic libtub_get_magic(const libtub_bundle *LIBTUB_NONNULL bundle);
	LIBTUB_EXPORT uint16_t libtub_get_version(const libtub_bundle *LIBTUB_NONNULL bundle);
	LIBTUB_EXPORT libtub_platform libtub_get_platform(const libtub_bundle *LIBTUB_NONNULL bundle);
	LIBTUB_EXPORT libtub_flags libtub_get_flags(const libtub_bundle *LIBTUB_NONNULL bundle);

	LIBTUB_EXPORT bool libtub_is_burnout_era(const libtub_bundle *LIBTUB_NONNULL bundle);
	LIBTUB_EXPORT bool libtub_is_need_for_speed_era(const libtub_bundle *LIBTUB_NONNULL bundle);


	/* Resource ID */
	typedef uint64_t libtub_resource_id;

	enum
	{
#define LIBTUB_ENUM_ID_TYPE(_, name, value) LIBTUB_RESOURCE_ID_TYPE_##name = value,
#include <libtub/internal/enum.inc>
#undef LIBTUB_ENUM_ID_TYPE
	};
	typedef uint8_t libtub_resource_id_type;

	LIBTUB_EXPORT libtub_resource_id libtub_resource_id_from_name(const char *LIBTUB_NONNULL name);
	LIBTUB_EXPORT libtub_resource_id libtub_resource_id_from_game_changer(uint32_t id, uint16_t resourceType, uint8_t index);
	LIBTUB_EXPORT libtub_resource_id libtub_resource_id_from_components(uint32_t id, uint16_t resourceType, uint8_t index, libtub_resource_id_type idType);

	LIBTUB_EXPORT uint32_t libtub_resource_id_get_game_changer_id(libtub_resource_id resourceID);
#	define libtub_resource_id_get_id32(resourceID) libtub_resource_id_get_game_changer_id(resourceID)
	LIBTUB_EXPORT uint16_t libtub_resource_id_get_resource_type_id(libtub_resource_id resourceID);
	LIBTUB_EXPORT uint8_t libtub_resource_id_get_index(libtub_resource_id resourceID);
	LIBTUB_EXPORT libtub_resource_id_type libtub_resource_id_get_id_type(libtub_resource_id resourceID);


	/* Resource debug data */
	typedef struct libtub_resource_debug_data libtub_resource_debug_data;

	LIBTUB_EXPORT libtub_error libtub_get_resource_debug_data(const libtub_bundle *LIBTUB_NONNULL bundle, libtub_resource_debug_data *LIBTUB_NULLABLE *LIBTUB_NONNULL debugData, libtub_resource_id resourceID, uint8_t streamIndex);

	LIBTUB_EXPORT libtub_error libtub_resource_debug_data_create(libtub_resource_debug_data *LIBTUB_NULLABLE *LIBTUB_NONNULL debugData, const char *LIBTUB_NONNULL name, const char *LIBTUB_NONNULL typeName);
	LIBTUB_EXPORT void libtub_resource_debug_data_free(libtub_resource_debug_data *LIBTUB_NULLABLE debugData);

	LIBTUB_EXPORT libtub_error libtub_resource_debug_data_get_name(const libtub_resource_debug_data *LIBTUB_NONNULL debugData, char *LIBTUB_NONNULL buffer, size_t length);
	LIBTUB_EXPORT libtub_error libtub_resource_debug_data_get_type_name(const libtub_resource_debug_data *LIBTUB_NONNULL debugData, char *LIBTUB_NONNULL buffer, size_t length);

	LIBTUB_EXPORT void libtub_add_resource_debug_data(libtub_bundle *LIBTUB_NONNULL bundle, libtub_resource_id resourceID, const libtub_resource_debug_data *LIBTUB_NONNULL debugData, uint8_t streamIndex);


	/* Resource type */
	enum
	{
#define LIBTUB_ENUM_RESOURCE_TYPE_BURNOUT(_, name, value) LIBTUB_RESOURCE_TYPE_BURNOUT_##name = value,
#include <libtub/internal/enum.inc>
#undef LIBTUB_ENUM_RESOURCE_TYPE_BURNOUT
	};
	enum
	{
#define LIBTUB_ENUM_RESOURCE_TYPE_NFS(_, name, value) LIBTUB_RESOURCE_TYPE_NFS_##name = value,
#include <libtub/internal/enum.inc>
#undef LIBTUB_ENUM_RESOURCE_TYPE_NFS
	};
	typedef uint32_t libtub_resource_type;

	LIBTUB_EXPORT libtub_error libtub_get_resource_type(const libtub_bundle *LIBTUB_NONNULL bundle, libtub_resource_type *LIBTUB_NONNULL resourceType, libtub_resource_id resourceID, uint8_t streamIndex);


	/* Buffer */
	typedef struct libtub_buffer libtub_buffer;

	enum
	{
#define LIBTUB_ENUM_MEMORY_TYPE(_, name, value) LIBTUB_MEMORY_TYPE_##name = value,
#include <libtub/internal/enum.inc>
#undef LIBTUB_ENUM_MEMORY_TYPE
	};
	typedef uint8_t libtub_memory_type;

	LIBTUB_EXPORT libtub_error libtub_buffer_create(libtub_buffer *LIBTUB_NULLABLE *LIBTUB_NONNULL buffer, const void *LIBTUB_NONNULL data, size_t size, uint32_t alignment);
	LIBTUB_EXPORT void libtub_buffer_free(libtub_buffer *LIBTUB_NULLABLE buffer);

	LIBTUB_EXPORT libtub_error libtub_copy_binary(const libtub_bundle *LIBTUB_NONNULL bundle, libtub_buffer *LIBTUB_NULLABLE *LIBTUB_NONNULL buffer, libtub_resource_id resourceID, libtub_memory_type memoryType, uint8_t streamIndex);

	LIBTUB_EXPORT void *LIBTUB_NULLABLE libtub_buffer_get_data(const libtub_buffer *LIBTUB_NONNULL buffer);
	LIBTUB_EXPORT size_t libtub_buffer_get_size(const libtub_buffer *LIBTUB_NONNULL buffer);
	LIBTUB_EXPORT uint32_t libtub_buffer_get_alignment(const libtub_buffer *LIBTUB_NONNULL buffer);


	/* Import */
	typedef struct libtub_import libtub_import;

	enum
	{
#define LIBTUB_ENUM_IMPORT_TYPE(_, name, value) LIBTUB_IMPORT_TYPE_##name = value,
#include <libtub/internal/enum.inc>
#undef LIBTUB_ENUM_IMPORT_TYPE
	};
	typedef uint8_t libtub_import_type;

	LIBTUB_EXPORT libtub_error libtub_import_create(libtub_import *LIBTUB_NULLABLE *LIBTUB_NONNULL import, libtub_resource_id resourceID, uint32_t offset, libtub_import_type importType);
	LIBTUB_EXPORT void libtub_import_free(libtub_import *LIBTUB_NULLABLE import);

	LIBTUB_EXPORT libtub_resource_id libtub_import_get_resource_id(const libtub_import *LIBTUB_NONNULL import);
	LIBTUB_EXPORT uint32_t libtub_import_get_offset(const libtub_import *LIBTUB_NONNULL import);
	LIBTUB_EXPORT libtub_import_type libtub_import_get_import_type(const libtub_import *LIBTUB_NONNULL import);


	/* Resource */
	typedef struct libtub_resource libtub_resource;

	LIBTUB_EXPORT libtub_error libtub_resource_create(libtub_resource *LIBTUB_NULLABLE *LIBTUB_NONNULL resource, libtub_resource_type resourceType);
	LIBTUB_EXPORT void libtub_resource_free(libtub_resource *LIBTUB_NULLABLE resource);

	LIBTUB_EXPORT libtub_error libtub_copy_resource(const libtub_bundle *LIBTUB_NONNULL bundle, libtub_resource *LIBTUB_NULLABLE *LIBTUB_NONNULL resource, libtub_resource_id resourceID, uint8_t streamIndex);

	LIBTUB_EXPORT libtub_error libtub_resource_get_binary_mut(libtub_resource *LIBTUB_NONNULL resource, libtub_buffer *LIBTUB_NULLABLE *LIBTUB_NONNULL buffer, libtub_memory_type memoryType);
	LIBTUB_EXPORT libtub_error libtub_resource_get_binary_const(const libtub_resource *LIBTUB_NONNULL resource, const libtub_buffer *LIBTUB_NULLABLE *LIBTUB_NONNULL buffer, libtub_memory_type memoryType);
#	define libtub_resource_get_binary(resource, buffer, memoryType) _Generic((resource), \
		const libtub_resource *: libtub_resource_get_binary_const, \
		libtub_resource *: libtub_resource_get_binary_mut)(resource, buffer, memoryType)

	LIBTUB_EXPORT size_t libtub_resource_get_import_count(const libtub_resource *LIBTUB_NONNULL resource);
	LIBTUB_EXPORT libtub_error libtub_resource_copy_import(const libtub_resource *LIBTUB_NONNULL resource, libtub_import *LIBTUB_NULLABLE *LIBTUB_NONNULL import, size_t index);

	LIBTUB_EXPORT libtub_error libtub_resource_replace_binary(libtub_resource *LIBTUB_NONNULL resource, const libtub_buffer *LIBTUB_NONNULL buffer, libtub_memory_type memoryType);
	LIBTUB_EXPORT void libtub_resource_add_import(libtub_resource *LIBTUB_NONNULL resource, libtub_import *LIBTUB_NONNULL import);

	LIBTUB_EXPORT libtub_error libtub_add_resource(libtub_bundle *LIBTUB_NONNULL bundle, libtub_resource_id resourceID, const libtub_resource *LIBTUB_NONNULL resource, uint8_t streamIndex);

	LIBTUB_EXPORT libtub_error libtub_replace_resource(libtub_bundle *LIBTUB_NONNULL bundle, libtub_resource_id resourceID, const libtub_resource *LIBTUB_NONNULL resource, uint8_t streamIndex);


	/* Bundle helpers */
	LIBTUB_EXPORT uint32_t libtub_get_resource_count(const libtub_bundle *LIBTUB_NONNULL bundle);
	LIBTUB_EXPORT libtub_error libtub_get_resource_id_at_index(const libtub_bundle *LIBTUB_NONNULL bundle, libtub_resource_id *LIBTUB_NONNULL resourceID, uint32_t index);

#	define LIBTUB_STREAM_MAX_COUNT 4
	LIBTUB_EXPORT bool libtub_is_populated_resource_stream_index(const libtub_bundle *LIBTUB_NONNULL bundle, libtub_resource_id resourceID, uint8_t streamIndex);

	LIBTUB_EXPORT libtub_resource_id libtub_get_default_resource_id(const libtub_bundle *LIBTUB_NONNULL bundle);
	LIBTUB_EXPORT int32_t libtub_get_default_resource_stream_index(const libtub_bundle *LIBTUB_NONNULL bundle);

#	define LIBTUB_STREAM_NAME_MAX_LENGTH 15
	LIBTUB_EXPORT libtub_error libtub_get_stream_name(const libtub_bundle *LIBTUB_NONNULL bundle, char *LIBTUB_NONNULL buffer, size_t length, uint8_t streamIndex);

	LIBTUB_EXPORT bool libtub_is_valid_memory_type(const libtub_bundle *LIBTUB_NONNULL bundle, libtub_memory_type memoryType);
#ifdef __cplusplus
}
#endif
