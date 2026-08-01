#include "bnd2.hpp"
#include <algorithm>
#include <cstring>
#include <format>
#include <limits>
#include <ranges>

using namespace libbndl;
using namespace libbndl::Formats;

bool Bnd2::Load(binaryio::BinaryReader &reader)
{
	auto version = reader.Read<uint32_t>();
	if ((version & 0xFF) == 0)
	{
		reader.SwapEndian();
		reader.Seek(-4, std::ios::cur);
		version = reader.Read<uint32_t>();
	}
	if (version > std::numeric_limits<uint16_t>::max())
	{
		reader.Seek(-4, std::ios::cur);
		version = reader.Read<uint16_t>();
	}
	m_version = static_cast<uint16_t>(version);
	if (m_version != 2 && m_version != 3 && m_version != 5)
		return false;

	if (m_version >= 5)
	{
		m_platform = reader.Read<Platform>();

		// Swap to older order
		if (m_platform == Platform::Xbox360)
			m_platform = Platform::PS3;
		else if (m_platform == Platform::PS3)
			m_platform = Platform::Xbox360;
	}
	else
	{
		m_platform = static_cast<Platform>(reader.Read<uint32_t>());
	}
	if (!IsValidPlatform())
		return false;

	const auto rstOffset = reader.Read<uint32_t>();
	const auto numEntries = reader.Read<uint32_t>();

	const auto idBlockOffset = reader.Read<uint32_t>();
	const auto blocks = (m_version >= 3) ? 4 : 3;
	std::array<uint32_t, 4> fileBlockOffsets = {
		reader.Read<uint32_t>(),
		reader.Read<uint32_t>(),
		reader.Read<uint32_t>(),
		(blocks == 4) ? reader.Read<uint32_t>() : 0,
	};

	if (fileBlockOffsets[blocks - 1] > reader.GetBuffer().size())
		return false;

	m_flags = Flags(reader.Read<uint32_t>());
	if (m_version >= 5)
	{
		m_flags = (m_flags & Flags::Compressed) | Flags(static_cast<uint32_t>(m_flags & ~Flags::Compressed) << 2);

		m_defaultResourceID = ResourceID(reader.Read<uint64_t>());
		m_defaultResourceStreamIndex = reader.Read<int32_t>();
		for (auto i = 0; i < kStreamLimit; i++)
		{
			m_streamNames[i] = reader.ReadString(15);

			const auto nullIdx = m_streamNames[i].find('\0');
			if (nullIdx != std::string::npos)
				m_streamNames[i].erase(nullIdx);
		}
	}


	m_entries.clear();
	m_debugDataEntries.clear();

	reader.Seek(idBlockOffset);
	for (auto i = 0U; i < numEntries; i++)
	{
		const auto resourceID = ResourceID(reader.Read<uint64_t>());
		assert(resourceID != 0);

		ResourceEntry e;

		if (m_version < 5)
			e.importHash = reader.Read<uint64_t>();

		for (auto j = 0; j < blocks; j++)
		{
			const auto uncompSize = reader.Read<uint32_t>();
			e.descriptors[j].uncompressedSize = uncompSize & ~(0xFU << 28);
			e.descriptors[j].uncompressedAlignment = 1 << (uncompSize >> 28);
		}

		for (auto j = 0; j < blocks; j++)
		{
			const auto onDiskSize = reader.Read<uint32_t>();
			e.descriptors[j].onDiskSize = onDiskSize & ~(0xFU << 28);
			e.descriptors[j].onDiskAlignment = 1 << (onDiskSize >> 28);
		}

		auto dataReader = reader;
		for (auto j = 0; j < blocks; j++)
		{
			dataReader.Seek(fileBlockOffsets[j] + reader.Read<uint32_t>()); // Read offset

			auto &descriptor = e.descriptors[j];

			if (descriptor.onDiskSize == 0)
			{
				descriptor.data = nullptr;
				continue;
			}

			descriptor.data = std::unique_ptr<uint8_t[]>(dataReader.Read<uint8_t *>(descriptor.onDiskSize));
		}

		e.importOffset = reader.Read<uint32_t>();
		e.resourceType = reader.Read<uint32_t>();
		e.importCount = reader.Read<uint16_t>();

		reader.Verify<uint8_t>(0); // flags
		
		const auto streamIndex = reader.Read<uint8_t>();
		assert(streamIndex == 0 || m_flags & Flags::MultistreamBundle);
		m_entries.emplace(std::make_pair(resourceID, streamIndex), std::move(e));

		reader.Align(8);
	}

	if (m_flags & Flags::HasDebugData)
	{
		reader.Seek(rstOffset, std::ios::beg);
		ParseDebugData(reader.ReadString());
	}

	return true;
};

