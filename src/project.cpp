#include <libtub/bundle.hpp>
#include <yaml-cpp/yaml.h>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <format>
#include <fstream>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <system_error>
#include <vector>

using namespace libtub;

namespace
{
	using ResourceKey = std::pair<ResourceID, uint8_t>;

	struct ResourceKeyLess
	{
		bool operator()(const ResourceKey &lhs, const ResourceKey &rhs) const
		{
			return lhs < rhs;
		}
	};

	std::string FormatResourceID(ResourceID resourceID)
	{
		const auto rawID = static_cast<uint64_t>(resourceID);
		const auto width = (resourceID.GetIDType() != ResourceID::IDType::Normal || rawID > 0xFFFFFFFFULL) ? 16 : 8;
		return std::format("0x{:0{}x}", rawID, width);
	}

	std::string FormatUint32(uint32_t value)
	{
		return std::format("0x{:08x}", value);
	}

	template <typename T>
	std::optional<T> ParseUnsignedScalar(const YAML::Node &node)
	{
		if (!node || !node.IsScalar())
			return {};

		try
		{
			const auto &scalar = node.Scalar();
			int base = 10;
			size_t index = 0;
			if (scalar.starts_with("0x") || scalar.starts_with("0X"))
			{
				base = 16;
				index = 2;
			}

			return static_cast<T>(std::stoull(scalar.substr(index), nullptr, base));
		}
		catch (const std::exception &)
		{
			return {};
		}
	}

	std::optional<int32_t> ParseSignedScalar(const YAML::Node &node)
	{
		if (!node || !node.IsScalar())
			return {};

		try
		{
			return static_cast<int32_t>(std::stoll(node.Scalar(), nullptr, 10));
		}
		catch (const std::exception &)
		{
			return {};
		}
	}

	std::optional<bool> ParseBoolScalar(const YAML::Node &node)
	{
		if (!node || !node.IsScalar())
			return {};

		try
		{
			return node.as<bool>();
		}
		catch (const std::exception &)
		{
			return {};
		}
	}

	std::optional<ResourceID> ParseResourceID(const YAML::Node &node)
	{
		const auto rawID = ParseUnsignedScalar<uint64_t>(node);
		if (!rawID)
			return {};

		return ResourceID(*rawID);
	}

	std::optional<Magic> ParseMagic(const YAML::Node &node)
	{
		if (!node || !node.IsScalar())
			return {};

		const auto value = node.Scalar();
		if (value == "bndl")
			return Magic::Bndl;
		if (value == "bnd2")
			return Magic::Bnd2;
		return {};
	}

	std::string MagicToString(Magic magic)
	{
		switch (magic)
		{
		case Magic::Bndl:
			return "bndl";
		case Magic::Bnd2:
			return "bnd2";
		default:
			return "unknown";
		}
	}

	std::optional<Platform> ParsePlatform(const YAML::Node &node)
	{
		if (!node || !node.IsScalar())
			return {};

		const auto value = node.Scalar();
		if (value == "pc")
			return Platform::PC;
		if (value == "xbox360")
			return Platform::Xbox360;
		if (value == "ps3")
			return Platform::PS3;
		if (value == "psvita")
			return Platform::PSVita;
		if (value == "wiiu")
			return Platform::WiiU;

		const auto numeric = ParseUnsignedScalar<uint16_t>(node);
		if (!numeric)
			return {};

		return static_cast<Platform>(*numeric);
	}

	std::string PlatformToString(Platform platform)
	{
		switch (platform)
		{
		case Platform::PC:
			return "pc";
		case Platform::Xbox360:
			return "xbox360";
		case Platform::PS3:
			return "ps3";
		case Platform::PSVita:
			return "psvita";
		case Platform::WiiU:
			return "wiiu";
		default:
			return "unknown";
		}
	}

	std::string MemoryTypeToString(MemoryType memoryType)
	{
		if (memoryType == MemoryType::MainMemory)
			return "mainMemory";
		if (memoryType == MemoryType::Physical)
			return "physical";
		if (memoryType == MemoryType::Mem1)
			return "mem1";
		if (memoryType == MemoryType::GraphicsSystem)
			return "graphicsSystem";
		if (memoryType == MemoryType::GraphicsMem2)
			return "graphicsMem2";
		if (memoryType == MemoryType::GraphicsLocal)
			return "graphicsLocal";
		if (memoryType == MemoryType::Disposable)
			return "disposable";
		return "unknown";
	}

