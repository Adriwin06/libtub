#include <libbndl/bundle.hpp>
#include <iostream>
#include <iomanip>
#include <cxxopts.hpp>

using namespace libbndl;

int main(int argc, char** argv)
{
	cxxopts::Options options("bndl_util", "A program to work with Burnout Paradise bundle archives.");
	options.add_options()
		("e,extract", "Extract the archive")
		("p,pack", "Pack a folder structure to a bundle archive")
		("f,file", "Name of the archive that should be extracted/generated", cxxopts::value<std::string>())
		("s,search", "Search for an entry", cxxopts::value<std::string>())
		("l,list", "List all entries");

	auto parsedOptions = options.parse(argc, argv);
	if (parsedOptions.count("file") == 0)
	{
		std::cout << "Please specify an input file." << std::endl << options.help() << std::endl;
		return EXIT_FAILURE;
	}

	bool extract = parsedOptions["pack"].as<bool>();
	bool pack = parsedOptions["pack"].as<bool>();
	bool list = parsedOptions["list"].as<bool>();
	std::string file = parsedOptions["file"].as<std::string>();
	std::string search = parsedOptions["search"].as<std::string>();
	bool bsearch = search.size() > 0;
	
	if ((pack + extract + list + bsearch) != 1)
	{
		std::cout << "Please specify exactly one operation that should be executed." << std::endl
		<< options.help() << std::endl;
		return EXIT_FAILURE;
	}

	Bundle arch;
	if (!pack)
	{
		if (!arch.Load(file))
		{
			std::cout << "Failed to open " << file << std::endl;
			return EXIT_FAILURE;
		}

		if (list)
		{
			std::cout.fill('-');
			std::cout << std::left << std::setw(70) << "NAME" << std::right << "FILE TYPE" << std::endl;
			std::cout.fill(' ');
			for (const auto &resourceID : arch.ListResourceIDs())
			{
				const auto debugInfo = arch.GetDebugInfo(resourceID);
				const auto resourceType = *arch.GetResourceType(resourceID);
				std::ostringstream name;
				if (debugInfo)
					name << debugInfo->name;
				else
					name << std::hex << resourceID;
				std::ostringstream typeName;
				if (debugInfo)
					typeName << debugInfo->typeName;
				else
					typeName << std::hex << resourceType;
				std::cout << std::left << std::setw(70) << name.str() << std::right << typeName.str() << std::endl;
			}
		}
	}

	return 0;
}