bool Bnd2::Save(binaryio::BinaryWriter &writer)
{
	if (m_version != 2 && m_version != 3 && m_version != 5)
		return false;

	// For version 2, only the first 4 flags are supported. 7 for version 3.
	if (m_version == 2 && BitScanReverse(static_cast<uint32_t>(m_flags)) >= 4)
		return false;
	else if (m_version == 3 && BitScanReverse(static_cast<uint32_t>(m_flags)) >= 7)
		return false;
	else if (m_version == 5 && (m_flags & (Flags::MainMemOptimised | Flags::GraphicsMemOptimised)))
		return false;

	if (!IsValidPlatform())
		return false;

	writer.Write("bnd2", 4);
	writer.SetEndian(GetPlatformEndian());

	if (m_version >= 5)
	{
		auto platform = m_platform;

		// Swap to newer order
		if (platform == Platform::Xbox360)
			platform = Platform::PS3;
		else if (platform == Platform::PS3)
			platform = Platform::Xbox360;

		writer.Write<uint16_t>(m_version);
		writer.Write<uint16_t>(platform);
	}
	else
	{
		writer.Write<uint32_t>(m_version);
		writer.Write<uint32_t>(m_platform);
	}

	auto rstPointerPos = writer.GetOffset();
	writer.Seek(4, std::ios::cur); // write later

	writer.Write(static_cast<uint32_t>(m_entries.size()));

	const uint8_t blocks = (m_version >= 3) ? 4 : 3;

	auto idBlockPointerPos = writer.GetOffset();
	writer.Seek(4, std::ios::cur); // write later
	std::array<size_t, 4> fileBlockPointerPos;
	for (auto i = 0; i < blocks; i++)
	{
		fileBlockPointerPos[i] = writer.GetOffset();
		writer.Seek(4, std::ios::cur);
	}

	auto flags = static_cast<uint32_t>(m_flags);
	if (m_version >= 5)
		flags = (flags & 1) | ((flags >> 2) & ~1);
	writer.Write<uint32_t>(flags);

	if (m_version >= 5)
	{
		writer.Write<uint64_t>(m_defaultResourceID);
		writer.Write(m_defaultResourceStreamIndex);

		char buffer[15];
		for (auto i = 0; i < kStreamLimit; i++)
		{
			std::memset(buffer, 0, 15);
			std::memcpy(buffer, m_streamNames[i].c_str(), std::min(m_streamNames[i].size(), static_cast<size_t>(15)));
			writer.Write(buffer, 15);
		}
	}

	writer.Align(16);


	// RESOURCE STRING TABLE (version == 2)
	if (m_version == 2)
	{
		writer.VisitAndWrite<uint32_t>(rstPointerPos, writer.GetOffset32());
		if (m_flags & Flags::HasDebugData)
		{
			writer.Write(GenerateDebugData());
			writer.Align(16);
		}
	}

	// ID BLOCK
	const auto keys = std::views::keys(m_entries);
	std::vector<ResourceKey> sortedKeys{ keys.begin(), keys.end() };
	if (m_version >= 3)
	{
		std::sort(sortedKeys.begin(), sortedKeys.end(), [this](const auto &a, const auto &b) {
			const auto &debugDataA = GetResourceDebugData(a);
			const auto &debugDataB = GetResourceDebugData(b);

			const auto xmlA = (m_version == 3 && debugDataA && debugDataA->name.ends_with(".xml")) ? 1 : 0;
			const auto xmlB = (m_version == 3 && debugDataB && debugDataB->name.ends_with(".xml")) ? 1 : 0;
			const auto idTypeA = a.first.GetIDType();
			const auto idTypeB = b.first.GetIDType();
			const auto indexA = (idTypeA == ResourceID::IDType::GameChanger) ? a.first.GetIndex() : 0;
			const auto indexB = (idTypeB == ResourceID::IDType::GameChanger) ? b.first.GetIndex() : 0;
			const auto resTypeIDA = (idTypeA == ResourceID::IDType::GameChanger) ? a.first.GetResourceTypeID() : 0;
			const auto resTypeIDB = (idTypeB == ResourceID::IDType::GameChanger) ? b.first.GetResourceTypeID() : 0;
			const auto idA = a.first.GetGameChangerID();
			const auto idB = b.first.GetGameChangerID();

			return std::tie(xmlA, a.second, indexA, resTypeIDA, idTypeA, idA) < std::tie(xmlB, b.second, indexB, resTypeIDB, idTypeB, idB);
		});
	}

	writer.VisitAndWrite<uint32_t>(idBlockPointerPos, writer.GetOffset32());
	auto entryDataPointerPos = std::vector<std::array<size_t, 4>>(m_entries.size());
	auto entryIter = sortedKeys.begin();
	for (auto i = 0U; i < m_entries.size(); i++)
	{
		writer.Write<uint64_t>(entryIter->first);

		const auto &e = m_entries.at(*entryIter);

		if (m_version < 5)
			writer.Write(e.importHash);

		for (uint8_t j = 0; j < blocks; j++)
		{
			const auto mappedBlock = MapFileBlockToLibBlock(j);
			if (mappedBlock)
				writer.Write(e.descriptors[*mappedBlock].uncompressedSize | (BitScanReverse(e.descriptors[*mappedBlock].uncompressedAlignment) << 28));
			else
				writer.Write<uint32_t>(0);
		}

		for (uint8_t j = 0; j < blocks; j++)
		{
			const auto mappedBlock = MapFileBlockToLibBlock(j);
			if (mappedBlock)
				writer.Write(e.descriptors[*mappedBlock].onDiskSize | (BitScanReverse(e.descriptors[*mappedBlock].onDiskAlignment) << 28));
			else
				writer.Write<uint32_t>(0);
		}

		for (uint8_t j = 0; j < blocks; j++)
		{
			const auto mappedBlock = MapFileBlockToLibBlock(j);
			if (mappedBlock)
				entryDataPointerPos[i][*mappedBlock] = writer.GetOffset();

			writer.Write<uint32_t>(0);
		}

		writer.Write(e.importOffset);
		writer.Write(e.resourceType);
		writer.Write(e.importCount);

		writer.Write<uint8_t>(0); // flags
		writer.Write<uint8_t>((m_flags & Flags::MultistreamBundle) ? entryIter->second : 0);

		writer.Align(8);

		entryIter = std::next(entryIter);
	}

	// DATA BLOCK
	auto lastAlignment = 0U;
	for (uint8_t i = 0; i < blocks; i++)
	{
		uint32_t blockStart = 0;

		const auto mappedBlock = MapFileBlockToLibBlock(i);
		if (mappedBlock)
		{
			entryIter = sortedKeys.begin();
			for (auto j = 0U; j < m_entries.size(); j++)
			{
				const auto &e = m_entries.at(*entryIter);

				const auto &descriptor = e.descriptors[*mappedBlock];

				if (m_version >= 5)
					writer.Align(descriptor.onDiskAlignment);

				lastAlignment = descriptor.onDiskAlignment;

				if (descriptor.onDiskSize > 0)
				{
					if (m_version < 5)
						writer.Align((i == 0) ? 16 : 0x80);

					// Update the pointer to the first entry.
					if (blockStart == 0)
						blockStart = writer.GetOffset32();

					writer.VisitAndWrite<uint32_t>(entryDataPointerPos[j][*mappedBlock], writer.GetOffset32() - blockStart);
					writer.Write(descriptor.data.get(), descriptor.onDiskSize);
				}

				entryIter = std::next(entryIter);
			}
		}

		// TODO(version 2): GLOBALBACKDROPS, GLOBALPROPS and PERSISTENTAPT on PS3 have double alignment as if the middle block was populated.

		if (blockStart == 0)
		{
			if (m_version < 5)
				writer.Align((i == 0) ? 16 : 0x80);
			blockStart = writer.GetOffset32();
		}

		writer.VisitAndWrite<uint32_t>(fileBlockPointerPos[i], blockStart);
	}

	// RESOURCE STRING TABLE (version >= 3)
	if (m_version >= 3)
	{
		writer.VisitAndWrite<uint32_t>(rstPointerPos, writer.GetOffset32());
		if (m_flags & Flags::HasDebugData)
			writer.Write(GenerateDebugData());
		writer.Align(lastAlignment);
	}

	return true;
}

