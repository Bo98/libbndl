#include <libbndl/bundle.h>
#include <algorithm>
#include <cassert>
#include <cstring>
#include <new>

using namespace libbndl;

struct libbndl_bundle : public Bundle
{
	using Bundle::Bundle;
};

libbndl_error libbndl_load(libbndl_bundle *LIBBNDL_NULLABLE *LIBBNDL_NONNULL bundle, const char *LIBBNDL_NONNULL path)
{
	assert(bundle != nullptr);
	assert(path != nullptr);

	*bundle = new (std::nothrow) libbndl_bundle;
	if (*bundle == nullptr)
		return LIBBNDL_ERROR_MEMORY_ALLOCATION;

	if (!(*bundle)->Load(path))
	{
		delete *bundle;
		return LIBBNDL_ERROR_INVALID_BUNDLE;
	}

	return LIBBNDL_ERROR_SUCCESS;
}

libbndl_error libbndl_create(libbndl_bundle *LIBBNDL_NULLABLE *LIBBNDL_NONNULL bundle, libbndl_magic magic, uint16_t version, libbndl_platform platform, libbndl_flags flags)
{
	assert(bundle != nullptr);
	assert(magic == LIBBNDL_MAGIC_BNDL || magic == LIBBNDL_MAGIC_BND2);

	*bundle = new (std::nothrow) libbndl_bundle(static_cast<Magic>(magic), version, static_cast<Platform>(platform), Flags(flags));
	if (*bundle == nullptr)
		return LIBBNDL_ERROR_MEMORY_ALLOCATION;

	return LIBBNDL_ERROR_SUCCESS;
}

void libbndl_free(libbndl_bundle *LIBBNDL_NULLABLE bundle)
{
	delete bundle;
}

libbndl_error libbndl_save(libbndl_bundle *LIBBNDL_NONNULL bundle, const char *LIBBNDL_NONNULL path)
{
	if (!bundle->Save(path))
		return LIBBNDL_ERROR_GENERIC_FAILURE;

	return LIBBNDL_ERROR_SUCCESS;
}

libbndl_magic libbndl_get_magic(const libbndl_bundle *LIBBNDL_NONNULL bundle)
{
	return static_cast<libbndl_magic>(bundle->GetMagic());
}

uint16_t libbndl_get_version(const libbndl_bundle *LIBBNDL_NONNULL bundle)
{
	return bundle->GetVersion();
}

libbndl_platform libbndl_get_platform(const libbndl_bundle *LIBBNDL_NONNULL bundle)
{
	return static_cast<libbndl_platform>(bundle->GetPlatform());
}

libbndl_flags libbndl_get_flags(const libbndl_bundle *LIBBNDL_NONNULL bundle)
{
	return static_cast<libbndl_flags>(bundle->GetFlags());
}

bool libbndl_is_burnout_era(const libbndl_bundle *LIBBNDL_NONNULL bundle)
{
	return bundle->IsBurnoutEra();
}

bool libbndl_is_need_for_speed_era(const libbndl_bundle *LIBBNDL_NONNULL bundle)
{
	return bundle->IsNeedForSpeedEra();
}


/* Resource ID */
libbndl_resource_id libbndl_resource_id_from_name(const char *LIBBNDL_NONNULL name)
{
	return static_cast<libbndl_resource_id>(ResourceID(name));
}

libbndl_resource_id libbndl_resource_id_from_game_changer(uint32_t id, uint16_t resourceType, uint8_t index)
{
	return static_cast<libbndl_resource_id>(ResourceID(id, resourceType, index, ResourceID::IDType::GameChanger));
}

libbndl_resource_id libbndl_resource_id_from_components(uint32_t id, uint16_t resourceType, uint8_t index, libbndl_resource_id_type idType)
{
	return static_cast<libbndl_resource_id>(ResourceID(id, resourceType, index, static_cast<ResourceID::IDType>(idType)));
}

uint32_t libbndl_resource_id_get_game_changer_id(libbndl_resource_id resourceID)
{
	return ResourceID(resourceID).GetGameChangerID();
}

uint16_t libbndl_resource_id_get_resource_type_id(libbndl_resource_id resourceID)
{
	return ResourceID(resourceID).GetResourceTypeID();
}

