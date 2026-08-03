#pragma once
#ifdef __cplusplus
#include <libbndl/bundle.hpp>
#endif
#ifndef LIBBNDL_EXPORT
#include <libbndl/internal/export.h>
#endif
#include <stdbool.h>
#include <stdint.h>

#ifdef __has_feature
#	define LIBBNDL_HAS_FEATURE(feat) __has_feature(feat)
#else
#	define LIBBNDL_HAS_FEATURE(feat) 0
#endif

#if LIBBNDL_HAS_FEATURE(nullability)
#	define LIBBNDL_NONNULL _Nonnull
#	define LIBBNDL_NULLABLE _Nullable
#else
#	define LIBBNDL_NONNULL
#	define LIBBNDL_NULLABLE
#endif

#ifdef __cplusplus
extern "C"
{
#endif
	typedef struct libbndl_bundle libbndl_bundle;

	typedef enum
	{
		LIBBNDL_ERROR_SUCCESS = 0,

		LIBBNDL_ERROR_INVALID_BUNDLE = 1,
		LIBBNDL_ERROR_GENERIC_FAILURE = 2,
		LIBBNDL_ERROR_RESOURCE_NOT_FOUND = 3,
		LIBBNDL_ERROR_DEBUG_DATA_NOT_FOUND = 4,
		LIBBNDL_ERROR_OUT_OF_RANGE = 5,
		LIBBNDL_ERROR_INSUFFICIENT_BUFFER = 6,

		LIBBNDL_ERROR_MEMORY_ALLOCATION = -1,
	} libbndl_error;

	enum {
#define LIBBNDL_ENUM_MAGIC(_, name, value) LIBBNDL_MAGIC_##name = (value),
#include <libbndl/internal/enum.inc>
#undef LIBBNDL_ENUM_MAGIC
	};
	typedef uint8_t libbndl_magic;

	enum
	{
#define LIBBNDL_ENUM_PLATFORM(_, name, value) LIBBNDL_PLATFORM_##name = (value),
#include <libbndl/internal/enum.inc>
#undef LIBBNDL_ENUM_PLATFORM
	};
	typedef uint16_t libbndl_platform;

	enum
	{
#define LIBBNDL_ENUM_FLAGS(_, name, value) LIBBNDL_FLAGS_##name = (value),
#include <libbndl/internal/enum.inc>
#undef LIBBNDL_ENUM_FLAGS
	};
	typedef uint32_t libbndl_flags;


	LIBBNDL_EXPORT libbndl_error libbndl_load(libbndl_bundle *LIBBNDL_NULLABLE *LIBBNDL_NONNULL bundle, const char *LIBBNDL_NONNULL path);
	LIBBNDL_EXPORT libbndl_error libbndl_create(libbndl_bundle *LIBBNDL_NULLABLE *LIBBNDL_NONNULL bundle, libbndl_magic magic, uint16_t version, libbndl_platform platform, libbndl_flags flags);
	LIBBNDL_EXPORT void libbndl_free(libbndl_bundle *LIBBNDL_NULLABLE bundle);

	LIBBNDL_EXPORT libbndl_error libbndl_save(libbndl_bundle *LIBBNDL_NONNULL bundle, const char *LIBBNDL_NONNULL path);

	LIBBNDL_EXPORT libbndl_magic libbndl_get_magic(const libbndl_bundle *LIBBNDL_NONNULL bundle);
	LIBBNDL_EXPORT uint16_t libbndl_get_version(const libbndl_bundle *LIBBNDL_NONNULL bundle);
	LIBBNDL_EXPORT libbndl_platform libbndl_get_platform(const libbndl_bundle *LIBBNDL_NONNULL bundle);
	LIBBNDL_EXPORT libbndl_flags libbndl_get_flags(const libbndl_bundle *LIBBNDL_NONNULL bundle);

	LIBBNDL_EXPORT bool libbndl_is_burnout_era(const libbndl_bundle *LIBBNDL_NONNULL bundle);
	LIBBNDL_EXPORT bool libbndl_is_need_for_speed_era(const libbndl_bundle *LIBBNDL_NONNULL bundle);


	/* Resource ID */
	typedef uint64_t libbndl_resource_id;

	enum
	{
#define LIBBNDL_ENUM_ID_TYPE(_, name, value) LIBBNDL_RESOURCE_ID_TYPE_##name = (value),
#include <libbndl/internal/enum.inc>
#undef LIBBNDL_ENUM_ID_TYPE
	};
	typedef uint8_t libbndl_resource_id_type;

	LIBBNDL_EXPORT libbndl_resource_id libbndl_resource_id_from_name(const char *LIBBNDL_NONNULL name);
	LIBBNDL_EXPORT libbndl_resource_id libbndl_resource_id_from_game_changer(uint32_t id, uint16_t resourceType, uint8_t index);
	LIBBNDL_EXPORT libbndl_resource_id libbndl_resource_id_from_components(uint32_t id, uint16_t resourceType, uint8_t index, libbndl_resource_id_type idType);

	LIBBNDL_EXPORT uint32_t libbndl_resource_id_get_game_changer_id(libbndl_resource_id resourceID);
#	define libbndl_resource_id_get_id32(resourceID) libbndl_resource_id_get_game_changer_id(resourceID)
	LIBBNDL_EXPORT uint16_t libbndl_resource_id_get_resource_type_id(libbndl_resource_id resourceID);
	LIBBNDL_EXPORT uint8_t libbndl_resource_id_get_index(libbndl_resource_id resourceID);
	LIBBNDL_EXPORT libbndl_resource_id_type libbndl_resource_id_get_id_type(libbndl_resource_id resourceID);


	/* Resource debug data */
	typedef struct libbndl_resource_debug_data libbndl_resource_debug_data;

	LIBBNDL_EXPORT libbndl_error libbndl_get_resource_debug_data(const libbndl_bundle *LIBBNDL_NONNULL bundle, libbndl_resource_debug_data *LIBBNDL_NULLABLE *LIBBNDL_NONNULL debugData, libbndl_resource_id resourceID, uint8_t streamIndex);

	LIBBNDL_EXPORT libbndl_error libbndl_resource_debug_data_create(libbndl_resource_debug_data *LIBBNDL_NULLABLE *LIBBNDL_NONNULL debugData, const char *LIBBNDL_NONNULL name, const char *LIBBNDL_NONNULL typeName);
	LIBBNDL_EXPORT void libbndl_resource_debug_data_free(libbndl_resource_debug_data *LIBBNDL_NULLABLE debugData);

	LIBBNDL_EXPORT libbndl_error libbndl_resource_debug_data_get_name(const libbndl_resource_debug_data *LIBBNDL_NONNULL debugData, char *LIBBNDL_NONNULL buffer, size_t length);
	LIBBNDL_EXPORT libbndl_error libbndl_resource_debug_data_get_type_name(const libbndl_resource_debug_data *LIBBNDL_NONNULL debugData, char *LIBBNDL_NONNULL buffer, size_t length);

	LIBBNDL_EXPORT void libbndl_add_resource_debug_data(libbndl_bundle *LIBBNDL_NONNULL bundle, libbndl_resource_id resourceID, const libbndl_resource_debug_data *LIBBNDL_NONNULL debugData, uint8_t streamIndex);


	/* Resource type */
	enum
	{
#define LIBBNDL_ENUM_RESOURCE_TYPE_BURNOUT(_, name, value) LIBBNDL_RESOURCE_TYPE_BURNOUT_##name = (value),
#include <libbndl/internal/enum.inc>
#undef LIBBNDL_ENUM_RESOURCE_TYPE_BURNOUT
	};
	enum
	{
#define LIBBNDL_ENUM_RESOURCE_TYPE_NFS(_, name, value) LIBBNDL_RESOURCE_TYPE_NFS_##name = (value),
#include <libbndl/internal/enum.inc>
#undef LIBBNDL_ENUM_RESOURCE_TYPE_NFS
	};
	typedef uint32_t libbndl_resource_type;

	LIBBNDL_EXPORT libbndl_error libbndl_get_resource_type(const libbndl_bundle *LIBBNDL_NONNULL bundle, libbndl_resource_type *LIBBNDL_NONNULL resourceType, libbndl_resource_id resourceID, uint8_t streamIndex);


	/* Buffer */
	typedef struct libbndl_buffer libbndl_buffer;

	enum
	{
#define LIBBNDL_ENUM_MEMORY_TYPE(_, name, value) LIBBNDL_MEMORY_TYPE_##name = (value),
#include <libbndl/internal/enum.inc>
#undef LIBBNDL_ENUM_MEMORY_TYPE
	};
	typedef uint8_t libbndl_memory_type;

	LIBBNDL_EXPORT libbndl_error libbndl_buffer_create(libbndl_buffer *LIBBNDL_NULLABLE *LIBBNDL_NONNULL buffer, const void *LIBBNDL_NONNULL data, size_t size, uint32_t alignment);
	LIBBNDL_EXPORT void libbndl_buffer_free(libbndl_buffer *LIBBNDL_NULLABLE buffer);

	LIBBNDL_EXPORT libbndl_error libbndl_copy_binary(const libbndl_bundle *LIBBNDL_NONNULL bundle, libbndl_buffer *LIBBNDL_NULLABLE *LIBBNDL_NONNULL buffer, libbndl_resource_id resourceID, libbndl_memory_type memoryType, uint8_t streamIndex);

	LIBBNDL_EXPORT void *LIBBNDL_NULLABLE libbndl_buffer_get_data(const libbndl_buffer *LIBBNDL_NONNULL buffer);
	LIBBNDL_EXPORT size_t libbndl_buffer_get_size(const libbndl_buffer *LIBBNDL_NONNULL buffer);
	LIBBNDL_EXPORT uint32_t libbndl_buffer_get_alignment(const libbndl_buffer *LIBBNDL_NONNULL buffer);


	/* Import */
	typedef struct libbndl_import libbndl_import;

	enum
	{
#define LIBBNDL_ENUM_IMPORT_TYPE(_, name, value) LIBBNDL_IMPORT_TYPE_##name = (value),
#include <libbndl/internal/enum.inc>
#undef LIBBNDL_ENUM_IMPORT_TYPE
	};
	typedef uint8_t libbndl_import_type;

	LIBBNDL_EXPORT libbndl_error libbndl_import_create(libbndl_import *LIBBNDL_NULLABLE *LIBBNDL_NONNULL import, libbndl_resource_id resourceID, uint32_t offset, libbndl_import_type importType);
	LIBBNDL_EXPORT void libbndl_import_free(libbndl_import *LIBBNDL_NULLABLE import);

	LIBBNDL_EXPORT libbndl_resource_id libbndl_import_get_resource_id(const libbndl_import *LIBBNDL_NONNULL import);
	LIBBNDL_EXPORT uint32_t libbndl_import_get_offset(const libbndl_import *LIBBNDL_NONNULL import);
	LIBBNDL_EXPORT libbndl_import_type libbndl_import_get_import_type(const libbndl_import *LIBBNDL_NONNULL import);


	/* Resource */
	typedef struct libbndl_resource libbndl_resource;

	LIBBNDL_EXPORT libbndl_error libbndl_resource_create(libbndl_resource *LIBBNDL_NULLABLE *LIBBNDL_NONNULL resource, libbndl_resource_type resourceType);
	LIBBNDL_EXPORT void libbndl_resource_free(libbndl_resource *LIBBNDL_NULLABLE resource);

	LIBBNDL_EXPORT libbndl_error libbndl_copy_resource(const libbndl_bundle *LIBBNDL_NONNULL bundle, libbndl_resource *LIBBNDL_NULLABLE *LIBBNDL_NONNULL resource, libbndl_resource_id resourceID, uint8_t streamIndex);

	LIBBNDL_EXPORT libbndl_error libbndl_resource_get_binary_mut(libbndl_resource *LIBBNDL_NONNULL resource, libbndl_buffer *LIBBNDL_NULLABLE *LIBBNDL_NONNULL buffer, libbndl_memory_type memoryType);
	LIBBNDL_EXPORT libbndl_error libbndl_resource_get_binary_const(const libbndl_resource *LIBBNDL_NONNULL resource, const libbndl_buffer *LIBBNDL_NULLABLE *LIBBNDL_NONNULL buffer, libbndl_memory_type memoryType);
#	define libbndl_resource_get_binary(resource, buffer, memoryType) _Generic((resource), \
		const libbndl_resource *: libbndl_resource_get_binary_const, \
		libbndl_resource *: libbndl_resource_get_binary_mut)(resource, buffer, memoryType)

	LIBBNDL_EXPORT size_t libbndl_resource_get_import_count(const libbndl_resource *LIBBNDL_NONNULL resource);
	LIBBNDL_EXPORT libbndl_error libbndl_resource_copy_import(const libbndl_resource *LIBBNDL_NONNULL resource, libbndl_import *LIBBNDL_NULLABLE *LIBBNDL_NONNULL import, size_t index);

	LIBBNDL_EXPORT libbndl_error libbndl_resource_replace_binary(libbndl_resource *LIBBNDL_NONNULL resource, const libbndl_buffer *LIBBNDL_NONNULL buffer, libbndl_memory_type memoryType);
	LIBBNDL_EXPORT void libbndl_resource_add_import(libbndl_resource *LIBBNDL_NONNULL resource, libbndl_import *LIBBNDL_NONNULL import);

	LIBBNDL_EXPORT libbndl_error libbndl_add_resource(libbndl_bundle *LIBBNDL_NONNULL bundle, libbndl_resource_id resourceID, const libbndl_resource *LIBBNDL_NONNULL resource, uint8_t streamIndex);

	LIBBNDL_EXPORT libbndl_error libbndl_replace_resource(libbndl_bundle *LIBBNDL_NONNULL bundle, libbndl_resource_id resourceID, const libbndl_resource *LIBBNDL_NONNULL resource, uint8_t streamIndex);


	/* Bundle helpers */
	LIBBNDL_EXPORT uint32_t libbndl_get_resource_count(const libbndl_bundle *LIBBNDL_NONNULL bundle);
	LIBBNDL_EXPORT libbndl_error libbndl_get_resource_id_at_index(const libbndl_bundle *LIBBNDL_NONNULL bundle, libbndl_resource_id *LIBBNDL_NONNULL resourceID, uint32_t index);