std::optional<Resource> Bnd2::GetResource(ResourceKey resourceKey) const
{
	const auto it = m_entries.find(resourceKey);
	if (it == m_entries.end())
		return {};

	std::array<Buffer, 4> buffers;
	for (const auto &memoryType : GetMemoryTypes())
		buffers[LIBBNDL_TO_UNDERLYING(memoryType)] = GetBinary(resourceKey, memoryType);

	std::vector<Import> imports;
	const auto numImports = it->second.importCount;
	if (numImports > 0)
	{
		imports.reserve(numImports);

		binaryio::BinaryReader reader(buffers[0], GetPlatformEndian());
		reader.Seek(it->second.importOffset);
		for (auto i = 0U; i < numImports; i++)
		{
			const auto &importEntry = ReadImport(reader);
			imports.emplace_back(importEntry.resourceID, importEntry.offset);
		}

		auto buffer = std::make_unique_for_overwrite<uint8_t[]>(buffers[0].GetSize());
		std::memcpy(buffer.get(), buffers[0].GetData(), buffers[0].GetSize());
		buffers[0] = { std::move(buffer), buffers[0].GetSize(), buffers[0].GetAlignment() };
	}

	return Resource{ std::move(buffers), std::move(imports), it->second.resourceType };
}

ResourceID Bnd2::GetDefaultResourceID() const
{
	if (m_version < 5)
		return Base::GetDefaultResourceID();

	return m_defaultResourceID;
}

