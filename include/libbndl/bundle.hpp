#pragma once
#include <libbndl/internal/export.h>
#include <array>
#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <stdint.h>
#include <string>
#include <vector>
#include <version>

#if __cpp_lib_constexpr_memory >= 202202L
#	define LIBBNDL_BUFFER_CONSTEXPR constexpr
#else
#	define LIBBNDL_BUFFER_CONSTEXPR
#endif

#ifdef __cpp_lib_to_underlying
#	define LIBBNDL_TO_UNDERLYING(x) std::to_underlying(x)
#else
#	define LIBBNDL_TO_UNDERLYING(x) static_cast<std::underlying_type_t<std::remove_reference_t<decltype(x)>>>(x)
#endif

namespace libbndl
{
	namespace Formats { class Base; }

	enum class Magic : uint8_t
	{
#define LIBBNDL_ENUM_MAGIC(name, _, value) name = (value),
#include <libbndl/internal/enum.inc>
#undef LIBBNDL_ENUM_MAGIC
	};

	enum class Platform : uint16_t
	{
#define LIBBNDL_ENUM_PLATFORM(name, _, value) name = (value),
#include <libbndl/internal/enum.inc>
#undef LIBBNDL_ENUM_PLATFORM
	};

	namespace ResourceType
	{
		namespace Burnout
		{
			enum : uint32_t
			{
#define LIBBNDL_ENUM_RESOURCE_TYPE_BURNOUT(name, _, value) name = (value),
#include <libbndl/internal/enum.inc>
#undef LIBBNDL_ENUM_RESOURCE_TYPE_BURNOUT
			};
		}

		namespace NeedForSpeed
		{
			enum : uint32_t
			{
#define LIBBNDL_ENUM_RESOURCE_TYPE_NFS(name, _, value) name = (value),
#include <libbndl/internal/enum.inc>
#undef LIBBNDL_ENUM_RESOURCE_TYPE_NFS
			};
		}
	}

	enum class MemoryType : uint8_t
	{
#define LIBBNDL_ENUM_MEMORY_TYPE(name, _, value) name = (value),
#include <libbndl/internal/enum.inc>
#undef LIBBNDL_ENUM_MEMORY_TYPE
	};

	class Flags
	{
	private:
		using UnderlyingType = uint32_t;
		enum class Values : UnderlyingType
		{
#define LIBBNDL_ENUM_FLAGS(name, _, value) name = (value),
#include <libbndl/internal/enum.inc>
#undef LIBBNDL_ENUM_FLAGS
		};

	public:
		using enum Values;

		constexpr Flags() noexcept : m_value(0) {}
		constexpr Flags(Values flag) noexcept : m_value(LIBBNDL_TO_UNDERLYING(flag)) {}
		constexpr explicit Flags(UnderlyingType flags) noexcept : m_value(flags) {}

		[[nodiscard]] constexpr bool operator==(const Flags &flags) const noexcept = default;
		[[nodiscard]] constexpr bool operator==(UnderlyingType flags) const noexcept { return m_value == flags; };

		[[nodiscard]] constexpr Flags operator&(const Flags &rhs) const noexcept { return Flags(m_value & rhs.m_value); }
		[[nodiscard]] friend constexpr Flags operator&(const Values &lhs, const Flags &rhs) noexcept { return rhs & lhs; }
		[[nodiscard]] friend constexpr Flags operator&(Values lhs, Values rhs) noexcept { return Flags(lhs) & rhs; }
		[[nodiscard]] constexpr Flags operator|(const Flags &rhs) const noexcept { return Flags(m_value | rhs.m_value); }
		[[nodiscard]] friend constexpr Flags operator|(const Values &lhs, const Flags &rhs) noexcept { return rhs | lhs; }
		[[nodiscard]] friend constexpr Flags operator|(Values lhs, Values rhs) noexcept { return Flags(lhs) | rhs; }
		[[nodiscard]] constexpr Flags operator^(const Flags &rhs) const noexcept { return Flags(m_value ^ rhs.m_value); }
		[[nodiscard]] friend constexpr Flags operator^(const Values &lhs, const Flags &rhs) noexcept { return rhs ^ lhs; }
		[[nodiscard]] friend constexpr Flags operator^(Values lhs, Values rhs) noexcept { return Flags(lhs) ^ rhs; }
		[[nodiscard]] constexpr Flags operator~() const noexcept { return Flags(~m_value); }
		[[nodiscard]] friend constexpr Flags operator~(Values value) noexcept { return ~Flags(value); }

