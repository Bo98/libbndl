#include "bndl.hpp"
#include <cstring>

using namespace libbndl;
using namespace libbndl::Formats;

bool Bndl::Load(binaryio::BinaryReader &reader)
{
	auto version = reader.Read<uint32_t>();
	if ((version & 0xFFFF) == 0)
	{
		reader.SwapEndian();
		reader.Seek(-4, std::ios::cur);
		version = reader.Read<uint32_t>();
	}
	m_version = static_cast<uint16_t>(version);
	if (version < 3 || version > 5)
		return false;

	std::optional<Platform> detectedPlatform;
	auto platformReader = reader;
	for (const auto offset : { 0x4C, 0x58, 0x64 })
	{
		platformReader.Seek(offset);
		const auto platform = static_cast<Platform>(platformReader.Read<uint32_t>());
		if (platform == Platform::PC || platform == Platform::Xbox360 || platform == Platform::PS3)
		{
			detectedPlatform = platform;
			break;
		}
	}
	if (!detectedPlatform.has_value())
		return false;
	m_platform = *detectedPlatform;

	const auto numEntries = reader.Read<uint32_t>();

	uint8_t blocks = 4;
	if (m_platform == Platform::Xbox360)
		blocks = 5;
	else if (m_platform == Platform::PS3)
		blocks = 6;
	std::array<uint32_t, 6> dataBlockSizes;
	for (uint8_t i = 0; i < blocks; i++)
	{
		dataBlockSizes[i] = reader.Read<uint32_t>();
		reader.Skip<uint32_t>(); // Alignment
	}

	reader.Seek(static_cast<std::streamoff>(0x4) * blocks, std::ios::cur); // memory address stuff

	const auto idListOffset = reader.Read<uint32_t>();
	const auto idTableOffset = reader.Read<uint32_t>();
	reader.Skip<uint32_t>(); // import block
	reader.Skip<uint32_t>(); // start of data block

	reader.Verify(static_cast<uint32_t>(m_platform));

	auto compressed = 0U;
	auto uncompInfoOffset = 0U;

	if (m_version >= 4)
	{
		compressed = reader.Read<uint32_t>(); // flags but compression is the only valid one
		if (compressed)
			m_flags = Flags::Compressed;
		else
			m_flags = static_cast<Flags>(0);

		reader.Skip<uint32_t>(); // number of compressed resources
		uncompInfoOffset = reader.Read<uint32_t>();
	}

	if (m_version >= 5)
	{
		reader.Skip<uint32_t>(); // main memory alignment
		reader.Skip<uint32_t>(); // graphics memory alignment
	}

	m_entries.clear();
	m_debugDataEntries.clear();
	m_imports.clear();

	reader.Seek(idListOffset);
	std::vector<ResourceID> resourceIDs;
	resourceIDs.reserve(numEntries);
	for (auto i = 0U; i < numEntries; i++)
		resourceIDs.emplace_back(reader.Read<uint64_t>());

	reader.Seek(idTableOffset);
	for (const auto resourceID : resourceIDs)
	{
		auto &e = m_entries[{ resourceID, static_cast<uint8_t>(0) }];

		reader.Skip<uint32_t>(); // runtime-only memory variable
		e.importOffset = reader.Read<uint32_t>();
		e.resourceType = reader.Read<uint32_t>();

		for (uint8_t j = 0; j < blocks; j++)
		{
			const auto mappedBlock = MapFileBlockToLibBlock(j);
			if (!mappedBlock)
			{
				reader.Verify<uint32_t>(0); // size
				reader.Verify<uint32_t>(1); // alignment
			}
			else
			{
				e.descriptors[*mappedBlock].onDiskSize = reader.Read<uint32_t>();
				e.descriptors[*mappedBlock].onDiskAlignment = reader.Read<uint32_t>();

				if (!compressed)
				{
					e.descriptors[*mappedBlock].uncompressedSize = e.descriptors[*mappedBlock].onDiskSize;
					e.descriptors[*mappedBlock].uncompressedAlignment = e.descriptors[*mappedBlock].onDiskAlignment;
				}
			}
		}

		auto dataReader = reader;
		uint32_t dataBlockStartOffset = 0;
		for (uint8_t j = 0; j < blocks; j++)
		{
			if (j > 0)
				dataBlockStartOffset += dataBlockSizes[j - 1];

			const auto readOffset = reader.Read<uint32_t>() + dataBlockStartOffset;
			reader.Skip<uint32_t>(); // 1

			const auto mappedBlock = MapFileBlockToLibBlock(j);
			if (!mappedBlock)
			{
				assert(dataBlockSizes[j] == 0);
				continue;
			}

			auto &descriptor = e.descriptors[*mappedBlock];

			if (descriptor.onDiskSize == 0)
			{
				descriptor.data = nullptr;
				continue;
			}

			dataReader.Seek(readOffset); // Read offset

			descriptor.data = std::unique_ptr<uint8_t[]>(dataReader.Read<uint8_t *>(descriptor.onDiskSize));
		}

		reader.Seek(static_cast<std::streamoff>(0x4) * blocks, std::ios::cur); // memory address stuff
	}

	if (compressed)
	{
		reader.Seek(uncompInfoOffset);
		for (const auto resourceID : resourceIDs)
		{
			auto &e = m_entries[{ resourceID, static_cast<uint8_t>(0) }];

			for (uint8_t j = 0; j < blocks; j++)
			{
				const auto mappedBlock = MapFileBlockToLibBlock(j);
				if (!mappedBlock)
				{
					reader.Verify<uint32_t>(0); // size
					reader.Verify<uint32_t>(1); // alignment
				}
				else
				{
					e.descriptors[*mappedBlock].uncompressedSize = reader.Read<uint32_t>();
					e.descriptors[*mappedBlock].uncompressedAlignment = reader.Read<uint32_t>();
				}
			}
		}
	}

	for (const auto resourceID : resourceIDs)
	{
		auto &e = m_entries[{ resourceID, static_cast<uint8_t>(0) }];
		const auto depOffset = e.importOffset;
		if (depOffset == 0)
			continue;

		reader.Seek(depOffset);
		e.importCount = static_cast<uint16_t>(reader.Read<uint32_t>());
		reader.Verify<uint32_t>(0);
		for (auto i = 0U; i < e.importCount; i++)
			m_imports[resourceID].emplace_back(ReadImport(reader));
	}

	auto rstFile = GetBinary({ ResourceID(0xC039284A), static_cast<uint8_t>(0) }, MemoryType::MainMemory);
	if (rstFile == nullptr)
		return true;

	m_flags |= Flags::HasDebugData;

	auto rstReader = binaryio::BinaryReader(rstFile);

	const auto strLen = rstReader.Read<uint32_t>();
	auto rstXML = rstReader.ReadString(strLen);

	// Cover Criterion's broken XML writer.
	if (rstXML.starts_with("</ResourceStringTable>"))
		rstXML.erase(1, 1);
	const auto pos = rstXML.find("</ResourceStringTable>\n\t");
	if (pos != std::string::npos)
		rstXML.erase(pos, 23);

	ParseDebugData(rstXML);

	m_entries.erase({ ResourceID(0xC039284A), static_cast<uint8_t>(0) });

	return true;
};