	std::optional<Import::ImportType> ParseImportType(const YAML::Node &node)
	{
		if (!node || !node.IsScalar())
			return {};

		const auto value = node.Scalar();
		if (value == "pointer")
			return Import::ImportType::Pointer;
		if (value == "resourceHandle")
			return Import::ImportType::ResourceHandle;
		return {};
	}

	std::string ImportTypeToString(Import::ImportType importType)
	{
		switch (importType)
		{
		case Import::ImportType::Pointer:
			return "pointer";
		case Import::ImportType::ResourceHandle:
			return "resourceHandle";
		default:
			return "pointer";
		}
	}

	std::string SanitisePathComponent(std::string value)
	{
		std::replace(value.begin(), value.end(), ' ', '_');
		for (auto &character : value)
		{
			const auto unsignedCharacter = static_cast<unsigned char>(character);
			if (std::isalnum(unsignedCharacter) || character == '_' || character == '-')
				continue;

			character = '_';
		}

		if (value.empty())
			value = "resource";
		return value;
	}

	std::string TypeFolderName(const ResourceDescriptor &resource)
	{
		auto folder = std::format("type_{:08x}", resource.resourceType);
		if (resource.debugData && !resource.debugData->GetTypeName().empty())
			folder += "_" + SanitisePathComponent(resource.debugData->GetTypeName());

		return folder;
	}

	std::string ResourceStem(const ResourceDescriptor &resource)
	{
		return std::format("{}.s{}", FormatResourceID(resource.resourceID), resource.streamIndex);
	}

	bool WriteBinaryFile(const std::filesystem::path &path, const Buffer &buffer)
	{
		if (path.has_parent_path())
		{
			std::error_code error;
			std::filesystem::create_directories(path.parent_path(), error);
			if (error)
				return false;
		}

		std::ofstream stream(path, std::ios::binary | std::ios::trunc);
		if (!stream)
			return false;

		stream.write(reinterpret_cast<const char *>(buffer.GetData()), static_cast<std::streamsize>(buffer.GetSize()));
		return static_cast<bool>(stream);
	}

	std::optional<std::vector<uint8_t>> ReadBinaryFile(const std::filesystem::path &path)
	{
		std::ifstream stream(path, std::ios::binary | std::ios::ate);
		if (!stream)
			return {};

		const auto size = stream.tellg();
		if (size < 0)
			return {};

		stream.seekg(0, std::ios::beg);
		std::vector<uint8_t> data(static_cast<size_t>(size));
		if (!data.empty())
			stream.read(reinterpret_cast<char *>(data.data()), static_cast<std::streamsize>(size));

		if (!stream && !data.empty())
			return {};

		return data;
	}

	Buffer MakeBuffer(const std::vector<uint8_t> &data, uint32_t alignment)
	{
		auto buffer = std::make_unique_for_overwrite<uint8_t[]>(data.size());
		if (!data.empty())
			std::memcpy(buffer.get(), data.data(), data.size());

		return { std::move(buffer), data.size(), alignment };
	}

	YAML::Node ExportImports(const std::vector<Import> &imports)
	{
		YAML::Node node(YAML::NodeType::Sequence);
		for (const auto &importEntry : imports)
		{
			YAML::Node item;
			item["resource"] = FormatResourceID(importEntry.GetResourceID());
			item["offset"] = FormatUint32(importEntry.GetOffset());
			item["kind"] = ImportTypeToString(importEntry.GetImportType());
			node.push_back(item);
		}
		return node;
	}

	std::optional<std::vector<Import>> ParseImportList(const YAML::Node &node)
	{
		if (!node)
			return std::vector<Import>{};

		YAML::Node importList = node;
		if (node.IsMap() && node["imports"])
			importList = node["imports"];

		if (!importList.IsSequence())
			return {};

		std::vector<Import> imports;
		imports.reserve(importList.size());
		for (const auto &entry : importList)
		{
			std::optional<ResourceID> resourceID;
			std::optional<uint32_t> offset;
			auto importType = Import::ImportType::Pointer;

			if (entry.IsMap() && (entry["resource"] || entry["id"]))
			{
				resourceID = ParseResourceID(entry["resource"] ? entry["resource"] : entry["id"]);
				offset = ParseUnsignedScalar<uint32_t>(entry["offset"]);

				const auto parsedImportType = ParseImportType(entry["kind"]);
				if (parsedImportType)
					importType = *parsedImportType;
			}
			else if (entry.IsMap() && entry.size() == 1)
			{
				const auto &pair = *entry.begin();
				offset = ParseUnsignedScalar<uint32_t>(pair.first);
				resourceID = ParseResourceID(pair.second);
			}
			else
			{
				return {};
			}

			if (!resourceID || !offset)
				return {};

			imports.emplace_back(*resourceID, *offset, importType);
		}

		return imports;
	}