		constexpr Flags &operator|=(const Flags &rhs) noexcept
		{
			m_value |= rhs.m_value;
			return *this;
		}

		constexpr Flags &operator&=(const Flags &rhs) noexcept
		{
			m_value &= rhs.m_value;
			return *this;
		}

		constexpr Flags &operator^=(const Flags &rhs) noexcept
		{
			m_value ^= rhs.m_value;
			return *this;
		}

		[[nodiscard]] constexpr explicit operator bool() const noexcept { return !!m_value; }
		[[nodiscard]] constexpr explicit operator UnderlyingType() const noexcept { return m_value; }

	private:
		UnderlyingType m_value;
	};

	class ResourceID
	{
	private:
		using UnderlyingType = uint64_t;

	public:
		enum class IDType : uint8_t
		{
#define LIBBNDL_ENUM_ID_TYPE(name, _, value) name = (value),
#include <libbndl/internal/enum.inc>
#undef LIBBNDL_ENUM_ID_TYPE
		};

		constexpr ResourceID() noexcept : m_id(0) {}
		constexpr ResourceID(uint32_t id, uint16_t type, uint8_t index, IDType idType) noexcept
			: m_id(id | (static_cast<uint64_t>(type) << 32) | (static_cast<uint64_t>(index) << 48) || (static_cast<uint64_t>(idType) << 56)) {}
		LIBBNDL_EXPORT explicit ResourceID(std::string name) noexcept;
		constexpr explicit ResourceID(UnderlyingType id) noexcept : m_id(id) {}

		[[nodiscard]] constexpr bool operator==(const ResourceID &id) const noexcept = default;
		[[nodiscard]] constexpr auto operator<=>(const ResourceID &flags) const noexcept = default;
		[[nodiscard]] constexpr auto operator==(UnderlyingType id) const noexcept { return m_id == id; };

		[[nodiscard]] constexpr uint32_t GetGameChangerID() const noexcept { return m_id & 0xFFFFFFFF; }
		[[nodiscard]] constexpr uint32_t GetID32() const noexcept { return GetGameChangerID(); }
		[[nodiscard]] constexpr uint16_t GetResourceTypeID() const noexcept { return (m_id >> 32) & 0xFFFF; }
		[[nodiscard]] constexpr uint8_t GetIndex() const noexcept { return (m_id >> 48) & 0xFF; }
		[[nodiscard]] constexpr IDType GetIDType() const noexcept { return static_cast<IDType>((m_id >> 56) & 0xFF); }

		[[nodiscard]] constexpr explicit operator uint32_t() const noexcept { return GetGameChangerID(); }
		[[nodiscard]] constexpr explicit operator UnderlyingType() const noexcept { return m_id; }

	private:
		UnderlyingType m_id;
	};

	class ResourceDebugData
	{
	public:
		constexpr ResourceDebugData(std::string name, std::string typeName) noexcept : m_name(std::move(name)), m_typeName(std::move(typeName)) {}

		[[nodiscard]] constexpr std::string GetName() const { return m_name; }
		[[nodiscard]] constexpr std::string GetTypeName() const { return m_typeName; }

	private:
		std::string m_name;
		std::string m_typeName;
	};

	class Import
	{
	public:
		enum class ImportType : uint8_t
		{
#define LIBBNDL_ENUM_IMPORT_TYPE(name, _, value) name = (value),
#include <libbndl/internal/enum.inc>
#undef LIBBNDL_ENUM_IMPORT_TYPE
		};