uint8_t libbndl_resource_id_get_index(libbndl_resource_id resourceID)
{
	return ResourceID(resourceID).GetIndex();
}

libbndl_resource_id_type libbndl_resource_id_get_id_type(libbndl_resource_id resourceID)
{
	return static_cast<libbndl_resource_id_type>(ResourceID(resourceID).GetIDType());
}


/* Resource debug data */
struct libbndl_resource_debug_data : public ResourceDebugData
{
	using ResourceDebugData::ResourceDebugData;
	constexpr libbndl_resource_debug_data(ResourceDebugData &&debugData) : ResourceDebugData(std::move(debugData)) {}
};

libbndl_error libbndl_get_resource_debug_data(const libbndl_bundle *LIBBNDL_NONNULL bundle, libbndl_resource_debug_data *LIBBNDL_NULLABLE *LIBBNDL_NONNULL debugData, libbndl_resource_id resourceID, uint8_t streamIndex)
{
	assert(debugData != nullptr);

	auto internalDebugData = bundle->GetResourceDebugData(ResourceID(resourceID), streamIndex);
	if (!internalDebugData)
		return LIBBNDL_ERROR_DEBUG_DATA_NOT_FOUND;

	*debugData = new (std::nothrow) libbndl_resource_debug_data(*std::move(internalDebugData));
	if (*debugData == nullptr)
		return LIBBNDL_ERROR_MEMORY_ALLOCATION;

	return LIBBNDL_ERROR_SUCCESS;
}

libbndl_error libbndl_resource_debug_data_create(libbndl_resource_debug_data *LIBBNDL_NULLABLE *LIBBNDL_NONNULL debugData, const char *LIBBNDL_NONNULL name, const char *LIBBNDL_NONNULL typeName)
{
	assert(name != nullptr);
	assert(typeName != nullptr);

	*debugData = new (std::nothrow) libbndl_resource_debug_data(name, typeName);
	if (*debugData == nullptr)
		return LIBBNDL_ERROR_MEMORY_ALLOCATION;

	return LIBBNDL_ERROR_SUCCESS;
}

void libbndl_resource_debug_data_free(libbndl_resource_debug_data *LIBBNDL_NULLABLE debugData)
{
	delete debugData;
}

libbndl_error libbndl_resource_debug_data_get_name(const libbndl_resource_debug_data *LIBBNDL_NONNULL debugData, char *LIBBNDL_NONNULL buffer, size_t length)
{
	if (length == 0)
		return LIBBNDL_ERROR_SUCCESS;

	const auto name = debugData->GetName();
	const auto size = name.size();

	if (size >= length)
		return LIBBNDL_ERROR_INSUFFICIENT_BUFFER;

	std::memcpy(buffer, name.data(), size);
	buffer[size] = '\0';

	return LIBBNDL_ERROR_SUCCESS;
}

libbndl_error libbndl_resource_debug_data_get_type_name(const libbndl_resource_debug_data *LIBBNDL_NONNULL debugData, char *LIBBNDL_NONNULL buffer, size_t length)
{
	if (length == 0)
		return LIBBNDL_ERROR_SUCCESS;

	const auto typeName = debugData->GetTypeName();
	const auto size = typeName.size();

	if (size >= length)
		return LIBBNDL_ERROR_INSUFFICIENT_BUFFER;

	std::memcpy(buffer, typeName.data(), size);
	buffer[size] = '\0';

	return LIBBNDL_ERROR_SUCCESS;
}

void libbndl_add_resource_debug_data(libbndl_bundle *LIBBNDL_NONNULL bundle, libbndl_resource_id resourceID, const libbndl_resource_debug_data *LIBBNDL_NONNULL debugData, uint8_t streamIndex)
{
	assert(debugData != nullptr);

	bundle->AddResourceDebugData(ResourceID(resourceID), *debugData, streamIndex);
}