	YAML::Node BuildFlagsNode(Flags flags)
	{
		YAML::Node node;
		node["compressed"] = static_cast<bool>(flags & Flags::Compressed);
		node["mainMemOptimised"] = static_cast<bool>(flags & Flags::MainMemOptimised);
		node["graphicsMemOptimised"] = static_cast<bool>(flags & Flags::GraphicsMemOptimised);
		node["hasDebugData"] = static_cast<bool>(flags & Flags::HasDebugData);
		node["nonAsyncFixupRequired"] = static_cast<bool>(flags & Flags::NonAsynchFixupRequired);
		node["multistreamBundle"] = static_cast<bool>(flags & Flags::MultistreamBundle);
		node["deltaBundle"] = static_cast<bool>(flags & Flags::DeltaBundle);
		node["containsDefaultResource"] = static_cast<bool>(flags & Flags::ContainsDefaultResource);
		return node;
	}

	Flags ParseFlagsNode(const YAML::Node &bundleNode)
	{
		Flags flags;
		const auto flagNode = bundleNode["flags"];
		if (flagNode && flagNode.IsMap())
		{
			if (ParseBoolScalar(flagNode["compressed"]).value_or(false))
				flags |= Flags::Compressed;
			if (ParseBoolScalar(flagNode["mainMemOptimised"]).value_or(false))
				flags |= Flags::MainMemOptimised;
			if (ParseBoolScalar(flagNode["graphicsMemOptimised"]).value_or(false))
				flags |= Flags::GraphicsMemOptimised;
			if (ParseBoolScalar(flagNode["hasDebugData"]).value_or(false))
				flags |= Flags::HasDebugData;
			if (ParseBoolScalar(flagNode["nonAsyncFixupRequired"]).value_or(false))
				flags |= Flags::NonAsynchFixupRequired;
			if (ParseBoolScalar(flagNode["multistreamBundle"]).value_or(false))
				flags |= Flags::MultistreamBundle;
			if (ParseBoolScalar(flagNode["deltaBundle"]).value_or(false))
				flags |= Flags::DeltaBundle;
			if (ParseBoolScalar(flagNode["containsDefaultResource"]).value_or(false))
				flags |= Flags::ContainsDefaultResource;
			return flags;
		}

		if (ParseBoolScalar(bundleNode["compressed"]).value_or(false))
			flags |= Flags::Compressed;
		if (ParseBoolScalar(bundleNode["mainMemOptimised"]).value_or(false))
			flags |= Flags::MainMemOptimised;
		if (ParseBoolScalar(bundleNode["graphicsMemOptimised"]).value_or(false))
			flags |= Flags::GraphicsMemOptimised;
		if (ParseBoolScalar(bundleNode["hasDebugData"]).value_or(false))
			flags |= Flags::HasDebugData;

		return flags;
	}

	bool WriteYamlFile(const std::filesystem::path &path, const YAML::Node &node)
	{
		if (path.has_parent_path())
		{
			std::error_code error;
			std::filesystem::create_directories(path.parent_path(), error);
			if (error)
				return false;
		}

		YAML::Emitter emitter;
		emitter.SetIndent(2);
		emitter << node;

		std::ofstream stream(path, std::ios::binary | std::ios::trunc);
		if (!stream)
			return false;

		stream.write(emitter.c_str(), static_cast<std::streamsize>(std::strlen(emitter.c_str())));
		return static_cast<bool>(stream);
	}

	std::map<ResourceKey, std::vector<Import>, ResourceKeyLess> LoadCombinedImports(const std::filesystem::path &path)
	{
		std::map<ResourceKey, std::vector<Import>, ResourceKeyLess> importsByResource;
		if (!std::filesystem::exists(path))
			return importsByResource;

		const auto root = YAML::LoadFile(path.string());
		if (root["resources"] && root["resources"].IsSequence())
		{
			for (const auto &resourceNode : root["resources"])
			{
				const auto resourceID = ParseResourceID(resourceNode["id"]);
				const auto streamIndex = ParseUnsignedScalar<uint8_t>(resourceNode["streamIndex"]).value_or(0);
				const auto imports = ParseImportList(resourceNode["imports"]);
				if (!resourceID || !imports)
					continue;

				importsByResource[{ *resourceID, streamIndex }] = *imports;
			}
			return importsByResource;
		}

		if (root.IsMap())
		{
			for (const auto &resourceNode : root)
			{
				const auto resourceID = ParseResourceID(resourceNode.first);
				const auto imports = ParseImportList(resourceNode.second);
				if (!resourceID || !imports)
					continue;

				importsByResource[{ *resourceID, 0 }] = *imports;
			}
		}

		return importsByResource;
	}
}