		constexpr Import(ResourceID resourceID, uint32_t offset, ImportType type = ImportType::Pointer) noexcept
			: m_resourceID(resourceID), m_offset(offset | (static_cast<uint32_t>(type) << 31)) {}
		Import(std::string resourceName, uint32_t offset, ImportType type = ImportType::Pointer) noexcept
			: Import(ResourceID(std::move(resourceName)), offset, type) {}

		[[nodiscard]] constexpr ResourceID GetResourceID() const noexcept { return m_resourceID; }
		[[nodiscard]] constexpr uint32_t GetOffset() const noexcept { return m_offset & 0x7FFFFFFF; }
		[[nodiscard]] constexpr ImportType GetImportType() const noexcept { return static_cast<ImportType>((m_offset >> 31) & 1); }

	private:
		ResourceID m_resourceID;
		uint32_t m_offset;
	};

	class Buffer
	{
	public:
		using value_type = uint8_t;
		using size_type = size_t;
		using pointer = value_type *;
		using reference = value_type &;
		using iterator = value_type *;
		using const_iterator = const value_type *;

		constexpr Buffer() noexcept : m_ptr({}), m_size(0), m_alignment(0) {}
		LIBBNDL_BUFFER_CONSTEXPR Buffer(std::unique_ptr<value_type[]> ptr, size_type size, uint32_t alignment) noexcept : m_ptr(std::move(ptr)), m_size(size), m_alignment(alignment) {}
		constexpr Buffer(const Buffer &) = delete;
		constexpr Buffer &operator=(const Buffer &) = delete;
		LIBBNDL_BUFFER_CONSTEXPR Buffer(Buffer &&) noexcept = default;
		LIBBNDL_BUFFER_CONSTEXPR Buffer &operator=(Buffer &&buffer) noexcept
		{
			m_ptr = std::move(buffer.m_ptr);
			m_size = buffer.m_size;
			m_alignment = buffer.m_alignment;
			return *this;
		}
		LIBBNDL_BUFFER_CONSTEXPR ~Buffer() = default;

		[[nodiscard]] constexpr size_type GetSize() const noexcept { return m_size; }
		[[nodiscard]] constexpr uint32_t GetAlignment() const noexcept { return m_alignment; }
		[[nodiscard]] LIBBNDL_BUFFER_CONSTEXPR pointer GetData() const noexcept { return m_ptr.get(); }

		[[nodiscard]] LIBBNDL_BUFFER_CONSTEXPR iterator begin() const noexcept { return m_ptr.get(); }
		[[nodiscard]] LIBBNDL_BUFFER_CONSTEXPR iterator end() const noexcept { return m_ptr.get() + m_size; }
		[[nodiscard]] LIBBNDL_BUFFER_CONSTEXPR const_iterator cbegin() const noexcept { return begin(); }
		[[nodiscard]] LIBBNDL_BUFFER_CONSTEXPR const_iterator cend() const noexcept { return end(); }

		[[nodiscard]] LIBBNDL_BUFFER_CONSTEXPR bool operator==(std::nullptr_t) const noexcept { return m_ptr.get() == nullptr; }
		[[nodiscard]] LIBBNDL_BUFFER_CONSTEXPR reference operator[](size_type idx) const { return m_ptr[idx]; }

	private:
		std::unique_ptr<value_type[]> m_ptr;
		size_type m_size;
		uint32_t m_alignment;
	};

	class Resource
	{
	public:
		constexpr Resource(uint32_t resourceType) : m_buffers(), m_imports(), m_resourceType(resourceType) {}
		Resource(std::array<Buffer, 4> buffers, std::vector<Import> imports, uint32_t resourceType) : m_buffers(std::move(buffers)), m_imports(std::move(imports)), m_resourceType(resourceType) {}

		[[nodiscard]] constexpr Buffer &GetBinary(MemoryType block) { return m_buffers[LIBBNDL_TO_UNDERLYING(block)]; }
		[[nodiscard]] constexpr const Buffer &GetBinary(MemoryType block) const { return m_buffers[LIBBNDL_TO_UNDERLYING(block)]; }
		[[nodiscard]] constexpr std::vector<Import> GetImports() const { return m_imports; }
		[[nodiscard]] constexpr uint32_t GetResourceType() const noexcept { return m_resourceType; }