bool Bndl::Save(binaryio::BinaryWriter &writer)
{
	if (m_version < 3 || m_version > 5)
		return false;

	// Only one flag is supported. Allow HasDebugData since we simulate it ourselves here.
	if (BitScanReverse(static_cast<uint32_t>(m_flags & ~Flags::HasDebugData)) >= 1)
		return false;

	if (m_version <= 3 && (m_flags & Flags::Compressed))
		return false; // Invalid combination

	if (!IsValidPlatform())
		return false;

	writer.SetEndian(GetPlatformEndian());

	writer.Write("bndl", 4);
	writer.Write<uint32_t>(m_version);

	const bool writeDebugData = !m_debugDataEntries.empty() && !(m_flags & Flags::Compressed); // TODO: is the compressed check accurate?
	auto entryCount = static_cast<uint32_t>(m_entries.size());
	if (writeDebugData)
		entryCount++;

	writer.Write<uint32_t>(entryCount);

	uint8_t blocks = 4;
	if (m_platform == Platform::Xbox360)
		blocks = 5;
	else if (m_platform == Platform::PS3)
		blocks = 6;

	std::array<size_t, 4> dataBlockDescriptorsPos;
	for (uint8_t i = 0; i < blocks; i++)
	{
		const auto mappedBlock = MapFileBlockToLibBlock(i);
		if (mappedBlock)
			dataBlockDescriptorsPos[*mappedBlock] = writer.GetOffset();
		writer.Write<uint32_t>(0); // size
		writer.Write<uint32_t>(1); // alignment
	}

	for (auto i = 0; i < blocks; i++)
	{
		writer.Write<uint32_t>(0); // memory addresses
	}

	auto idListPointerPos = writer.GetOffset();
	writer.Seek(4, std::ios::cur);
	auto idTablePointerPos = writer.GetOffset();
	writer.Seek(4, std::ios::cur);
	auto importBlockPointerPos = writer.GetOffset();
	writer.Seek(4, std::ios::cur);
	auto dataBlockPointerPos = writer.GetOffset();
	writer.Seek(4, std::ios::cur);

	writer.Write<uint32_t>(m_platform);

	size_t uncompInfoBlockPointerPos = 0;

	if (m_version >= 4)
	{
		writer.Write<uint32_t>(m_flags & ~Flags::HasDebugData);
		writer.Write<uint32_t>((m_flags & Flags::Compressed) ? entryCount : 0);
		uncompInfoBlockPointerPos = writer.GetOffset();
		writer.Write<uint32_t>(0); // will write later, but only if needed
	}

	if (m_version >= 5)
	{
		writer.Write<uint32_t>(0); // Main memory alignment. Setting this to 0 so we don't need to deal with memory addresses.
		writer.Write<uint32_t>(0); // Graphics memory alignment.
	}

	writer.Align(0x10);

	// ID LIST
	writer.VisitAndWrite<uint32_t>(idListPointerPos, writer.GetOffset32());
	for (const auto &entry : m_entries)
	{
		writer.Write<uint64_t>(entry.first.first);
	}
	if (writeDebugData)
		writer.Write<uint64_t>(0xC039284A);

	// Prepare ResourceStringTable
	if (writeDebugData)
	{
		const auto outStr = GenerateDebugData();

		auto debugDataWriter = binaryio::BinaryWriter();
		debugDataWriter.Write(static_cast<uint32_t>(outStr.size()));
		debugDataWriter.Write(outStr);

		const auto stream = debugDataWriter.GetStream();
		const auto data = stream.view();
		const auto dataSize = data.size();

		auto &e = m_entries[{ ResourceID(0xFFFFFFFF), static_cast<uint8_t>(0) }]; // HACK
		e.resourceType = ResourceType::Burnout::TextFile;

		e.descriptors[0].data = std::make_unique_for_overwrite<uint8_t[]>(dataSize);
		std::memcpy(e.descriptors[0].data.get(), data.data(), dataSize);

		e.descriptors[0].uncompressedSize = static_cast<uint32_t>(dataSize);
		e.descriptors[0].uncompressedAlignment = 4;
	}

	// ID TABLE
	writer.VisitAndWrite<uint32_t>(idTablePointerPos, writer.GetOffset32());

	struct FilePointerPosHelper
	{
		size_t importPointerPos;
		std::array<size_t, 4> dataBlockPointerPos;
	};
	std::map<ResourceID, FilePointerPosHelper> filePointerPosMap;
	for (const auto &entry : m_entries)
	{
		writer.Write<uint32_t>(0); // Ignore

		auto &posHelper = filePointerPosMap[entry.first.first];

		posHelper.importPointerPos = writer.GetOffset();
		writer.Write<uint32_t>(0);

		writer.Write(entry.second.resourceType);

		for (uint8_t i = 0; i < blocks; i++)
		{
			const auto mappedBlock = MapFileBlockToLibBlock(i);
			if (!mappedBlock)
			{
				writer.Write<uint32_t>(0); // size
				writer.Write<uint32_t>(1); // alignment
			}
			else
			{
				const auto &descriptor = entry.second.descriptors[*mappedBlock];
				writer.Write<uint32_t>(descriptor.onDiskSize);
				writer.Write<uint32_t>((descriptor.onDiskSize == 0) ? 1 : descriptor.onDiskAlignment);
			}
		}

		for (uint8_t i = 0; i < blocks; i++)
		{
			const auto mappedBlock = MapFileBlockToLibBlock(i);
			if (mappedBlock)
				posHelper.dataBlockPointerPos[*mappedBlock] = writer.GetOffset();

			writer.Write<uint32_t>(0);
			writer.Write<uint32_t>(1); // constant
		}

		// Memory stuff
		for (auto i = 0; i < blocks; i++)
			writer.Write<uint32_t>(0);
	}

	// UNCOMPRESSED SIZE INFO
	if (m_flags & Flags::Compressed)
	{
		writer.VisitAndWrite<uint32_t>(uncompInfoBlockPointerPos, writer.GetOffset32());
		for (const auto &entry : m_entries)
		{
			for (uint8_t i = 0; i < blocks; i++)
			{
				const auto mappedBlock = MapFileBlockToLibBlock(i);
				if (!mappedBlock)
				{
					writer.Write<uint32_t>(0); // size
					writer.Write<uint32_t>(1); // alignment
				}
				else
				{
					const auto &descriptor = entry.second.descriptors[*mappedBlock];
					writer.Write<uint32_t>(descriptor.uncompressedSize);
					writer.Write<uint32_t>((descriptor.uncompressedSize == 0) ? 1 : descriptor.uncompressedAlignment);
				}
			}
		}
	}

	// IMPORTS
	writer.VisitAndWrite<uint32_t>(importBlockPointerPos, writer.GetOffset32());
	for (const auto &entry : m_entries)
	{
		const auto &imports = m_imports[entry.first.first];
		if (imports.empty())
			continue;

		writer.VisitAndWrite<uint32_t>(filePointerPosMap.at(entry.first.first).importPointerPos, writer.GetOffset32());

		writer.Write(static_cast<uint32_t>(imports.size()));
		writer.Write<uint32_t>(0); // padding
		for (const auto &import : imports)
		{
			writer.Write<uint64_t>(import.resourceID);
			writer.Write<uint32_t>(import.offset);
			writer.Align(8);
		}
	}

	// DATA
	writer.VisitAndWrite<uint32_t>(dataBlockPointerPos, writer.GetOffset32());
	uint32_t blockStartOffset = 0;
	for (uint8_t i = 0; i < blocks; i++)
	{
		const auto mappedBlock = MapFileBlockToLibBlock(i);
		if (!mappedBlock)
			continue;

		for (const auto &entry : m_entries)
		{
			const auto &e = entry.second;

			const auto &descriptor = e.descriptors[i];

			if (descriptor.onDiskSize > 0)
			{
				writer.VisitAndWrite<uint32_t>(filePointerPosMap.at(entry.first.first).dataBlockPointerPos[*mappedBlock], writer.GetOffset32() - blockStartOffset);
				writer.Write(descriptor.data.get(), descriptor.onDiskSize);
			}
		}

		const auto size = writer.GetOffset32() - blockStartOffset;
		writer.VisitAndWrite<uint32_t>(dataBlockDescriptorsPos[*mappedBlock], size);
		writer.VisitAndWrite<uint32_t>(dataBlockDescriptorsPos[*mappedBlock], (size == 0) ? 1 : ((*mappedBlock >= 1) ? 4096 : 1024)); // TODO: This changes and I don't know the pattern.
		blockStartOffset = writer.GetOffset32();
	}

	m_entries.erase({ ResourceID(0xFFFFFFFF), static_cast<uint8_t>(0) });

	return true;
}