bool Bundle::ExportProject(const std::filesystem::path &directory, const ProjectExportOptions &options) const
{
	if (!m_impl)
		return false;

	std::error_code error;
	std::filesystem::create_directories(directory, error);
	if (error)
		return false;

	YAML::Node root;
	root["schemaVersion"] = 1;

	YAML::Node bundleNode;
	bundleNode["magic"] = MagicToString(GetMagic());
	bundleNode["version"] = GetVersion();
	bundleNode["platform"] = PlatformToString(GetPlatform());
	bundleNode["flags"] = BuildFlagsNode(GetFlags());

	const auto defaultStreamIndex = GetDefaultResourceStreamIndex();
	if (defaultStreamIndex >= 0 || GetDefaultResourceID() != ResourceID(0))
	{
		YAML::Node defaultResource;
		defaultResource["id"] = FormatResourceID(GetDefaultResourceID());
		defaultResource["streamIndex"] = defaultStreamIndex;
		bundleNode["defaultResource"] = defaultResource;
	}

	YAML::Node streamNodes(YAML::NodeType::Sequence);
	for (uint8_t index = 0; index < 4; ++index)
	{
		const auto streamName = GetStreamName(index);
		if (streamName.empty())
			continue;

		YAML::Node streamNode;
		streamNode["index"] = static_cast<uint32_t>(index);
		streamNode["name"] = streamName;
		streamNodes.push_back(streamNode);
	}
	if (streamNodes.size() > 0)
		bundleNode["streams"] = streamNodes;

	root["bundle"] = bundleNode;

	YAML::Node resourcesNode(YAML::NodeType::Sequence);
	YAML::Node combinedImportsRoot;
	combinedImportsRoot["resources"] = YAML::Node(YAML::NodeType::Sequence);

	for (const auto &resource : DescribeResources())
	{
		YAML::Node resourceNode;
		resourceNode["id"] = FormatResourceID(resource.resourceID);
		resourceNode["streamIndex"] = static_cast<uint32_t>(resource.streamIndex);
		resourceNode["type"] = FormatUint32(resource.resourceType);

		if (resource.debugData)
		{
			resourceNode["name"] = resource.debugData->GetName();
			resourceNode["typeName"] = resource.debugData->GetTypeName();
		}

		YAML::Node binariesNode;
		const auto resourceFolder = options.sortByType ? std::filesystem::path(TypeFolderName(resource)) : std::filesystem::path();
		const auto resourceStem = ResourceStem(resource);
		for (const auto &block : resource.memoryBlocks)
		{
			const auto buffer = GetBinary(resource.resourceID, block.memoryType, resource.streamIndex);
			if (buffer == nullptr)
				continue;

			const auto fileName = resourceStem + "." + MemoryTypeToString(block.memoryType) + ".bin";
			const auto relativePath = resourceFolder / fileName;
			if (!WriteBinaryFile(directory / relativePath, buffer))
				return false;

			YAML::Node blockNode;
			blockNode["path"] = relativePath.generic_string();
			blockNode["alignment"] = FormatUint32(block.alignment);
			binariesNode[MemoryTypeToString(block.memoryType)] = blockNode;
		}
		resourceNode["binaries"] = binariesNode;

		if (!resource.imports.empty())
		{
			if (options.combineImports)
			{
				YAML::Node combinedEntry;
				combinedEntry["id"] = FormatResourceID(resource.resourceID);
				combinedEntry["streamIndex"] = static_cast<uint32_t>(resource.streamIndex);
				combinedEntry["imports"] = ExportImports(resource.imports);
				combinedImportsRoot["resources"].push_back(combinedEntry);
			}
			else
			{
				const auto importPath = resourceFolder / (resourceStem + ".imports.yaml");
				if (!WriteYamlFile(directory / importPath, ExportImports(resource.imports)))
					return false;

				resourceNode["imports"] = importPath.generic_string();
			}
		}

		resourcesNode.push_back(resourceNode);
	}

	root["resources"] = resourcesNode;

	if (!WriteYamlFile(directory / ".meta.yaml", root))
		return false;

	if (options.combineImports && combinedImportsRoot["resources"].size() > 0)
		return WriteYamlFile(directory / ".imports.yaml", combinedImportsRoot);

	return true;
}

