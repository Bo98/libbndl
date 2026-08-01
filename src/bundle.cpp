#include <libbndl/bundle.hpp>
#include "formats/bndl.hpp"
#include "formats/bnd2.hpp"
#include <binaryio/binaryreader.hpp>
#include <binaryio/binarywriter.hpp>
#include <algorithm>
#include <fstream>
#include <locale>
#include <zlib.h>

using namespace libbndl;

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

Bundle::Bundle(Bundle &&) noexcept = default;
Bundle &Bundle::operator=(Bundle &&) noexcept = default;
Bundle::~Bundle() = default;

bool Bundle::Load(const std::filesystem::path &name)
{
	std::ifstream stream;

	stream.open(name, std::ios::in | std::ios::binary | std::ios::ate);

	// Check if archive exists
	if (stream.fail())
		return false;

	const auto fileSize = stream.tellg();
	if (fileSize < 4)
		return false;

	stream.seekg(0, std::ios::beg);

	std::vector<uint8_t> buffer(fileSize);
	stream.read(reinterpret_cast<char *>(buffer.data()), fileSize);
	stream.close();
	if (stream.fail())
		return false;

	auto reader = binaryio::BinaryReader(buffer, std::endian::little);

	// Check if it's a BNDL archive
	auto magic = reader.ReadString(4);
	if (magic == std::string("bndl"))
		m_impl = std::make_unique<Formats::Bndl>();
	else if (magic == std::string("bnd2"))
		m_impl = std::make_unique<Formats::Bnd2>();
	else
		return false;

	return m_impl->Load(reader);
}

bool Bundle::Save(const std::filesystem::path &name)
{
	auto writer = binaryio::BinaryWriter();

	if (!m_impl->Save(writer))
		return false;

	const auto stream = writer.GetStream();

	std::ofstream f(name, std::ios::out | std::ios::binary);
	f << stream.rdbuf();
	f.close();

	return true;
}

Magic Bundle::GetMagic() const
{
	return m_impl->GetMagic();
}

uint16_t Bundle::GetVersion() const
{
	return m_impl->GetVersion();
}

Platform Bundle::GetPlatform() const
{
	return m_impl->GetPlatform();
}

Flags Bundle::GetFlags() const
{
	return m_impl->GetFlags();
}

bool Bundle::IsBurnoutEra() const
{
	return GetMagic() == Magic::Bndl || GetVersion() <= 2;
}

bool Bundle::IsNeedForSpeedEra() const
{
	return GetMagic() == Magic::Bnd2 && GetVersion() >= 3;
}

std::optional<Resource> Bundle::GetResource(ResourceID resourceID, uint8_t streamIndex) const
{
	return m_impl->GetResource({ resourceID, streamIndex });
}

Buffer Bundle::GetBinary(ResourceID resourceID, MemoryType memoryType, uint8_t streamIndex) const
{
	return m_impl->GetBinary({ resourceID, streamIndex }, memoryType);
}

std::optional<ResourceDebugData> Bundle::GetResourceDebugData(ResourceID resourceID, uint8_t streamIndex) const
{
	const auto &internalDebugData = m_impl->GetResourceDebugData({ resourceID, streamIndex });
	if (!internalDebugData)
		return {};

	return ResourceDebugData{ internalDebugData->name, internalDebugData->typeName };
}

std::optional<uint32_t> Bundle::GetResourceType(ResourceID resourceID, uint8_t streamIndex) const
{
	return m_impl->GetResourceType({ resourceID, streamIndex });
}

bool Bundle::AddResource(ResourceID resourceID, const Resource &resource, uint8_t streamIndex)
{
	return m_impl->AddResource({ resourceID, streamIndex }, resource);
}

bool Bundle::AddResourceDebugData(ResourceID resourceID, const ResourceDebugData &debugData, uint8_t streamIndex)
{
	return m_impl->AddResourceDebugData({ resourceID, streamIndex }, debugData.GetName(), debugData.GetTypeName());
}

bool Bundle::ReplaceResource(ResourceID resourceID, const Resource &resource, uint8_t streamIndex)
{
	return m_impl->ReplaceResource({ resourceID, streamIndex }, resource);
}

uint32_t Bundle::GetResourceCount() const
{
	return m_impl->GetResourceCount();
}

std::vector<ResourceID> Bundle::GetResourceIDs() const
{
	return m_impl->GetResourceIDs();
}

std::map<uint32_t, std::vector<ResourceID>> Bundle::GetResourceIDsByType() const
{
	return m_impl->GetResourceIDsByType();
}

std::vector<uint8_t> Bundle::GetResourceStreamIndices(ResourceID resourceID) const
{
	return m_impl->GetResourceStreamIndices(resourceID);
}

ResourceID Bundle::GetDefaultResourceID() const
{
	return m_impl->GetDefaultResourceID();
}

int32_t Bundle::GetDefaultResourceStreamIndex() const
{
	return m_impl->GetDefaultResourceStreamIndex();
}

std::string Bundle::GetStreamName(uint8_t index) const
{
	return m_impl->GetStreamName(index);
}

std::vector<MemoryType> Bundle::GetMemoryTypes() const
{
	return m_impl->GetMemoryTypes();
}