int32_t Bnd2::GetDefaultResourceStreamIndex() const
{
	if (m_version < 5)
		return Base::GetDefaultResourceStreamIndex();

	return m_defaultResourceStreamIndex;
}

std::string Bnd2::GetStreamName(uint8_t index) const
{
	if (m_version < 5 || index >= kStreamLimit)
		return Base::GetStreamName(index);

	return m_streamNames[index];
}

std::vector<MemoryType> Bnd2::GetMemoryTypes() const
{
	auto types = Base::GetMemoryTypes();

	if (m_version >= 3)
	{
		switch (m_platform)
		{
		case Platform::Xbox360:
		case Platform::PS3:
		case Platform::PSVita:
		case Platform::WiiU:
			types.reserve(types.capacity() + 1);
			types.emplace_back(MemoryType::Disposable);
			break;
		default:
			break;
		}
	}

	return types;
}

bool Bnd2::IsValidPlatform() const
{
	bool valid = Base::IsValidPlatform();

	if (m_version >= 5 && !valid)
		return (m_platform == Platform::PSVita || m_platform == Platform::WiiU);

	return valid;
}

std::vector<ResourceKey> Bnd2::SortedDebugDataKeys() const
{
	// TODO: This is correct for every bundle except REVERBROADDATA.BNDL

	const auto keys = std::views::keys(m_debugDataEntries);
	std::vector<ResourceKey> sortedKeys{ keys.begin(), keys.end() };
	std::sort(sortedKeys.begin(), sortedKeys.end(), [this](const auto &a, const auto &b) {
		if (m_version < 5)
		{
			const auto &debugDataA = m_debugDataEntries.at(a);
			const auto &debugDataB = m_debugDataEntries.at(b);

			const auto xmlA = (debugDataA.name.ends_with(".xml")) ? 1 : 0;
			const auto xmlB = (debugDataB.name.ends_with(".xml")) ? 1 : 0;

			return std::tie(xmlA, a.first) < std::tie(xmlB, b.first);
		}
		else
		{
			return std::tie(a.second, a.first) < std::tie(b.second, b.first);
		}
	});
	return sortedKeys;
}

std::vector<std::pair<std::string, std::string>> Bnd2::GetDebugDataAttributes(const ResourceKey &resourceKey, const ResourceDebugDataEntry &debugData) const
{
	auto attributes = Base::GetDebugDataAttributes(resourceKey, debugData);

	if (m_version >= 3)
	{
		if (m_entries.size() == 1 && m_defaultResourceStreamIndex == resourceKey.second && !(m_flags & Flags::Compressed)
			|| (m_version == 3 && debugData.name.ends_with(".xml")))
		{
			auto it = std::ranges::find(attributes, "id", &std::pair<std::string, std::string>::first);
			it->second.assign(std::format("{:016x}", static_cast<uint64_t>(resourceKey.first)));
			return attributes;
		}
	}

	if (m_version >= 5)
	{
		const auto &entry = m_entries.find(resourceKey);
		if (entry == m_entries.end())
		{
			attributes.emplace_back("streamIndex", "importEntry");
		}
		else
		{
			attributes.emplace_back("streamIndex", std::to_string(resourceKey.second));
		}
	}

	return attributes;
}

std::optional<uint8_t> Bnd2::MapFileBlockToLibBlock(uint8_t block) const
{
	std::optional<MemoryType> mappedType = {};
	switch (block)
	{
	case 0:
		mappedType = MemoryType::MainMemory;
		break;
	case 1:
		switch (m_platform)
		{
		case Platform::PS3:
		case Platform::PSVita:
			mappedType = MemoryType::GraphicsSystem;
			break;
		case Platform::Xbox360:
			mappedType = MemoryType::Physical;
			break;
		case Platform::PC:
			mappedType = MemoryType::Disposable;
			break;
		case Platform::WiiU:
			mappedType = MemoryType::Mem1;
			break;
		}
		break;
	case 2:
		switch (m_platform)
		{
		case Platform::PS3:
		case Platform::PSVita:
			mappedType = MemoryType::GraphicsLocal;
			break;
		case Platform::Xbox360:
			if (m_version >= 3)
				mappedType = MemoryType::Disposable;
			break;
		case Platform::WiiU:
			mappedType = MemoryType::GraphicsMem2;
			break;
		default:
			break;
		}
		break;
	case 3:
		if (m_version >= 3 && (m_platform == Platform::PS3 || m_platform == Platform::PSVita || m_platform == Platform::WiiU))
			mappedType = MemoryType::Disposable;
		break;
	}

	if (mappedType)
		return LIBBNDL_TO_UNDERLYING(*mappedType);

	return {};
}
