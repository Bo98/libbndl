#pragma once
#include <libbndl/bundle.hpp>
#include <binaryio/binaryreader.hpp>
#include <binaryio/binarywriter.hpp>
#include <array>
#include <map>
#include <memory>
#include <optional>
#include <stdint.h>
#include <string>
#include <utility>
#include <vector>

#ifndef __has_builtin
#	define __has_builtin(x) 0
#endif

namespace libbndl::Formats
{
	inline unsigned long BitScanReverse(unsigned long input)
	{
		unsigned long result;

#if defined(_MSC_VER)
		_BitScanReverse(&result, input);
#elif __has_builtin(__builtin_clzl) || defined(__GNUC__)
		result = static_cast<unsigned long>(std::numeric_limits<unsigned long>::digits - 1 - __builtin_clzl(input));
#else
		result = std::bit_width(input | 1U) - 1;
#endif

		return result;
	}

	using ResourceKey = std::pair<ResourceID, uint8_t>;

	struct ResourceData
	{
		uint32_t uncompressedSize;
		uint32_t uncompressedAlignment; // default depending on file type
		uint32_t onDiskSize;
		uint32_t onDiskAlignment;
		std::unique_ptr<uint8_t[]> data;
	};

	struct ResourceEntry
	{
		std::array<ResourceData, 4> descriptors;

		uint64_t importHash;

		uint32_t importOffset;
		uint32_t resourceType;
		uint16_t importCount;
	};

	struct ResourceDebugDataEntry
	{
		std::string name;
		std::string typeName;
	};

	struct ImportEntry
	{
		ResourceID resourceID;
		uint32_t offset;
	};

	class Base
	{
	public:
		Base() = default;
		Base(uint16_t version, Platform platform, Flags flags);
		Base(const Base &) = delete;
		Base &operator=(const Base &) = delete;
		Base(Base &&) = delete;
		Base &operator=(Base &&) = delete;
		virtual ~Base() = default;

		virtual bool Load(binaryio::BinaryReader &reader) = 0;
		virtual bool Save(binaryio::BinaryWriter &reader) = 0;

		[[nodiscard]] virtual constexpr Magic GetMagic() const = 0;
		[[nodiscard]] constexpr uint16_t GetVersion() const { return m_version; }
		[[nodiscard]] constexpr Platform GetPlatform() const { return m_platform; }
		[[nodiscard]] constexpr Flags GetFlags() const { return m_flags; }

		[[nodiscard]] std::optional<ResourceDebugDataEntry> GetResourceDebugData(ResourceKey resourceKey) const;
		[[nodiscard]] std::optional<uint32_t> GetResourceType(ResourceKey resourceKey) const;
		[[nodiscard]] virtual std::optional<Resource> GetResource(ResourceKey resourceKey) const = 0;
		[[nodiscard]] Buffer GetBinary(ResourceKey resourceKey, MemoryType memoryType) const;

		bool AddResource(ResourceKey resourceKey, const Resource &data);
		bool AddResourceDebugData(ResourceKey resourceID, std::string name, std::string typeName);

		bool ReplaceResource(ResourceKey resourceKey, const Resource &data);

		[[nodiscard]] uint32_t GetResourceCount() const { return static_cast<uint32_t>(m_entries.size()); }
		[[nodiscard]] std::vector<ResourceID> GetResourceIDs() const;
		[[nodiscard]] std::map<uint32_t, std::vector<ResourceID>> GetResourceIDsByType() const;
		[[nodiscard]] std::vector<uint8_t> GetResourceStreamIndices(ResourceID resourceID) const;

		[[nodiscard]] virtual ResourceID GetDefaultResourceID() const;
		[[nodiscard]] virtual int32_t GetDefaultResourceStreamIndex() const;
		[[nodiscard]] virtual std::string GetStreamName(uint8_t index) const;

		[[nodiscard]] virtual std::vector<MemoryType> GetMemoryTypes() const;

	protected:
		static constexpr const uint8_t kStreamLimit = 4;

		std::map<ResourceKey, ResourceEntry> m_entries;
		std::map<ResourceKey, ResourceDebugDataEntry> m_debugDataEntries;

		uint16_t m_version;
		Platform m_platform;
		Flags m_flags;

		[[nodiscard]] virtual constexpr bool AppendsImportsToResource() const = 0;
		[[nodiscard]] virtual bool IsValidPlatform() const;

		[[nodiscard]] std::endian GetPlatformEndian() const;

		void ParseDebugData(const std::string &rstXML);
		[[nodiscard]] std::string GenerateDebugData() const;
		[[nodiscard]] virtual std::vector<ResourceKey> SortedDebugDataKeys() const;
		[[nodiscard]] virtual std::vector<std::pair<std::string, std::string>> GetDebugDataAttributes(const ResourceKey &resourceKey, const ResourceDebugDataEntry &debugData) const;

		[[nodiscard]] static ImportEntry ReadImport(binaryio::BinaryReader &reader);
		static void WriteImport(binaryio::BinaryWriter &writer, const Import &import);
	};
}