/* Resource type */
libbndl_error libbndl_get_resource_type(const libbndl_bundle *LIBBNDL_NONNULL bundle, libbndl_resource_type *LIBBNDL_NONNULL resourceType, libbndl_resource_id resourceID, uint8_t streamIndex)
{
	assert(resourceType != nullptr);

	const auto internalResourceType = bundle->GetResourceType(ResourceID(resourceID), streamIndex);
	if (!internalResourceType)
		return LIBBNDL_ERROR_RESOURCE_NOT_FOUND;

	*resourceType = *internalResourceType;

	return LIBBNDL_ERROR_SUCCESS;
}


/* Buffer */
struct libbndl_buffer : public Buffer
{
	using Buffer::Buffer;
	LIBBNDL_BUFFER_CONSTEXPR libbndl_buffer(Buffer &&buffer) : Buffer(std::move(buffer)) {}
};

libbndl_error libbndl_buffer_create(libbndl_buffer *LIBBNDL_NULLABLE *LIBBNDL_NONNULL buffer, const void *LIBBNDL_NONNULL data, size_t size, uint32_t alignment)
{
	auto internalPtr = new (std::nothrow) uint8_t[size];
	if (internalPtr == nullptr)
		return LIBBNDL_ERROR_MEMORY_ALLOCATION;

	std::memcpy(internalPtr, data, size);

	std::unique_ptr<uint8_t[]> internalData(internalPtr);

	*buffer = new (std::nothrow) libbndl_buffer(std::move(internalData), size, alignment);
	if (*buffer == nullptr)
		return LIBBNDL_ERROR_MEMORY_ALLOCATION;

	return LIBBNDL_ERROR_SUCCESS;
}

void libbndl_buffer_free(libbndl_buffer *LIBBNDL_NULLABLE buffer)
{
	delete buffer;
}

libbndl_error libbndl_copy_binary(const libbndl_bundle *LIBBNDL_NONNULL bundle, libbndl_buffer *LIBBNDL_NULLABLE *LIBBNDL_NONNULL buffer, libbndl_resource_id resourceID, libbndl_memory_type memoryType, uint8_t streamIndex)
{
	assert(buffer != nullptr);

	// TODO: consider handling errors better here.

	auto binary = bundle->GetBinary(ResourceID(resourceID), static_cast<MemoryType>(memoryType), streamIndex);
	*buffer = new libbndl_buffer(std::move(binary));
	if (*buffer == nullptr)
		return LIBBNDL_ERROR_MEMORY_ALLOCATION;

	return LIBBNDL_ERROR_SUCCESS;
}

void *libbndl_buffer_get_data(const libbndl_buffer *LIBBNDL_NONNULL buffer)
{
	return buffer->GetData();
}

size_t libbndl_buffer_get_size(const libbndl_buffer *LIBBNDL_NONNULL buffer)
{
	return buffer->GetSize();
}

uint32_t libbndl_buffer_get_alignment(const libbndl_buffer *LIBBNDL_NONNULL buffer)
{
	return buffer->GetAlignment();
}


/* Import */
struct libbndl_import : public Import
{
	using Import::Import;
	constexpr libbndl_import(const Import &import) : Import(import) {}
};

libbndl_error libbndl_import_create(libbndl_import *LIBBNDL_NULLABLE *LIBBNDL_NONNULL import, libbndl_resource_id resourceID, uint32_t offset, libbndl_import_type importType)
{
	*import = new (std::nothrow) libbndl_import(ResourceID(resourceID), offset, static_cast<Import::ImportType>(importType));
	if (*import == nullptr)
		return LIBBNDL_ERROR_MEMORY_ALLOCATION;

	return LIBBNDL_ERROR_SUCCESS;
}

void libbndl_import_free(libbndl_import *LIBBNDL_NULLABLE import)
{
	delete import;
}

libbndl_resource_id libbndl_import_get_resource_id(const libbndl_import *LIBBNDL_NONNULL import)
{
	return static_cast<libbndl_resource_id>(import->GetResourceID());
}

uint32_t libbndl_import_get_offset(const libbndl_import *LIBBNDL_NONNULL import)
{
	return import->GetOffset();
}

libbndl_import_type libbndl_import_get_import_type(const libbndl_import *LIBBNDL_NONNULL import)
{
	return static_cast<libbndl_import_type>(import->GetImportType());
}


