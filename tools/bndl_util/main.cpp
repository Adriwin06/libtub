#include <libbndl/bundle.hpp>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <regex>
#include <cxxopts.hpp>
#include <pugixml.hpp>
#include "types.hpp"

using namespace libbndl;

static std::string cleanResourceNameForHash(const std::string &resourceName)
{
	if (std::regex_search(resourceName, std::regex("^[a-zA-Z]:\\\\")))
	{
		return std::filesystem::path(resourceName).filename().string();
	}

	return resourceName;
}

static std::string cleanResourceNameForIO(const std::string &resourceName)
{
	const auto gameDBStart = resourceName.find("gamedb://");
	if (gameDBStart != std::string::npos)
	{
		const auto basenameStart = resourceName.find_last_of("/") + 1;
		if (basenameStart < gameDBStart)
			return resourceName;
		auto basenameEnd = resourceName.find_first_of("?", basenameStart);
		if (basenameEnd == std::string::npos)
			basenameEnd = resourceName.find_first_of("#", basenameStart);
		std::smatch match;
		std::regex_search(resourceName.begin() + basenameEnd, resourceName.end(), match, std::regex("^(?:\\?ID=|#)\\d+"));
		return resourceName.substr(0, gameDBStart) + resourceName.substr(basenameStart, basenameEnd - basenameStart) + match.suffix().str();
	}

	return cleanResourceNameForHash(resourceName);
}