		void ReplaceBinary(MemoryType block, Buffer &&buffer) { m_buffers[LIBBNDL_TO_UNDERLYING(block)] = std::move(buffer); }
		void AddImport(Import import) { m_imports.emplace_back(import); }

	private:
		std::array<Buffer, 4> m_buffers;
		std::vector<Import> m_imports;
		uint32_t m_resourceType;
	};

	class Bundle
	{
	public:
		LIBBNDL_EXPORT Bundle();
		LIBBNDL_EXPORT Bundle(Magic magic, uint16_t version, Platform platform, Flags flags);
		Bundle(const Bundle &) = delete;
		Bundle &operator=(const Bundle &) = delete;
		LIBBNDL_EXPORT Bundle(Bundle &&) noexcept;
		LIBBNDL_EXPORT Bundle &operator=(Bundle &&) noexcept;
		LIBBNDL_EXPORT ~Bundle();

		LIBBNDL_EXPORT bool Load(const std::filesystem::path &name);
		LIBBNDL_EXPORT bool Save(const std::filesystem::path &name);

		[[nodiscard]] LIBBNDL_EXPORT Magic GetMagic() const;
		[[nodiscard]] LIBBNDL_EXPORT uint16_t GetVersion() const;
		[[nodiscard]] LIBBNDL_EXPORT Platform GetPlatform() const;
		[[nodiscard]] LIBBNDL_EXPORT Flags GetFlags() const;

		[[nodiscard]] LIBBNDL_EXPORT bool IsBurnoutEra() const;
		[[nodiscard]] LIBBNDL_EXPORT bool IsNeedForSpeedEra() const;

		[[nodiscard]] LIBBNDL_EXPORT std::optional<ResourceDebugData> GetResourceDebugData(ResourceID resourceID, uint8_t streamIndex = 0) const;
		[[nodiscard]] LIBBNDL_EXPORT std::optional<uint32_t> GetResourceType(ResourceID resourceID, uint8_t streamIndex = 0) const;
		[[nodiscard]] LIBBNDL_EXPORT std::optional<Resource> GetResource(ResourceID resourceID, uint8_t streamIndex = 0) const;
		[[nodiscard]] LIBBNDL_EXPORT Buffer GetBinary(ResourceID resourceID, MemoryType memoryType, uint8_t streamIndex = 0) const;

		LIBBNDL_EXPORT bool AddResource(ResourceID resourceID, const Resource &resource, uint8_t streamIndex = 0);
		LIBBNDL_EXPORT bool AddResourceDebugData(ResourceID resourceID, const ResourceDebugData &debugData, uint8_t streamIndex = 0);

		LIBBNDL_EXPORT bool ReplaceResource(ResourceID resourceID, const Resource &data, uint8_t streamIndex = 0);

		[[nodiscard]] LIBBNDL_EXPORT uint32_t GetResourceCount() const;
		[[nodiscard]] LIBBNDL_EXPORT std::vector<ResourceID> GetResourceIDs() const;
		[[nodiscard]] LIBBNDL_EXPORT std::map<uint32_t, std::vector<ResourceID>> GetResourceIDsByType() const;
		[[nodiscard]] LIBBNDL_EXPORT std::vector<uint8_t> GetResourceStreamIndices(ResourceID resourceID) const;

		[[nodiscard]] LIBBNDL_EXPORT ResourceID GetDefaultResourceID() const;
		[[nodiscard]] LIBBNDL_EXPORT int32_t GetDefaultResourceStreamIndex() const;
		[[nodiscard]] LIBBNDL_EXPORT std::string GetStreamName(uint8_t index) const;

		[[nodiscard]] LIBBNDL_EXPORT std::vector<MemoryType> GetMemoryTypes() const;

	private:
		std::unique_ptr<Formats::Base> m_impl;
	};
}