/* Resource */
struct libbndl_resource : public Resource
{
	using Resource::Resource;
	libbndl_resource(Resource &&resource) : Resource(std::move(resource)) {}
};

libbndl_error libbndl_resource_create(libbndl_resource *LIBBNDL_NULLABLE *LIBBNDL_NONNULL resource, libbndl_resource_type resourceType)
{
	*resource = new (std::nothrow) libbndl_resource(resourceType);
	if (*resource == nullptr)
		return LIBBNDL_ERROR_MEMORY_ALLOCATION;

	return LIBBNDL_ERROR_SUCCESS;
}

void libbndl_resource_free(libbndl_resource *LIBBNDL_NULLABLE resource)
{
	delete resource;
}

libbndl_error libbndl_copy_resource(const libbndl_bundle *LIBBNDL_NONNULL bundle, libbndl_resource *LIBBNDL_NULLABLE *LIBBNDL_NONNULL resource, libbndl_resource_id resourceID, uint8_t streamIndex)
{
	assert(resource != nullptr);

	auto internalResource = bundle->GetResource(ResourceID(resourceID), streamIndex);
	if (!internalResource)
		return LIBBNDL_ERROR_RESOURCE_NOT_FOUND;

	*resource = new (std::nothrow) libbndl_resource(*std::move(internalResource));
	if (*resource == nullptr)
		return LIBBNDL_ERROR_MEMORY_ALLOCATION;

	return LIBBNDL_ERROR_SUCCESS;
}

libbndl_error libbndl_resource_get_binary_mut(libbndl_resource *LIBBNDL_NONNULL resource, libbndl_buffer *LIBBNDL_NULLABLE *LIBBNDL_NONNULL buffer, libbndl_memory_type memoryType)
{
	assert(buffer != nullptr);

	if (memoryType >= 4)
		return LIBBNDL_ERROR_OUT_OF_RANGE;

	*buffer = &reinterpret_cast<libbndl_buffer &>(resource->GetBinary(static_cast<libbndl::MemoryType>(memoryType)));
	return LIBBNDL_ERROR_SUCCESS;
}

libbndl_error libbndl_resource_get_binary_const(const libbndl_resource *LIBBNDL_NONNULL resource, const libbndl_buffer *LIBBNDL_NULLABLE *LIBBNDL_NONNULL buffer, libbndl_memory_type memoryType)
{
	assert(buffer != nullptr);

	if (memoryType >= 4)
		return LIBBNDL_ERROR_OUT_OF_RANGE;

	*buffer = &reinterpret_cast<const libbndl_buffer &>(resource->GetBinary(static_cast<libbndl::MemoryType>(memoryType)));
	return LIBBNDL_ERROR_SUCCESS;
}

size_t libbndl_resource_get_import_count(const libbndl_resource *LIBBNDL_NONNULL resource)
{
	return resource->GetImports().size();
}

libbndl_error libbndl_resource_copy_import(const libbndl_resource *LIBBNDL_NONNULL resource, libbndl_import *LIBBNDL_NULLABLE *LIBBNDL_NONNULL import, size_t index)
{
	assert(import != nullptr);

	auto imports = resource->GetImports();
	if (index >= imports.size())
		return LIBBNDL_ERROR_OUT_OF_RANGE;

	*import = new libbndl_import(imports[index]);
	if (*import == nullptr)
		return LIBBNDL_ERROR_MEMORY_ALLOCATION;

	return LIBBNDL_ERROR_SUCCESS;
}

libbndl_error libbndl_resource_replace_binary(libbndl_resource *LIBBNDL_NONNULL resource, const libbndl_buffer *LIBBNDL_NONNULL buffer, libbndl_memory_type memoryType)
{
	assert(buffer != nullptr);

	auto size = buffer->GetSize();
	auto newData = new (std::nothrow) uint8_t[size];
	if (newData == nullptr)
		return LIBBNDL_ERROR_MEMORY_ALLOCATION;

	std::memcpy(newData, buffer->GetData(), size);

	std::unique_ptr<uint8_t[]> newPtr(newData);
	Buffer newBuffer(std::move(newPtr), size, buffer->GetAlignment());
	resource->ReplaceBinary(static_cast<MemoryType>(memoryType), std::move(newBuffer));

	return LIBBNDL_ERROR_SUCCESS;
}