int main(int argc, char** argv)
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
		std::cout << "Please specify a bundle file." << std::endl << options.help() << std::endl;
		return EXIT_FAILURE;
	}

	if (parsedOptions.count("file") > 0 && parsedOptions.count("all") > 0)
	{
		std::cout << "The --file and --all arguments are mutually exclusive." << std::endl;
		return EXIT_FAILURE;
	}

	if (parsedOptions.count("all") > 0 && parsedOptions.count("extract") == 0)
	{
		std::cout << "--all can only be used in extract mode." << std::endl;
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
		std::cout << "Please specify exactly one operation that should be executed." << std::endl
		<< options.help() << std::endl;
		return EXIT_FAILURE;
	}

	Bundle arch;
	if (!pack)
	{
		if (list)
		{
			if (!arch.Load(file.value()))
			{
				std::cout << "Failed to open " << file.value() << std::endl;
				return EXIT_FAILURE;
			}

			std::cout.fill('-');
			std::cout << std::left << std::setw(70) << "NAME" << std::right << "FILE TYPE" << std::endl;
			std::cout.fill(' ');
			for (const auto &resourceID : arch.GetResourceIDs())
			{
				const auto debugInfo = arch.GetResourceDebugInfo(resourceID);
				const auto resourceType = *arch.GetResourceType(resourceID);
				std::ostringstream name;
				if (debugInfo)
					name << debugInfo->GetName();
				else
					name << std::hex << resourceID;
				std::ostringstream typeName;
				if (debugInfo)
					typeName << debugInfo->GetTypeName();
				else
					typeName << std::hex << resourceType;
				std::cout << std::left << std::setw(70) << name.str() << std::right << typeName.str() << std::endl;
			}
		}
		else if (extract)
		{
			std::cout << "Extracting..." << std::endl;

			std::vector<std::filesystem::path> archives;
			if (file.has_value())
				archives.emplace_back(file.value());
			else
				for (const auto &entry : std::filesystem::recursive_directory_iterator(all.value()))
					if (std::filesystem::is_regular_file(entry))
						archives.push_back(entry.path());

			for (const auto &archive : archives)
			{
				if (!arch.Load(archive.string()))
				{
					if (file.has_value())
					{
						std::cout << "Failed to open " << file.value() << std::endl;
						return EXIT_FAILURE;
					}
					continue;
				}

				std::filesystem::path archiveExtractDir;
				if (file.has_value())
					archiveExtractDir = extractDir;
				else
					archiveExtractDir = extractDir / std::filesystem::relative(archive, all.value()).replace_extension();

				try
				{
					std::filesystem::create_directories(archiveExtractDir);
				}
				catch (std::filesystem::filesystem_error &e)
				{
					std::cout << "Failed to create extract directory: " << e.what() << std::endl;
					return EXIT_FAILURE;
				}

				pugi::xml_document doc;
				auto configRoot = doc.append_child("Bundle");

				configRoot.append_attribute("version").set_value((std::to_string(static_cast<uint32_t>(arch.GetMagicNumber())) + "." + std::to_string(arch.GetVersion())).c_str());

				const auto platform = arch.GetPlatform();
				std::string platformName;
				switch (platform)
				{
				case Bundle::Platform::PC:
					platformName = "PC";
					break;
				case Bundle::Platform::Xbox360:
					platformName = "Xbox 360";
					break;
				case Bundle::Platform::PS3:
					platformName = "PS3";
					break;
				}
				configRoot.append_attribute("platform").set_value(platformName.c_str());

				const auto flags = arch.GetFlags();
				configRoot.append_attribute("compressed").set_value(!!(flags & Bundle::Flags::Compressed));
				if (arch.GetMagicNumber() == Bundle::MagicNumber::BND2)
				{
					configRoot.append_attribute("mainMemOptimised").set_value(!!(flags & Bundle::Flags::MainMemOptimised));
					configRoot.append_attribute("graphicsMemOptimised").set_value(!!(flags & Bundle::Flags::GraphicsMemOptimised));
				}
				configRoot.append_attribute("debugData").set_value(!!(flags & Bundle::Flags::HasResourceStringTable));

				std::ofstream manifest(archiveExtractDir / "_manifest.txt");
				manifest << "# This is a mapping of IDs to file names for your information. This file is not used for bundle packing and does not need to exist." << std::endl;

				for (const auto &resourceID : arch.GetResourceIDs())
				{
					const auto debugInfo = arch.GetResourceDebugInfo(resourceID);
					const auto resourceType = *arch.GetResourceType(resourceID);
					const auto data = *arch.GetResource(resourceID);

					std::ostringstream idStrStream;
					idStrStream << "0x" << std::hex << std::setw(8) << std::setfill('0') << std::uppercase << resourceID;

					std::ostringstream name;
					if (debugInfo)
					{
						const auto cleanedName = cleanResourceNameForIO(debugInfo->GetName());

						if (arch.GetResourceDebugInfo(cleanResourceNameForHash(debugInfo->GetName())) && std::all_of(cleanedName.begin(), cleanedName.end(), [](char c) { return std::isalnum(c) || c == '.' || c == '-' || c == '_' || c == '~' || c == '(' || c == ')' || c == ',' || c == '+' || c == ' '; }))
							name << cleanedName;
						else
							name << idStrStream.str();
					}
					else
					{
						name << idStrStream.str();
					}

					const auto it = g_fileTypeNames.find(resourceType);
					std::ostringstream pathTypeNameStream;
					if (it == g_fileTypeNames.end())
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
						std::cout << "Failed to create directory: " << e.what() << std::endl;
						return EXIT_FAILURE;
					}

					for (const auto& memoryType : arch.GetMemoryTypes())
					{
						const auto &buffer = data.GetBinary(memoryType);
						if (buffer == nullptr)
							continue;

						std::string typeExt;
						switch (memoryType)
						{
						case Bundle::MemoryType::MainMemory:
							typeExt = ".mainmem";
							break;
						case Bundle::MemoryType::GraphicsSystem:
							if (platform == Bundle::Platform::PS3)
								typeExt = ".gfxsysmem";
							else if (platform == Bundle::Platform::Xbox360)
								typeExt = ".physical";
							else
								typeExt = ".disposable";
							break;
						case Bundle::MemoryType::GraphicsLocal:
							if (platform == Bundle::Platform::PS3)
								typeExt = ".gfxlocalmem";
							else
								typeExt = ".dummy";
							break;
						}

						const auto path = fileExtractDir / (name.str() + typeExt + ".bin");
						std::ofstream outfile(path, std::ios::out | std::ios::binary);
						outfile.write(reinterpret_cast<const char *>(buffer.GetData()), buffer.GetSize());
						outfile.close();
						if (outfile.fail())
						{
							char errmsg[94];
							strerror_s(errmsg, sizeof(errmsg) / sizeof(errmsg[0]), errno);
							std::cout << "Failed to create extract " << path << ": " << errmsg << std::endl;
						}
					}

					const auto &imports = data.GetImports();
					if (!imports.empty())
					{
						pugi::xml_document importsDoc;
						auto importsRoot = importsDoc.append_child("Imports");

						for (const auto &import : imports)
						{
							auto entryChild = importsRoot.append_child("Import");
							const auto depDebugInfo = arch.GetResourceDebugInfo(import.GetResourceID());
							if (depDebugInfo)
							{
								entryChild.append_attribute("name").set_value(depDebugInfo->GetName().c_str());
							}
							else
							{
								std::ostringstream depIdStrStream;
								depIdStrStream << "0x" << std::hex << std::setw(8) << std::setfill('0') << std::uppercase << import.GetResourceID();
								entryChild.append_attribute("id").set_value(depIdStrStream.str().c_str());
							}

							std::ostringstream offsetStrStream;
							offsetStrStream << "0x" << std::hex << std::setw(8) << std::setfill('0') << std::uppercase << import.GetOffset();
							entryChild.append_attribute("offset").set_value(offsetStrStream.str().c_str());
						}

						std::ofstream outfile(fileExtractDir / (name.str() + ".imports.xml"));
						importsDoc.save(outfile, "\t", pugi::format_indent | pugi::format_no_declaration, pugi::encoding_utf8);
					}

					auto entryChild = configRoot.append_child("Resource");
					if (!debugInfo || debugInfo->GetName().empty())
						entryChild.append_attribute("id").set_value(idStrStream.str().c_str());
					else
						entryChild.append_attribute("name").set_value(debugInfo->GetName().c_str());
					if (!debugInfo || debugInfo->GetTypeName().empty())
						entryChild.append_attribute("typeDebugName").set_value(pathTypeNameStream.str().c_str());
					else
						entryChild.append_attribute("typeDebugName").set_value(debugInfo->GetTypeName().c_str());

					auto debugName = std::string();
					if (debugInfo && !debugInfo->GetName().empty())
						debugName = debugInfo->GetName();
					manifest << idStrStream.str() << " = " << debugName << std::endl;
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
			std::cout << "The path supplied for packing is not a directory." << std::endl;
			return EXIT_FAILURE;
		}

		const auto configPath = packDir / "_config.xml";
		if (!std::filesystem::is_regular_file(configPath) && !std::filesystem::is_symlink(configPath))
		{
			std::cout << "Could not find a _config.xml file in the directory to pack." << std::endl;
			return EXIT_FAILURE;
		}

		// TODO: check for overwrite?

		std::vector<std::filesystem::path> resourceFilePaths;
	}

	return 0;
}
