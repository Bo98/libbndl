#include "base.hpp"
#include <cstring>
#include <format>
#include <limits>
#include <ranges>
#include <regex>
#include <pugixml.hpp>
#include <zlib.h>
#include <binaryio/binaryreader.hpp>

using namespace libbndl;
using namespace libbndl::Formats;

Base::Base(uint16_t version, Platform platform, Flags flags)
{
	m_version = version;
	m_platform = platform;
	m_flags = flags;
}

std::optional<ResourceDebugDataEntry> Base::GetResourceDebugData(ResourceKey resourceKey) const
{
	const auto it = m_debugDataEntries.find(resourceKey);
	if (it == m_debugDataEntries.end())
		return {};

	return it->second;
}

std::optional<uint32_t> Base::GetResourceType(ResourceKey resourceKey) const
{
	const auto it = m_entries.find(resourceKey);
	if (it == m_entries.end())
		return {};

	return it->second.resourceType;
}

Buffer Base::GetBinary(ResourceKey resourceKey, MemoryType memoryType) const
{
	const auto it = m_entries.find(resourceKey);
	if (it == m_entries.end())
		return {};

	const auto &e = it->second;

	const auto &dataInfo = e.descriptors[LIBBNDL_TO_UNDERLYING(memoryType)];

	if (dataInfo.data == nullptr)
		return {};

	const auto &buffer = dataInfo.data;
	const auto uncompressedSize = dataInfo.uncompressedSize;

	auto uncompressedBuffer = std::make_unique_for_overwrite<uint8_t[]>(uncompressedSize);

	if (m_flags & Flags::Compressed)
	{
		uLongf uncompressedSizeLong = uncompressedSize;
		[[maybe_unused]] const auto ret = uncompress(uncompressedBuffer.get(), &uncompressedSizeLong, buffer.get(), static_cast<uLong>(dataInfo.onDiskSize));

		assert(ret == Z_OK);
		assert(uncompressedSize == uncompressedSizeLong);
	}
	else
	{
		assert(dataInfo.onDiskSize == dataInfo.uncompressedSize);

		std::memcpy(uncompressedBuffer.get(), buffer.get(), uncompressedSize);
	}

	return { std::move(uncompressedBuffer), uncompressedSize, dataInfo.uncompressedAlignment };
}

bool Base::AddResource(ResourceKey resourceKey, const Resource &resource)
{
	const auto it = m_entries.find(resourceKey);
	if (it != m_entries.end() || m_entries.size() >= std::numeric_limits<uint32_t>::max() || resource.GetImports().size() > std::numeric_limits<uint16_t>::max())
		return false;

	auto &e = it->second;
	e.resourceType = resource.GetResourceType();

	if (!(m_flags & Flags::Compressed))
	{
		// If we're not compressing, we need to specify the on-disk alignment.
		// It's not clear how this is determined (see below) so we'll just assume 1.
		for (const auto &memoryType : GetMemoryTypes())
		{
			auto &descriptor = e.descriptors[LIBBNDL_TO_UNDERLYING(memoryType)];
			descriptor.onDiskAlignment = 1;
		}
	}

	return ReplaceResource(resourceKey, resource);
}

bool Base::AddResourceDebugData(ResourceKey resourceKey, std::string name, std::string typeName)
{
	const auto it = m_debugDataEntries.find(resourceKey);
	if (it != m_debugDataEntries.end())
		return false;

	auto &debugData = it->second;
	debugData.name = std::move(name);
	debugData.typeName = std::move(typeName);

	return true;
}