std::optional<uint8_t> Bndl::MapFileBlockToLibBlock(uint8_t block) const
{
	std::optional<MemoryType> mappedType = {};
	switch (block)
	{
	case 0:
		mappedType = MemoryType::MainMemory;
		break;
	case 1:
		mappedType = MemoryType::Disposable;
		break;
	case 2:
		if (m_platform == Platform::Xbox360)
			mappedType = MemoryType::Physical;
		break;
	case 3:
		break;
	case 4:
		if (m_platform == Platform::PS3)
			mappedType = MemoryType::GraphicsSystem;
		break;
	case 5:
		if (m_platform == Platform::PS3)
			mappedType = MemoryType::GraphicsLocal;
		break;
	default:
		assert(false);
		break;
	}

	if (mappedType)
		return LIBBNDL_TO_UNDERLYING(*mappedType);

	return {};
}

std::optional<Resource> Bndl::GetResource(ResourceKey resourceKey) const
{
	const auto it = m_entries.find(resourceKey);
	if (it == m_entries.end())
		return {};

	std::array<Buffer, 4> buffers;
	for (const auto &memoryType : GetMemoryTypes())
		buffers[LIBBNDL_TO_UNDERLYING(memoryType)] = GetBinary(resourceKey, memoryType);

	std::vector<Import> imports;
	if (it->second.importCount > 0)
	{
		const auto &importEntries = m_imports.at(resourceKey.first);
		for (const auto &importEntry : importEntries)
			imports.emplace_back(importEntry.resourceID, importEntry.offset);
	}

	return Resource{ std::move(buffers), std::move(imports), it->second.resourceType };
}
