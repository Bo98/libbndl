#include <libbig/big.hpp>
#include "util.hpp"
#include <iostream>
using namespace libbig;


bool Big::Load(const std::string& name)
{
	m_stream.open(name,std::ios::in| std::ios::binary);
	//check if archive exists
	if (m_stream.fail())
		return false;

	//check if it's a big archive
	std::string magic;
	for (int i = 0; i < 4; ++i)
		magic += m_stream.get();
	
	if (magic == std::string("BIGF"))
		m_version = CC;
	else if (magic == std::string("BIG4"))
		m_version = BFME;
	else
		return false;

	m_size = read<uint32_t>(m_stream);
	m_numEntries = reverse(read<uint32_t>(m_stream));
	uint32_t first = reverse(read<uint32_t>(m_stream));

	for (auto i = 0; i < m_numEntries; i++)
	{
		Entry e;
		e.Offset = reverse(read<uint32_t>(m_stream));
		e.Size = reverse(read<uint32_t>(m_stream));
		auto Name = readString(m_stream);
		m_entries[Name] = e;
	}
	return true;
}

uint8_t* Big::GetEntry(const std::string & entry, uint32_t & size)
{
	size = 0;
	auto it = m_entries.find(entry);
	if (it == m_entries.end())
		return nullptr;

	Entry e = it->second;
	uint8_t* buffer = new uint8_t[e.Size];
	m_stream.seekg(e.Offset, std::ios::beg);
	m_stream.read(reinterpret_cast<char*>(buffer), e.Size);
	size = e.Size;
	return buffer;
}

std::string Big::GetEntry(const std::string & entry)
{
	auto it = m_entries.find(entry);
	if (it == m_entries.end())
		return std::string();

	Entry e = it->second;
	std::string buffer;
	buffer.resize(e.Size);
	m_stream.seekg(e.Offset, std::ios::beg);
	m_stream.read(const_cast<char*>(buffer.data()), e.Size);
	return buffer;
}


std::vector<std::string> Big::ListEntries()
{
	std::vector<std::string> entries;
	for (const auto& e : m_entries)
	{
		entries.push_back(e.first);
	}
	return entries;
}