#	define LIBBNDL_STREAM_MAX_COUNT 4
	LIBBNDL_EXPORT bool libbndl_is_populated_resource_stream_index(const libbndl_bundle *LIBBNDL_NONNULL bundle, libbndl_resource_id resourceID, uint8_t streamIndex);

	LIBBNDL_EXPORT libbndl_resource_id libbndl_get_default_resource_id(const libbndl_bundle *LIBBNDL_NONNULL bundle);
	LIBBNDL_EXPORT int32_t libbndl_get_default_resource_stream_index(const libbndl_bundle *LIBBNDL_NONNULL bundle);

#	define LIBBNDL_STREAM_NAME_MAX_LENGTH 15
#	define LIBBNDL_STREAM_NAME_BUFFER_SIZE (LIBBNDL_STREAM_NAME_MAX_LENGTH + 1)
	LIBBNDL_EXPORT libbndl_error libbndl_get_stream_name(const libbndl_bundle *LIBBNDL_NONNULL bundle, char *LIBBNDL_NONNULL buffer, size_t length, uint8_t streamIndex);

	LIBBNDL_EXPORT bool libbndl_is_valid_memory_type(const libbndl_bundle *LIBBNDL_NONNULL bundle, libbndl_memory_type memoryType);
#ifdef __cplusplus
}
#endif
