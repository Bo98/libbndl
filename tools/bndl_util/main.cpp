#include <libbndl/bundle.hpp>
#include <cctype>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <regex>
#include <cxxopts.hpp>
#include <pugixml.hpp>
#include "types.hpp"

using namespace libbndl;

namespace
{
	std::string cleanResourceNameForHash(const std::string &resourceName)
	{
		if (std::regex_search(resourceName, std::regex("^[a-zA-Z]:\\\\")))
		{
			return std::filesystem::path(resourceName).filename().string();
		}

		return resourceName;
	}

	std::string cleanResourceNameForIO(const std::string &resourceName)
	{
		const auto gameDBStart = resourceName.find("gamedb://");
		if (gameDBStart != std::string::npos)
		{
			const auto basenameStart = resourceName.find_last_of('/') + 1;
			if (basenameStart < gameDBStart)
				return resourceName;
			auto basenameEnd = resourceName.find_first_of('?', basenameStart);
			if (basenameEnd == std::string::npos)
				basenameEnd = resourceName.find_first_of('#', basenameStart);

			const auto prefix = resourceName.substr(0, gameDBStart);
			if (basenameEnd == std::string::npos)
				return prefix + resourceName.substr(basenameStart);

			std::string_view suffix = resourceName;
			suffix.remove_prefix(basenameEnd);

			std::match_results<std::string_view::const_iterator> match;
			std::regex_search(suffix.begin(), suffix.end(), match, std::regex("^(?:\\?ID=|#)\\d+"));
			return prefix + resourceName.substr(basenameStart, basenameEnd - basenameStart) + match.suffix().str();
		}

		return cleanResourceNameForHash(resourceName);
	}

	std::string resourceIDToString(const ResourceID &resourceID)
	{
		return std::format("0x{:0{}X}", static_cast<uint64_t>(resourceID), (resourceID.GetIDType() != ResourceID::IDType::Normal) ? 16 : 8);
	}

	std::string getDebugName(const Bundle &arch, const std::optional<ResourceDebugData> &debugData, const std::string &fallback)
	{
		if (debugData)
		{
			const auto cleanedName = cleanResourceNameForIO(debugData->GetName());

			if (arch.GetResourceDebugData(ResourceID(cleanResourceNameForHash(debugData->GetName()))) && std::ranges::all_of(cleanedName, [](char c) { return std::isalnum(c) || c == '.' || c == '-' || c == '_' || c == '~' || c == '(' || c == ')' || c == ',' || c == '+' || c == ' '; }))
				return cleanedName;
		}

		return fallback;
	}
}