void libbndl_resource_add_import(libbndl_resource *LIBBNDL_NONNULL resource, libbndl_import *LIBBNDL_NONNULL import)
{
	assert(import != nullptr);

	resource->AddImport(*import);
}

libbndl_error libbndl_add_resource(libbndl_bundle *LIBBNDL_NONNULL bundle, libbndl_resource_id resourceID, const libbndl_resource *LIBBNDL_NONNULL resource, uint8_t streamIndex)
{
	assert(resource != nullptr);

	if (streamIndex >= LIBBNDL_STREAM_MAX_COUNT)
		return LIBBNDL_ERROR_OUT_OF_RANGE;

	if (!bundle->AddResource(ResourceID(resourceID), *resource, streamIndex))
		return LIBBNDL_ERROR_GENERIC_FAILURE;

	return LIBBNDL_ERROR_SUCCESS;
}

libbndl_error libbndl_replace_resource(libbndl_bundle *LIBBNDL_NONNULL bundle, libbndl_resource_id resourceID, const libbndl_resource *LIBBNDL_NONNULL resource, uint8_t streamIndex)
{
	assert(resource != nullptr);

	if (streamIndex >= LIBBNDL_STREAM_MAX_COUNT)
		return LIBBNDL_ERROR_OUT_OF_RANGE;

	if (!bundle->ReplaceResource(ResourceID(resourceID), *resource, streamIndex))
		return LIBBNDL_ERROR_GENERIC_FAILURE;

	return LIBBNDL_ERROR_SUCCESS;
}


/* Bundle helpers */
uint32_t libbndl_get_resource_count(const libbndl_bundle *LIBBNDL_NONNULL bundle)
{
	return bundle->GetResourceCount();
}

libbndl_error libbndl_get_resource_id_at_index(const libbndl_bundle *LIBBNDL_NONNULL bundle, libbndl_resource_id *LIBBNDL_NONNULL resourceID, uint32_t index)
{
	const auto resourceIDs = bundle->GetResourceIDs();
	if (index >= resourceIDs.size())
		return LIBBNDL_ERROR_OUT_OF_RANGE;

	*resourceID = static_cast<libbndl_resource_id>(resourceIDs[index]);
	return LIBBNDL_ERROR_SUCCESS;
}

bool libbndl_is_populated_resource_stream_index(const libbndl_bundle *LIBBNDL_NONNULL bundle, libbndl_resource_id resourceID, uint8_t streamIndex)
{
	const auto indices = bundle->GetResourceStreamIndices(ResourceID(resourceID));
	return std::ranges::find(indices, streamIndex) != indices.end();
}

libbndl_resource_id libbndl_get_default_resource_id(const libbndl_bundle *LIBBNDL_NONNULL bundle)
{
	return static_cast<libbndl_resource_id>(bundle->GetDefaultResourceID());
}

int32_t libbndl_get_default_resource_stream_index(const libbndl_bundle *LIBBNDL_NONNULL bundle)
{
	return bundle->GetDefaultResourceStreamIndex();
}

libbndl_error libbndl_get_stream_name(const libbndl_bundle *LIBBNDL_NONNULL bundle, char *LIBBNDL_NONNULL buffer, size_t length, uint8_t streamIndex)
{
	if (length == 0)
		return LIBBNDL_ERROR_SUCCESS;

	const auto streamName = bundle->GetStreamName(streamIndex);
	const auto size = streamName.size();

	if (size >= length)
		return LIBBNDL_ERROR_INSUFFICIENT_BUFFER;

	std::memcpy(buffer, streamName.data(), size);
	buffer[size] = '\0';

	return LIBBNDL_ERROR_SUCCESS;
}

bool libbndl_is_valid_memory_type(const libbndl_bundle *LIBBNDL_NONNULL bundle, libbndl_memory_type memoryType)
{
	const auto memoryTypes = bundle->GetMemoryTypes();
	return std::ranges::find(memoryTypes, static_cast<MemoryType>(memoryType)) != memoryTypes.end();
}