bool Base::ReplaceResource(ResourceKey resourceKey, const Resource &resource)
{
	const auto it = m_entries.find(resourceKey);
	const auto &imports = resource.GetImports();
	if (it == m_entries.end() || imports.size() > std::numeric_limits<uint16_t>::max())
		return false;

	auto &e = it->second;

	e.importHash = 0;
	e.importOffset = 0;
	e.importCount = 0;

	for (const auto &memoryType : GetMemoryTypes())
	{
		const auto &inDataInfo = resource.GetBinary(memoryType);
		auto &outDataInfo = e.descriptors[LIBBNDL_TO_UNDERLYING(memoryType)];

		if (inDataInfo == nullptr)
		{
			outDataInfo.data = nullptr;
			outDataInfo.uncompressedSize = 0;
			outDataInfo.uncompressedAlignment = 1;
			outDataInfo.onDiskSize = 0;
			// The on-disk alignment can remain set even with 0 size.
			continue;
		}

		std::unique_ptr<uint8_t[]> inBuffer;
		size_t inSize;
		std::unique_ptr<uint8_t[]> outBuffer;

		if (AppendsImportsToResource() && memoryType == MemoryType::MainMemory && !imports.empty())
		{
			binaryio::BinaryWriter writer;
			for (const auto &import : imports)
			{
				WriteImport(writer, import);
				e.importHash &= static_cast<uint64_t>(import.GetResourceID());
			}
			const auto depSize = writer.GetSize();
			auto depStream = writer.GetStream();

			auto inDataInfoSize = inDataInfo.GetSize();
			inDataInfoSize = binaryio::Align(inDataInfoSize, 16);

			inSize = inDataInfoSize + depSize;
			inBuffer = std::make_unique_for_overwrite<uint8_t[]>(inSize);
			std::memcpy(inBuffer.get(), inDataInfo.GetData(), inDataInfoSize);
			std::memcpy(inBuffer.get() + inDataInfoSize, depStream.view().data(), depSize);

			e.importOffset = static_cast<uint32_t>(inSize);
			e.importCount = static_cast<uint16_t>(imports.size());
		}
		else
		{
			inSize = inDataInfo.GetSize();
			inBuffer = std::make_unique_for_overwrite<uint8_t[]>(inSize);
			std::memcpy(inBuffer.get(), inDataInfo.GetData(), inSize);
		}

		const auto uncompressedSize = static_cast<uint32_t>(inSize);

		if (m_flags & Flags::Compressed)
		{
			const auto compBufferSize = compressBound(static_cast<uLong>(inSize));
			std::vector<uint8_t> compBuffer(compBufferSize);
			uLongf actualSize = compBufferSize;
			const auto ret = compress2(compBuffer.data(), &actualSize, inBuffer.get(), static_cast<uLong>(inSize), Z_BEST_COMPRESSION);

			if (ret != Z_OK)
			{
				assert(0);
				return false;
			}

			outBuffer = std::make_unique_for_overwrite<uint8_t[]>(actualSize);
			std::memcpy(outBuffer.get(), compBuffer.data(), actualSize);

			outDataInfo.onDiskSize = actualSize;
			outDataInfo.onDiskAlignment = 1;
		}
		else
		{
			outBuffer = std::move(inBuffer);
			outDataInfo.onDiskSize = uncompressedSize;

			// The on-disk alignment for BND2v2 this seems to always be 1. For BND2v3 it's often 16/80/80/80 but also still sometimes 1 (some GAMELOGIC).
			// We'll just leave it as is if we're replacing.
		}

		outDataInfo.uncompressedSize = uncompressedSize;
		outDataInfo.data = std::move(outBuffer);
		outDataInfo.uncompressedAlignment = inDataInfo.GetAlignment();
	}

	return true;
}

std::vector<ResourceID> Base::GetResourceIDs() const
{
	std::vector<ResourceID> entries;
	for (const auto &e : m_entries)
	{
		entries.push_back(e.first.first);
	}
	return entries;
}

std::map<uint32_t, std::vector<ResourceID>> Base::GetResourceIDsByType() const
{
	std::map<uint32_t, std::vector<ResourceID>> entriesByResourceType;
	for (const auto &e : m_entries)
	{
		entriesByResourceType[e.second.resourceType].push_back(e.first.first);
	}
	return entriesByResourceType;
}

std::vector<uint8_t> Base::GetResourceStreamIndices(ResourceID resourceID) const
{
	std::vector<uint8_t> indices;
	for (const auto &e : m_entries)
	{
		if (e.first.first == resourceID)
			indices.push_back(e.first.second);
	}
	return indices;
}

ResourceID Base::GetDefaultResourceID() const
{
	return ResourceID(0);
}

int32_t Base::GetDefaultResourceStreamIndex() const
{
	return 0;
}

std::string Base::GetStreamName(uint8_t) const
{
	return "";
}