bool Bundle::ImportProject(const std::filesystem::path &directory)
{
	try
	{
		const auto metadataPath = directory / ".meta.yaml";
		if (!std::filesystem::exists(metadataPath))
			return false;

		const auto root = YAML::LoadFile(metadataPath.string());
		const auto bundleNode = root["bundle"];
		const auto resourcesNode = root["resources"];
		if (!bundleNode || !resourcesNode)
			return false;

		const auto magic = ParseMagic(bundleNode["magic"]);
		const auto version = ParseUnsignedScalar<uint16_t>(bundleNode["version"]);
		const auto platform = ParsePlatform(bundleNode["platform"]);
		if (!magic || !version || !platform)
			return false;

		Bundle imported(*magic, *version, *platform, ParseFlagsNode(bundleNode));

		if (bundleNode["streams"] && bundleNode["streams"].IsSequence())
		{
			for (const auto &streamNode : bundleNode["streams"])
			{
				const auto index = ParseUnsignedScalar<uint8_t>(streamNode["index"]);
				if (!index || !streamNode["name"] || !streamNode["name"].IsScalar())
					return false;

				if (!imported.SetStreamName(*index, streamNode["name"].Scalar()))
					return false;
			}
		}

		const auto combinedImports = LoadCombinedImports(directory / ".imports.yaml");
		if (!resourcesNode.IsSequence())
			return false;

		for (const auto &resourceNode : resourcesNode)
		{
			const auto resourceID = ParseResourceID(resourceNode["id"]);
			const auto streamIndex = ParseUnsignedScalar<uint8_t>(resourceNode["streamIndex"]).value_or(0);
			const auto resourceType = ParseUnsignedScalar<uint32_t>(resourceNode["type"]);
			if (!resourceID || !resourceType)
				return false;

			Resource resource(*resourceType);
			const auto binariesNode = resourceNode["binaries"];
			if (!binariesNode || !binariesNode.IsMap())
				return false;

			for (const auto &memoryType : imported.GetMemoryTypes())
			{
				const auto blockNode = binariesNode[MemoryTypeToString(memoryType)];
				if (!blockNode)
					continue;

				std::filesystem::path relativePath;
				uint32_t alignment = 1;
				if (blockNode.IsScalar())
				{
					relativePath = blockNode.Scalar();
				}
				else
				{
					if (!blockNode["path"] || !blockNode["path"].IsScalar())
						return false;

					relativePath = blockNode["path"].Scalar();
					alignment = ParseUnsignedScalar<uint32_t>(blockNode["alignment"]).value_or(1);
				}

				const auto bufferData = ReadBinaryFile(directory / relativePath);
				if (!bufferData)
					return false;

				resource.ReplaceBinary(memoryType, MakeBuffer(*bufferData, alignment));
			}

			std::vector<Import> imports;
			if (resourceNode["imports"])
			{
				if (resourceNode["imports"].IsScalar())
				{
					const auto importRoot = YAML::LoadFile((directory / resourceNode["imports"].Scalar()).string());
					const auto parsedImports = ParseImportList(importRoot);
					if (!parsedImports)
						return false;

					imports = *parsedImports;
				}
				else
				{
					const auto parsedImports = ParseImportList(resourceNode["imports"]);
					if (!parsedImports)
						return false;

					imports = *parsedImports;
				}
			}
			else
			{
				const auto combinedImportIt = combinedImports.find({ *resourceID, streamIndex });
				if (combinedImportIt != combinedImports.end())
					imports = combinedImportIt->second;
			}

			for (const auto &importEntry : imports)
				resource.AddImport(importEntry);

			if (!imported.AddResource(*resourceID, resource, streamIndex))
				return false;

			if (resourceNode["name"] || resourceNode["typeName"])
			{
				const auto name = resourceNode["name"] ? resourceNode["name"].Scalar() : "";
				const auto typeName = resourceNode["typeName"] ? resourceNode["typeName"].Scalar() : "";
				if (!imported.AddResourceDebugData(*resourceID, ResourceDebugData(name, typeName), streamIndex))
					return false;
			}
		}

		if (bundleNode["defaultResource"])
		{
			const auto defaultResourceID = ParseResourceID(bundleNode["defaultResource"]["id"]);
			const auto defaultStreamIndex = ParseSignedScalar(bundleNode["defaultResource"]["streamIndex"]).value_or(0);
			if (!defaultResourceID || !imported.SetDefaultResource(*defaultResourceID, defaultStreamIndex))
				return false;
		}

		*this = std::move(imported);
		return true;
	}
	catch (const std::exception &)
	{
		return false;
	}
}