int main(int argc, char **argv)
{
	cxxopts::Options options("bndl_util", "A program to work with Burnout Paradise bundle archives (BETA)");
	options.add_options()
		("e,extract", "Extract the archive to a given directory", cxxopts::value<std::string>())
		("p,pack", "Pack a folder structure to a bundle archive", cxxopts::value<std::string>())
		("f,file", "Name of the archive that should be extracted/generated", cxxopts::value<std::string>())
		("a,all", "(Recursively) extract all bundles in the specified directory", cxxopts::value<std::string>())
		("s,search", "Search for an entry", cxxopts::value<std::string>())
		("l,list", "List all entries");

	auto parsedOptions = options.parse(argc, argv);
	if (parsedOptions.count("file") == 0 && parsedOptions.count("all") == 0)
	{
		std::cout << "Please specify a bundle file.\n" << options.help() << '\n';
		return EXIT_FAILURE;
	}

	if (parsedOptions.count("file") > 0 && parsedOptions.count("all") > 0)
	{
		std::cout << "The --file and --all arguments are mutually exclusive.\n";
		return EXIT_FAILURE;
	}

	if (parsedOptions.count("all") > 0 && parsedOptions.count("extract") == 0)
	{
		std::cout << "--all can only be used in extract mode.\n";
		return EXIT_FAILURE;
	}

	const auto extract = parsedOptions.count("extract") > 0;
	const auto extractDir = extract ? std::filesystem::path(parsedOptions["extract"].as<std::string>()) : std::filesystem::path();
	const auto pack = parsedOptions.count("pack") > 0;
	const auto packDir = pack ? std::filesystem::path(parsedOptions["pack"].as<std::string>()) : std::filesystem::path();
	const auto list = parsedOptions["list"].as<bool>();
	const auto file = parsedOptions["file"].as_optional<std::string>();
	const auto all = parsedOptions["all"].as_optional<std::string>();
	const auto searchTerm = parsedOptions["search"].as_optional<std::string>();
	const auto search = searchTerm.has_value();
	
	if ((pack + extract + list + search) != 1)
	{
		std::cout << "Please specify exactly one operation that should be executed.\n" << options.help() << '\n';
		return EXIT_FAILURE;
	}

	Bundle arch;
	if (!pack)
	{
		if (list)
		{
			if (!arch.Load(file.value()))
			{
				std::cout << "Failed to open " << file.value() << '\n';
				return EXIT_FAILURE;
			}

			std::cout.fill('-');
			std::cout << std::left << std::setw(70) << "NAME" << std::right << "FILE TYPE\n";
			std::cout.fill(' ');
			for (const auto &resourceID : arch.GetResourceIDs())
			{
				for (const auto &streamIndex : arch.GetResourceStreamIndices(resourceID))
				{
					const auto debugData = arch.GetResourceDebugData(resourceID, streamIndex);
					const auto resourceType = *arch.GetResourceType(resourceID, streamIndex);
					std::ostringstream name;
					if (debugData)
						name << debugData->GetName();
					else
						name << std::hex << static_cast<uint64_t>(resourceID);
					std::ostringstream typeName;
					if (debugData)
						typeName << debugData->GetTypeName();
					else
						typeName << std::hex << resourceType;
					std::cout << std::left << std::setw(70) << name.str() << std::right << typeName.str() << '\n';
				}
			}
		}
		else if (extract)
		{
			std::cout << "Extracting...\n";

			std::vector<std::filesystem::path> archives;
			if (file)
				archives.emplace_back(*file);
			else
				for (const auto &entry : std::filesystem::recursive_directory_iterator(all.value()))
					if (std::filesystem::is_regular_file(entry))
						archives.push_back(entry.path());

			for (const auto &archive : archives)
			{
				if (!arch.Load(archive.string()))
				{
					if (file)
					{
						std::cout << "Failed to open " << *file << '\n';
						return EXIT_FAILURE;
					}
					continue;
				}

				std::filesystem::path archiveExtractDir;
				if (file)
					archiveExtractDir = extractDir;
				else
					archiveExtractDir = extractDir / std::filesystem::relative(archive, all.value()).replace_extension();

				try
				{
					std::filesystem::create_directories(archiveExtractDir);
				}
				catch (std::filesystem::filesystem_error &e)
				{
					std::cout << "Failed to create extract directory: " << e.what() << '\n';
					return EXIT_FAILURE;
				}

				pugi::xml_document doc;
				auto configRoot = doc.append_child("Bundle");

				configRoot.append_attribute("version").set_value((std::to_string(static_cast<uint32_t>(arch.GetMagic())) + "." + std::to_string(arch.GetVersion())));

				const auto platform = arch.GetPlatform();
				std::string platformName;
				switch (platform)
				{
				case Platform::PC:
					platformName = "PC";
					break;
				case Platform::Xbox360:
					platformName = "Xbox 360";
					break;
				case Platform::PS3:
					platformName = "PS3";
					break;
				case Platform::PSVita:
					platformName = "PS Vita";
					break;
				case Platform::WiiU:
					platformName = "Wii U";
					break;
				}
				configRoot.append_attribute("platform").set_value(platformName);

				const auto flags = arch.GetFlags();
				configRoot.append_attribute("compressed").set_value(!!(flags & Flags::Compressed));
				if (arch.GetMagic() == Magic::Bnd2 && arch.GetVersion() < 5)
				{
					configRoot.append_attribute("mainMemOptimised").set_value(!!(flags & Flags::MainMemOptimised));
					configRoot.append_attribute("graphicsMemOptimised").set_value(!!(flags & Flags::GraphicsMemOptimised));
				}
				configRoot.append_attribute("debugData").set_value(!!(flags & Flags::HasDebugData));
				if (arch.IsNeedForSpeedEra())
				{
					configRoot.append_attribute("nonAsynchFixupRequired").set_value(!!(flags & Flags::NonAsynchFixupRequired));
					configRoot.append_attribute("multistreamBundle").set_value(!!(flags &Flags::MultistreamBundle));
					configRoot.append_attribute("deltaBundle").set_value(!!(flags &Flags::DeltaBundle));
					if (arch.GetVersion() >= 5)
					{
						if (flags & Flags::ContainsDefaultResource)
						{
							const auto debugData = arch.GetResourceDebugData(arch.GetDefaultResourceID());
							const auto defaultName = getDebugName(arch, debugData, resourceIDToString(arch.GetDefaultResourceID()));
							configRoot.append_attribute("defaultResource").set_value(defaultName);
							configRoot.append_attribute("defaultStreamIndex").set_value(arch.GetDefaultResourceStreamIndex());
						}

						for (uint8_t i = 0; i < 4; i++)
						{
							const auto streamName = arch.GetStreamName(i);
							if (streamName.empty())
								continue;

							auto entryChild = configRoot.append_child("Stream");
							entryChild.append_attribute("name").set_value(streamName);
						}
					}
				}

				std::ofstream manifest(archiveExtractDir / "_manifest.txt");
				manifest << "# This is a mapping of IDs to file names for your information. This file is not used for bundle packing and does not need to exist.\n";

				for (const auto &resourceID : arch.GetResourceIDs())
				{
					const auto idStr = resourceIDToString(resourceID);
					auto debugName = std::string();

					for (const auto &streamIndex : arch.GetResourceStreamIndices(resourceID))
					{
						const auto debugData = arch.GetResourceDebugData(resourceID);
						const auto resourceType = *arch.GetResourceType(resourceID, streamIndex);
						const auto data = *arch.GetResource(resourceID, streamIndex);

						auto name = getDebugName(arch, debugData, idStr);
						if (streamIndex > 0)
							name += ".stream" + std::to_string(streamIndex);

						const auto &fileTypeNames = (arch.IsNeedForSpeedEra() ? g_nfsFileTypeNames : g_burnoutFileTypeNames);
						const auto it = fileTypeNames.find(resourceType);
						std::ostringstream pathTypeNameStream;
						if (it == fileTypeNames.end())
							pathTypeNameStream << "Type " << std::hex << std::setw(2) << std::setfill('0') << std::uppercase << resourceType;
						else
							pathTypeNameStream << it->second;
						const auto fileExtractDir = archiveExtractDir / pathTypeNameStream.str();

						try
						{
							std::filesystem::create_directory(fileExtractDir);
						}
						catch (std::filesystem::filesystem_error &e)
						{
							std::cout << "Failed to create directory: " << e.what() << '\n';
							return EXIT_FAILURE;
						}

						for (const auto &memoryType : arch.GetMemoryTypes())
						{
							const auto &buffer = data.GetBinary(memoryType);
							if (buffer == nullptr)
								continue;

							std::string typeExt;
							switch (memoryType)
							{
							case MemoryType::MainMemory:
								typeExt = ".mainmem";
								break;
							case MemoryType::GraphicsSystem:
								if (platform == Platform::PS3)
									typeExt = ".gfxsysmem";
								else if (platform == Platform::Xbox360)
									typeExt = ".physical";
								else
									typeExt = ".dummy";
								break;
							case MemoryType::GraphicsLocal:
								if (platform == Platform::PS3)
									typeExt = ".gfxlocalmem";
								else
									typeExt = ".dummy";
								break;
							case MemoryType::Disposable:
								typeExt = ".disposable";
								break;
							}

							const auto path = fileExtractDir / (name + typeExt + ".bin");
							std::ofstream outfile(path, std::ios::out | std::ios::binary);
							outfile.write(reinterpret_cast<const char *>(buffer.GetData()), buffer.GetSize());
							outfile.close();
							if (outfile.fail())
								std::cout << "Failed to create extract " << path << ": " << std::error_code(errno, std::generic_category()) << '\n';
						}

						const auto &imports = data.GetImports();
						if (!imports.empty())
						{
							pugi::xml_document importsDoc;
							auto importsRoot = importsDoc.append_child("Imports");

							for (const auto &import : imports)
							{
								auto entryChild = importsRoot.append_child("Import");
								const auto depDebugData = arch.GetResourceDebugData(import.GetResourceID());
								if (depDebugData)
									entryChild.append_attribute("name").set_value(depDebugData->GetName());
								else
									entryChild.append_attribute("id").set_value(resourceIDToString(import.GetResourceID()));

								std::ostringstream offsetStrStream;
								offsetStrStream << "0x" << std::hex << std::setw(8) << std::setfill('0') << std::uppercase << import.GetOffset();
								entryChild.append_attribute("offset").set_value(offsetStrStream.str());

								if (arch.IsNeedForSpeedEra())
									entryChild.append_attribute("resourceHandle").set_value(import.GetImportType() == Import::ImportType::ResourceHandle);
							}

							std::ofstream outfile(fileExtractDir / (name + ".imports.xml"));
							importsDoc.save(outfile, "\t", pugi::format_indent | pugi::format_no_declaration, pugi::encoding_utf8);
						}

						auto entryChild = configRoot.append_child("Resource");
						if (!debugData || debugData->GetName().empty())
							entryChild.append_attribute("id").set_value(idStr);
						else
							entryChild.append_attribute("name").set_value(debugData->GetName());
						if (!debugData || debugData->GetTypeName().empty())
							entryChild.append_attribute("typeDebugName").set_value(pathTypeNameStream.str());
						else
							entryChild.append_attribute("typeDebugName").set_value(debugData->GetTypeName());
						if (arch.IsNeedForSpeedEra() && (flags & Flags::MultistreamBundle))
							entryChild.append_attribute("streamIndex").set_value(streamIndex);

						if (debugData && !debugData->GetName().empty())
							debugName = debugData->GetName();
					}

					manifest << idStr << " = " << debugName << '\n';
				}

				std::ofstream outfile(archiveExtractDir / "_config.xml");
				doc.save(outfile, "\t", pugi::format_indent | pugi::format_no_declaration, pugi::encoding_utf8);
			}
		}
	}
	else
	{
		if (!std::filesystem::is_directory(packDir))
		{
			std::cout << "The path supplied for packing is not a directory.\n";
			return EXIT_FAILURE;
		}

		const auto configPath = packDir / "_config.xml";
		if (!std::filesystem::is_regular_file(configPath) && !std::filesystem::is_symlink(configPath))
		{
			std::cout << "Could not find a _config.xml file in the directory to pack.\n";
			return EXIT_FAILURE;
		}

		// TODO: check for overwrite?
	}

	return 0;
}