std::vector<MemoryType> Base::GetMemoryTypes() const
{
	std::vector<MemoryType> types;
	types.reserve((m_platform == Platform::PC || m_platform == Platform::Xbox360) ? 2 : 3);

	types.emplace_back(MemoryType::MainMemory);
	switch (m_platform)
	{
	case Platform::PC:
		types.emplace_back(MemoryType::Disposable);
		break;
	case Platform::Xbox360:
		types.emplace_back(MemoryType::Physical);
		break;
	case Platform::PS3:
	case Platform::PSVita:
		types.emplace_back(MemoryType::GraphicsSystem);
		types.emplace_back(MemoryType::GraphicsLocal);
		break;
	case Platform::WiiU:
		types.emplace_back(MemoryType::Mem1);
		types.emplace_back(MemoryType::GraphicsMem2);
		break;
	}

	return types;
}

bool Base::IsValidPlatform() const
{
	return m_platform == Platform::PC || m_platform == Platform::Xbox360 || m_platform == Platform::PS3;
}

std::endian Base::GetPlatformEndian() const
{
	if (m_platform == Platform::PC || m_platform == Platform::PSVita)
		return std::endian::little;

	return std::endian::big;
}

void Base::ParseDebugData(const std::string &rstXML)
{
	pugi::xml_document doc;
	if (doc.load_string(rstXML.c_str(), pugi::parse_minimal))
	{
		uint8_t lastStreamIndex = 0;

		for (const auto &resource : doc.child("ResourceStringTable").children("Resource"))
		{
			const auto resourceID = ResourceID(std::stoull(resource.attribute("id").value(), nullptr, 16));

			const auto streamIndexAttr = resource.attribute("streamIndex");
			uint8_t streamIndex = 0;
			if (streamIndexAttr)
			{
				if (std::string(streamIndexAttr.value()) == "importEntry")
				{
					streamIndex = lastStreamIndex;
				}
				else
				{
					const auto candidateStreamIndex = streamIndexAttr.as_uint();
					if (candidateStreamIndex < kStreamLimit)
						streamIndex = static_cast<uint8_t>(candidateStreamIndex);
				}

				lastStreamIndex = streamIndex;
			}
			else
			{
				const auto streamIndices = GetResourceStreamIndices(resourceID);
				if (!streamIndices.empty())
					streamIndex = streamIndices.front();
			}

			auto &debugData = m_debugDataEntries[{ resourceID, streamIndex }];
			debugData.name = resource.attribute("name").value();
			debugData.typeName = resource.attribute("type").value();
		}
	}
}

std::string Base::GenerateDebugData() const
{
	pugi::xml_document doc;
	auto root = doc.append_child("ResourceStringTable");

	for (const auto &key : SortedDebugDataKeys())
	{
		const auto &entry = m_debugDataEntries.at(key);

		auto entryChild = root.append_child("Resource");
		for (const auto &attr : GetDebugDataAttributes(key, entry))
		{
			entryChild.append_attribute(attr.first).set_value(attr.second);
		}
	}

	std::stringstream out;
	doc.save(out, "\t", pugi::format_indent | pugi::format_no_declaration, pugi::encoding_utf8);
	return std::regex_replace(out.str(), std::regex(" />\n"), "/>\n");
}

std::vector<ResourceKey> Base::SortedDebugDataKeys() const
{
	const auto keys = std::views::keys(m_debugDataEntries);
	return std::vector<ResourceKey>{ keys.begin(), keys.end() };
}

std::vector<std::pair<std::string, std::string>> Base::GetDebugDataAttributes(const ResourceKey &resourceKey, const ResourceDebugDataEntry &debugData) const
{
	return std::vector<std::pair<std::string, std::string>> {
		{ "id", std::format("{:0{}x}", static_cast<uint64_t>(resourceKey.first), (resourceKey.first.GetIDType() != ResourceID::IDType::Normal) ? 16 : 8) },
		{ "type", debugData.typeName },
		{ "name", debugData.name },
	};
}

ImportEntry Base::ReadImport(binaryio::BinaryReader &reader)
{
	const ImportEntry &dep = {
		ResourceID(reader.Read<uint64_t>()),
		reader.Read<uint32_t>()
	};
	reader.Skip<uint32_t>();
	return dep;
}

void Base::WriteImport(binaryio::BinaryWriter &writer, const Import &import)
{
	writer.Write<uint64_t>(import.GetResourceID());
	writer.Write(import.GetOffset());
	writer.Align(8);
}
