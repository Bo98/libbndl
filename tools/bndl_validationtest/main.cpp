#include <libbndl/bundle.hpp>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <span>

namespace
{
	bool identicalFiles(const std::filesystem::path &path1, const std::filesystem::path &path2)
	{
		std::ifstream file1(path1, std::ifstream::binary | std::ifstream::ate);
		std::ifstream file2(path2, std::ifstream::binary | std::ifstream::ate);

		if (file1.fail() || file2.fail()) {
			return false;
		}

		if (file1.tellg() != file2.tellg()) {
			return false;
		}

		file1.seekg(0, std::ifstream::beg);
		file2.seekg(0, std::ifstream::beg);
		return std::equal(std::istreambuf_iterator<char>(file1.rdbuf()), std::istreambuf_iterator<char>(), std::istreambuf_iterator<char>(file2.rdbuf()));
	}
}

int main(int argc, char **argv)
{
	if (argc != 2)
	{
		std::cerr << "Invalid arguments\n";
		return 1;
	}

	auto args = std::span(argv, static_cast<size_t>(argc));

	const auto dir = std::filesystem::path(args[1]);
	if (!std::filesystem::is_directory(dir))
	{
		std::cerr << "Invalid directory\n";
		return 2;
	}

	// This is just a test tool so not to bothered about the scenario where this already exists.
	const auto outputFile = std::filesystem::temp_directory_path() / "bndl_validationtest.bndl";
	
	std::filesystem::remove(outputFile);

	for (const auto &entry : std::filesystem::recursive_directory_iterator(dir))
	{
		if (!std::filesystem::is_regular_file(entry))
			continue;

		const auto &path = entry.path();

		libbndl::Bundle bundle;

		if (!bundle.Load(path.string()))
		{
			auto ext = path.extension().string();
			std::ranges::transform(ext, ext.begin(), [](auto c) { return std::tolower(c, std::locale::classic()); });

			// If it looks like a bundle let's be vocal about it
			if (ext == ".bndl" || ext == ".bundle")
				std::cerr << "Failed to load likely bundle: " << path << '\n';

			continue;
		}

		if (!bundle.Save(outputFile.string()))
		{
			std::cerr << "Failed to save bundle: " << path << '\n';
		}
		else if (!identicalFiles(path, outputFile))
		{
			std::cerr << "Did not produce identical output on save: " << path << '\n';
		}

		std::filesystem::remove(outputFile);
	}
}
