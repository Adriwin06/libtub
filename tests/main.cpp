#include <libtub/bundle.h>
#include <libtub/bundle.hpp>
#include <libtub/builder.hpp>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace
{
	libtub::Buffer MakeBuffer(const std::vector<uint8_t> &bytes, uint32_t alignment = 1)
	{
		auto data = std::make_unique_for_overwrite<uint8_t[]>(bytes.size());
		if (!bytes.empty())
			std::memcpy(data.get(), bytes.data(), bytes.size());

		return { std::move(data), bytes.size(), alignment };
	}

	bool Expect(bool condition, const std::string &message)
	{
		if (!condition)
			std::cerr << message << '\n';

		return condition;
	}

	bool ExpectBytes(const libtub::Buffer &buffer, const std::vector<uint8_t> &expected, const std::string &message)
	{
		if (!Expect(buffer.GetSize() == expected.size(), message + ": size mismatch"))
			return false;

		if (buffer.GetSize() == 0)
			return true;

		return Expect(std::memcmp(buffer.GetData(), expected.data(), expected.size()) == 0, message + ": content mismatch");
	}

	libtub::Bundle MakeReferenceBundle(libtub::Platform platform = libtub::Platform::PC)
	{
		using namespace libtub;

		auto bundle = Bundle(Magic::Bnd2, 5, platform, Flags::HasDebugData);
		bundle.SetStreamName(0, "base");
		bundle.SetStreamName(1, "alt");

		const ResourceID dependencyA("dependency_a");
		Resource dependencyResourceA(ResourceType::NeedForSpeed::BinaryFile);
		dependencyResourceA.ReplaceBinary(MemoryType::MainMemory, MakeBuffer({ 0xAA, 0xBB, 0xCC }, 4));
		bundle.AddResource(dependencyA, dependencyResourceA, 0);
		bundle.AddResourceDebugData(dependencyA, ResourceDebugData("dependency_a.bin", "BinaryFile"), 0);

		const ResourceID dependencyB("dependency_b");
		Resource dependencyResourceB(ResourceType::NeedForSpeed::BinaryFile);
		dependencyResourceB.ReplaceBinary(MemoryType::MainMemory, MakeBuffer({ 0x11, 0x22 }, 4));
		bundle.AddResource(dependencyB, dependencyResourceB, 0);
		bundle.AddResourceDebugData(dependencyB, ResourceDebugData("dependency_b.bin", "BinaryFile"), 0);

		const ResourceID mainResourceID("main_resource");
		Resource mainResource(ResourceType::NeedForSpeed::Renderable);
		mainResource.ReplaceBinary(MemoryType::MainMemory, MakeBuffer({ 0x10, 0x20, 0x30, 0x40, 0x50 }, 16));
		mainResource.ReplaceBinary(MemoryType::Disposable, MakeBuffer({ 0xDE, 0xAD, 0xBE, 0xEF }, 16));
		mainResource.AddImport(Import(dependencyA, 0x00000004, Import::ImportType::Pointer));
		mainResource.AddImport(Import(dependencyB, 0x00000008, Import::ImportType::ResourceHandle));
		bundle.AddResource(mainResourceID, mainResource, 1);
		bundle.AddResourceDebugData(mainResourceID, ResourceDebugData("main_resource.bin", "Renderable"), 1);
		bundle.SetDefaultResource(mainResourceID, 1);

		return bundle;
	}

	bool VerifyBundleState(const libtub::Bundle &bundle, const std::string &label)
	{
		using namespace libtub;

		const ResourceID mainResourceID("main_resource");
		const auto resource = bundle.GetResource(mainResourceID, 1);
		if (!Expect(resource.has_value(), label + ": missing streamed resource"))
			return false;

		bool ok = true;
		ok &= Expect(bundle.GetDefaultResourceID() == mainResourceID, label + ": default resource ID mismatch");
		ok &= Expect(bundle.GetDefaultResourceStreamIndex() == 1, label + ": default resource stream mismatch");
		ok &= Expect(bundle.GetStreamName(0) == "base", label + ": stream 0 name mismatch");
		ok &= Expect(bundle.GetStreamName(1) == "alt", label + ": stream 1 name mismatch");
		ok &= Expect(static_cast<bool>(bundle.GetFlags() & libtub::Flags::MultistreamBundle), label + ": multistream flag missing");

		const auto bundleBinary = bundle.GetBinary(mainResourceID, MemoryType::MainMemory, 1);
		ok &= ExpectBytes(bundleBinary, { 0x10, 0x20, 0x30, 0x40, 0x50 }, label + ": exported main memory");

		const auto disposableBinary = bundle.GetBinary(mainResourceID, MemoryType::Disposable, 1);
		ok &= ExpectBytes(disposableBinary, { 0xDE, 0xAD, 0xBE, 0xEF }, label + ": exported disposable memory");

		const auto &resourceMainBinary = resource->GetBinary(MemoryType::MainMemory);
		ok &= ExpectBytes(resourceMainBinary, { 0x10, 0x20, 0x30, 0x40, 0x50 }, label + ": resource main memory");

		const auto &imports = resource->GetImports();
		ok &= Expect(imports.size() == 2, label + ": import count mismatch");
		if (imports.size() == 2)
		{
			ok &= Expect(imports[0].GetResourceID() == ResourceID("dependency_a"), label + ": pointer import ID mismatch");
			ok &= Expect(imports[0].GetOffset() == 0x00000004, label + ": pointer import offset mismatch");
			ok &= Expect(imports[0].GetImportType() == Import::ImportType::Pointer, label + ": pointer import kind mismatch");
			ok &= Expect(imports[1].GetResourceID() == ResourceID("dependency_b"), label + ": resource handle import ID mismatch");
			ok &= Expect(imports[1].GetOffset() == 0x00000008, label + ": resource handle import offset mismatch");
			ok &= Expect(imports[1].GetImportType() == Import::ImportType::ResourceHandle, label + ": resource handle import kind mismatch");
		}

		return ok;
	}

	bool TestBnd2ImportRoundTrip(libtub::Platform platform)
	{
		auto bundle = MakeReferenceBundle(platform);
		const auto bytes = bundle.SaveToMemory();
		if (!Expect(!bytes.empty(), "memory round-trip: failed to serialize bundle"))
			return false;

		libtub::Bundle reloaded;
		if (!Expect(reloaded.Load(std::span<const uint8_t>(bytes)), "memory round-trip: failed to load serialized bundle"))
			return false;

		return VerifyBundleState(reloaded, "memory round-trip platform " + std::to_string(static_cast<uint16_t>(platform)));
	}

	bool TestBnd2ImportRoundTrips()
	{
		bool ok = true;
		ok &= TestBnd2ImportRoundTrip(libtub::Platform::PC);
		ok &= TestBnd2ImportRoundTrip(libtub::Platform::Xbox360);
		ok &= TestBnd2ImportRoundTrip(libtub::Platform::PS3);
		return ok;
	}

	bool TestProjectRoundTrip()
	{
		auto bundle = MakeReferenceBundle();
		const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
		const auto directory = std::filesystem::temp_directory_path() / ("libtub-project-test-" + std::to_string(stamp));

		std::error_code error;
		std::filesystem::remove_all(directory, error);

		const bool exported = bundle.ExportProject(directory);
		const bool exportVerified = Expect(exported, "project round-trip: failed to export project");

		libtub::Bundle imported;
		const bool importedOk = exported && Expect(imported.ImportProject(directory), "project round-trip: failed to import project");
		const bool verified = importedOk && VerifyBundleState(imported, "project round-trip");

		if (exportVerified && importedOk && verified)
		{
			std::filesystem::remove_all(directory, error);
		}
		else
		{
			std::cerr << "project round-trip: preserved failing export at " << directory.string() << '\n';
		}

		return exportVerified && importedOk && verified;
	}

	bool TestBundleBuilder()
	{
		using namespace libtub;

		BundleBuilder builder(BundleProfiles::NeedForSpeedHotPursuitPC());
		builder.SetStreamName(0, "base");

		const ResourceID resourceID("builder_resource");
		const std::vector<uint8_t> bytes{ 0xBA, 0xAD, 0xF0, 0x0D };
		if (!Expect(builder.AddResource(resourceID, ResourceType::NeedForSpeed::BinaryFile)
				.MainMemory(std::span<const uint8_t>(bytes), 4)
				.DebugData("builder_resource.bin", "BinaryFile")
				.Commit(), "builder: failed to commit resource"))
		{
			return false;
		}

		if (!Expect(builder.SetDefaultResource(resourceID), "builder: failed to set default resource"))
			return false;

		const auto saved = builder.SaveToMemory();
		if (!Expect(!saved.empty(), "builder: failed to serialize bundle"))
			return false;

		Bundle reloaded;
		if (!Expect(reloaded.Load(std::span<const uint8_t>(saved)), "builder: failed to reload serialized bundle"))
			return false;

		bool ok = true;
		ok &= Expect(reloaded.GetStreamName(0) == "base", "builder: stream name mismatch");
		ok &= Expect(reloaded.GetDefaultResourceID() == resourceID, "builder: default resource mismatch");
		ok &= ExpectBytes(reloaded.GetBinary(resourceID, MemoryType::MainMemory), bytes, "builder: main memory mismatch");
		return ok;
	}

	bool TestBundleBuilderValidation()
	{
		using namespace libtub;

		BundleBuilder builder(BundleProfiles::NeedForSpeedHotPursuitPC());
		const ResourceID resourceID("invalid_builder_resource");
		const std::vector<uint8_t> bytes{ 0x01, 0x02, 0x03 };
		auto resource = builder.AddResource(resourceID, ResourceType::NeedForSpeed::BinaryFile);
		const bool committed = resource.GraphicsSystem(std::span<const uint8_t>(bytes), 4).Commit();

		bool ok = true;
		ok &= Expect(!committed, "builder validation: invalid memory type was accepted");
		ok &= Expect(!resource.GetLastErrorMessage().empty(), "builder validation: missing error message");
		return ok;
	}

	bool TestCApiBinaryErrors()
	{
		libtub_bundle *bundle = nullptr;
		if (!Expect(libtub_create(&bundle, LIBTUB_MAGIC_BND2, 5, LIBTUB_PLATFORM_PC, LIBTUB_FLAGS_HAS_DEBUG_DATA) == LIBTUB_ERROR_SUCCESS, "C API: create failed"))
			return false;

		libtub_resource *resource = nullptr;
		libtub_buffer *input = nullptr;
		libtub_buffer *output = nullptr;
		bool ok = true;

		const std::vector<uint8_t> bytes{ 0xCA, 0xFE, 0xBA, 0xBE };
		const auto resourceID = libtub_resource_id_from_name("c_api_resource");
		ok &= Expect(libtub_resource_create(&resource, LIBTUB_RESOURCE_TYPE_NFS_BINARY_FILE) == LIBTUB_ERROR_SUCCESS, "C API: resource create failed");
		ok &= Expect(libtub_buffer_create(&input, bytes.data(), bytes.size(), 4) == LIBTUB_ERROR_SUCCESS, "C API: buffer create failed");
		ok &= Expect(libtub_resource_replace_binary(resource, input, LIBTUB_MEMORY_TYPE_MAIN_MEMORY) == LIBTUB_ERROR_SUCCESS, "C API: replace binary failed");
		ok &= Expect(libtub_add_resource(bundle, resourceID, resource, 0) == LIBTUB_ERROR_SUCCESS, "C API: add resource failed");
		ok &= Expect(libtub_copy_binary(bundle, &output, resourceID, LIBTUB_MEMORY_TYPE_MAIN_MEMORY, 0) == LIBTUB_ERROR_SUCCESS, "C API: copy binary failed");
		if (output != nullptr)
		{
			ok &= Expect(libtub_buffer_get_size(output) == bytes.size(), "C API: copied buffer size mismatch");
			ok &= Expect(std::memcmp(libtub_buffer_get_data(output), bytes.data(), bytes.size()) == 0, "C API: copied buffer content mismatch");
			libtub_buffer_free(output);
			output = nullptr;
		}

		ok &= Expect(libtub_copy_binary(bundle, &output, resourceID, LIBTUB_MEMORY_TYPE_DISPOSABLE, 0) == LIBTUB_ERROR_RESOURCE_NOT_FOUND, "C API: missing block should fail");
		ok &= Expect(output == nullptr, "C API: missing block returned a buffer");
		ok &= Expect(libtub_get_last_error_code(bundle) == LIBTUB_ERROR_RESOURCE_NOT_FOUND, "C API: last error mismatch");

		char errorMessage[128]{};
		ok &= Expect(libtub_get_last_error_message(bundle, errorMessage, sizeof(errorMessage)) == LIBTUB_ERROR_SUCCESS, "C API: last error message copy failed");
		ok &= Expect(std::strlen(errorMessage) > 0, "C API: last error message was empty");

		ok &= Expect(libtub_copy_binary(nullptr, &output, resourceID, LIBTUB_MEMORY_TYPE_MAIN_MEMORY, 0) == LIBTUB_ERROR_INVALID_ARGUMENT, "C API: null bundle should fail");

		libtub_buffer_free(input);
		libtub_resource_free(resource);
		libtub_free(bundle);
		return ok;
	}

	bool TestBndlPlatformRoundTrip(libtub::Platform platform)
	{
		using namespace libtub;

		Bundle bundle(Magic::Bndl, 5, platform, Flags());
		const ResourceID resourceID(std::string("bndl_platform_resource_") + std::to_string(static_cast<uint16_t>(platform)));
		Resource resource(ResourceType::Burnout::BinaryFile);

		const std::vector<uint8_t> mainBytes{ 0x10, 0x11, 0x12 };
		resource.ReplaceBinary(MemoryType::MainMemory, MakeBuffer(mainBytes, 16));

		std::vector<uint8_t> secondaryBytes;
		MemoryType secondaryType = MemoryType::Disposable;
		if (platform == Platform::PC)
		{
			secondaryType = MemoryType::Disposable;
			secondaryBytes = { 0x20, 0x21 };
		}
		else if (platform == Platform::Xbox360)
		{
			secondaryType = MemoryType::Physical;
			secondaryBytes = { 0x30, 0x31, 0x32 };
		}
		else if (platform == Platform::PS3)
		{
			secondaryType = MemoryType::GraphicsLocal;
			secondaryBytes = { 0x40, 0x41, 0x42, 0x43 };
		}

		resource.ReplaceBinary(secondaryType, MakeBuffer(secondaryBytes, 16));
		if (!Expect(bundle.AddResource(resourceID, resource), "BNDL platform round-trip: failed to add resource"))
			return false;

		const auto saved = bundle.SaveToMemory();
		if (!Expect(!saved.empty(), "BNDL platform round-trip: failed to serialize"))
			return false;

		Bundle reloaded;
		if (!Expect(reloaded.Load(std::span<const uint8_t>(saved)), "BNDL platform round-trip: failed to reload"))
		{
			std::cerr << "  load error: " << reloaded.GetLastErrorMessage() << "\n";
			return false;
		}

		bool ok = true;
		ok &= Expect(reloaded.GetMagic() == Magic::Bndl, "BNDL platform round-trip: magic mismatch");
		ok &= Expect(reloaded.GetPlatform() == platform, "BNDL platform round-trip: platform mismatch");
		ok &= ExpectBytes(reloaded.GetBinary(resourceID, MemoryType::MainMemory), mainBytes, "BNDL platform round-trip: main memory mismatch");
		ok &= ExpectBytes(reloaded.GetBinary(resourceID, secondaryType), secondaryBytes, "BNDL platform round-trip: secondary memory mismatch");
		return ok;
	}

	bool TestBndlPlatformRoundTrips()
	{
		bool ok = true;
		ok &= TestBndlPlatformRoundTrip(libtub::Platform::PC);
		ok &= TestBndlPlatformRoundTrip(libtub::Platform::Xbox360);
		ok &= TestBndlPlatformRoundTrip(libtub::Platform::PS3);
		return ok;
	}

	bool TestBndlDebugDataRoundTrip()
	{
		using namespace libtub;

		// One ID sorts before the ResourceStringTable ID (0xC039284A) and one after it, so the
		// ID list and ID table must agree on where the string table entry goes.
		const ResourceID lowID(0x00001234ULL);
		const ResourceID highID(0xF0000000ULL);
		const std::vector<uint8_t> lowBytes{ 0x01, 0x02 };
		const std::vector<uint8_t> highBytes{ 0x03, 0x04, 0x05 };

		Bundle bundle(Magic::Bndl, 5, Platform::PC, Flags());
		Resource lowResource(ResourceType::Burnout::BinaryFile);
		lowResource.ReplaceBinary(MemoryType::MainMemory, MakeBuffer(lowBytes, 4));
		Resource highResource(ResourceType::Burnout::TextFile);
		highResource.ReplaceBinary(MemoryType::MainMemory, MakeBuffer(highBytes, 4));

		bool ok = true;
		ok &= Expect(bundle.AddResource(lowID, lowResource), "BNDL debug data: failed to add low resource");
		ok &= Expect(bundle.AddResource(highID, highResource), "BNDL debug data: failed to add high resource");
		ok &= Expect(bundle.AddResourceDebugData(lowID, ResourceDebugData("low.bin", "BinaryFile")), "BNDL debug data: failed to add low debug data");
		ok &= Expect(bundle.AddResourceDebugData(highID, ResourceDebugData("high.txt", "TextFile")), "BNDL debug data: failed to add high debug data");

		const auto saved = bundle.SaveToMemory();
		if (!Expect(ok && !saved.empty(), "BNDL debug data: failed to serialize"))
			return false;

		Bundle reloaded;
		if (!Expect(reloaded.Load(std::span<const uint8_t>(saved)), "BNDL debug data: failed to reload"))
			return false;

		ok &= Expect(reloaded.GetResourceCount() == 2, "BNDL debug data: string table leaked into resource list");
		ok &= Expect(reloaded.GetResourceType(lowID) == ResourceType::Burnout::BinaryFile, "BNDL debug data: low resource type mismatch");
		ok &= Expect(reloaded.GetResourceType(highID) == ResourceType::Burnout::TextFile, "BNDL debug data: high resource type mismatch");
		ok &= ExpectBytes(reloaded.GetBinary(lowID, MemoryType::MainMemory), lowBytes, "BNDL debug data: low resource bytes");
		ok &= ExpectBytes(reloaded.GetBinary(highID, MemoryType::MainMemory), highBytes, "BNDL debug data: high resource bytes");

		const auto lowDebugData = reloaded.GetResourceDebugData(lowID);
		const auto highDebugData = reloaded.GetResourceDebugData(highID);
		ok &= Expect(lowDebugData && lowDebugData->GetName() == "low.bin", "BNDL debug data: low debug name lost");
		ok &= Expect(highDebugData && highDebugData->GetName() == "high.txt", "BNDL debug data: high debug name lost");
		return ok;
	}

	bool TestBndlImportRoundTrip()
	{
		using namespace libtub;

		const ResourceID dependencyID("bndl_dependency");
		const ResourceID resourceID("bndl_importer");

		Bundle bundle(Magic::Bndl, 5, Platform::PC, Flags());
		Resource dependency(ResourceType::Burnout::BinaryFile);
		dependency.ReplaceBinary(MemoryType::MainMemory, MakeBuffer({ 0xAA }, 1));
		Resource resource(ResourceType::Burnout::BinaryFile);
		resource.ReplaceBinary(MemoryType::MainMemory, MakeBuffer({ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, 4));
		resource.AddImport(Import(dependencyID, 0x4));

		bool ok = true;
		ok &= Expect(bundle.AddResource(dependencyID, dependency), "BNDL imports: failed to add dependency");
		ok &= Expect(bundle.AddResource(resourceID, resource), "BNDL imports: failed to add resource");

		const auto added = bundle.GetResource(resourceID);
		ok &= Expect(added && added->GetImports().size() == 1, "BNDL imports: added import not visible");

		auto saved = bundle.SaveToMemory();
		Bundle reloaded;
		if (!Expect(!saved.empty() && reloaded.Load(std::span<const uint8_t>(saved)), "BNDL imports: failed to round-trip"))
			return false;

		const auto reloadedResource = reloaded.GetResource(resourceID);
		ok &= Expect(reloadedResource && reloadedResource->GetImports().size() == 1, "BNDL imports: import lost on save");
		if (reloadedResource && reloadedResource->GetImports().size() == 1)
		{
			ok &= Expect(reloadedResource->GetImports()[0].GetResourceID() == dependencyID, "BNDL imports: import ID mismatch");
			ok &= Expect(reloadedResource->GetImports()[0].GetOffset() == 0x4, "BNDL imports: import offset mismatch");
		}

		// Replacing the resource must replace its imports, both in memory and on disk.
		Resource replacement(ResourceType::Burnout::BinaryFile);
		replacement.ReplaceBinary(MemoryType::MainMemory, MakeBuffer({ 0x01, 0x02, 0x03, 0x04 }, 4));
		ok &= Expect(reloaded.ReplaceResource(resourceID, replacement), "BNDL imports: failed to replace resource");
		const auto replaced = reloaded.GetResource(resourceID);
		ok &= Expect(replaced && replaced->GetImports().empty(), "BNDL imports: replaced resource kept old imports in memory");

		saved = reloaded.SaveToMemory();
		Bundle resaved;
		if (!Expect(!saved.empty() && resaved.Load(std::span<const uint8_t>(saved)), "BNDL imports: failed to round-trip replacement"))
			return false;

		const auto resavedResource = resaved.GetResource(resourceID);
		ok &= Expect(resavedResource && resavedResource->GetImports().empty(), "BNDL imports: stale imports written after replacement");
		return ok;
	}

	bool TestBnd2DecompRoundTrip()
	{
		using namespace libtub;

		BundleBuilder builder(BundleProfiles::BurnoutParadiseDecomp());
		const ResourceID dependencyID("decomp_dependency");
		const ResourceID resourceID("decomp_resource");
		const std::vector<uint8_t> dependencyBytes{ 0x01, 0x02, 0x03, 0x04 };
		const std::vector<uint8_t> resourceBytes{ 0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0x80 };
		const std::vector<uint8_t> disposableBytes{ 0xFE, 0xED };

		bool ok = true;
		ok &= Expect(builder.AddResource(dependencyID, ResourceType::Burnout::BinaryFile)
			.MainMemory(std::span<const uint8_t>(dependencyBytes), 4)
			.DebugData("decomp_dependency.bin", "BinaryFile")
			.Commit(), "decomp round-trip: failed to add dependency");
		ok &= Expect(builder.AddResource(resourceID, ResourceType::Burnout::Renderable)
			.MainMemory(std::span<const uint8_t>(resourceBytes), 16)
			.Disposable(std::span<const uint8_t>(disposableBytes), 16)
			.Import(dependencyID, 0x4)
			.DebugData("decomp_resource.bin", "Renderable")
			.Commit(), "decomp round-trip: failed to add resource");

		const auto saved = builder.SaveToMemory();
		if (!Expect(ok && saved.size() > 12, "decomp round-trip: failed to serialize"))
			return false;

		uint32_t onDiskPlatform = 0;
		std::memcpy(&onDiskPlatform, saved.data() + 8, sizeof(onDiskPlatform));
		ok &= Expect(onDiskPlatform == 4, "decomp round-trip: platform was not stored as 4");

		Bundle reloaded;
		if (!Expect(reloaded.Load(std::span<const uint8_t>(saved)), "decomp round-trip: failed to reload"))
			return false;

		ok &= Expect(reloaded.GetPlatform() == Platform::PCx64, "decomp round-trip: platform mismatch");
		ok &= Expect(static_cast<bool>(reloaded.GetFlags() & Flags::Compressed), "decomp round-trip: compression flag lost");
		ok &= ExpectBytes(reloaded.GetBinary(resourceID, MemoryType::MainMemory), resourceBytes, "decomp round-trip: main memory");
		ok &= ExpectBytes(reloaded.GetBinary(resourceID, MemoryType::Disposable), disposableBytes, "decomp round-trip: disposable memory");

		const auto resource = reloaded.GetResource(resourceID);
		ok &= Expect(resource && resource->GetImports().size() == 1 && resource->GetImports()[0].GetResourceID() == dependencyID, "decomp round-trip: import mismatch");

		const auto debugData = reloaded.GetResourceDebugData(resourceID);
		ok &= Expect(debugData && debugData->GetName() == "decomp_resource.bin", "decomp round-trip: debug data mismatch");

		ok &= Expect(reloaded.SaveToMemory() == saved, "decomp round-trip: resave was not byte-identical");
		return ok;
	}

	bool TestCompressedBnd2RoundTrip(uint16_t version)
	{
		using namespace libtub;

		// Only main memory is populated, so the other blocks rely on a default on-disk alignment.
		Bundle bundle(Magic::Bnd2, version, Platform::PC, Flags::Compressed | Flags::HasDebugData);
		const ResourceID resourceID("compressed_resource");
		const std::vector<uint8_t> bytes{ 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77 };
		Resource resource(ResourceType::NeedForSpeed::BinaryFile);
		resource.ReplaceBinary(MemoryType::MainMemory, MakeBuffer(bytes, 8));

		const auto label = "compressed BND2 v" + std::to_string(version);
		if (!Expect(bundle.AddResource(resourceID, resource), label + ": failed to add resource"))
			return false;

		const auto saved = bundle.SaveToMemory();
		Bundle reloaded;
		if (!Expect(!saved.empty() && reloaded.Load(std::span<const uint8_t>(saved)), label + ": failed to round-trip"))
			return false;

		return ExpectBytes(reloaded.GetBinary(resourceID, MemoryType::MainMemory), bytes, label + ": main memory");
	}

	uint32_t ReadU32(const std::vector<uint8_t> &bytes, size_t offset)
	{
		uint32_t value = 0;
		std::memcpy(&value, bytes.data() + offset, sizeof(value));
		return value;
	}

	bool TestCorruptBlockIsReported()
	{
		using namespace libtub;

		BundleBuilder builder(BundleProfiles::BurnoutParadisePC());
		const ResourceID resourceID("corrupt_resource");
		const std::vector<uint8_t> mainBytes{ 0x10, 0x20, 0x30, 0x40 };
		const std::vector<uint8_t> disposableBytes{ 0x50, 0x60, 0x70, 0x80, 0x90 };
		if (!Expect(builder.AddResource(resourceID, ResourceType::Burnout::BinaryFile)
			.MainMemory(std::span<const uint8_t>(mainBytes), 4)
			.Disposable(std::span<const uint8_t>(disposableBytes), 4)
			.Commit(), "corrupt block: failed to add resource"))
			return false;

		auto bytes = builder.SaveToMemory();
		if (!Expect(bytes.size() > 64, "corrupt block: failed to serialize"))
			return false;

		// BND2 v2 PC: file block 1 holds disposable memory. Flip the last byte of its zlib stream (the Adler-32 checksum).
		const auto idBlockOffset = ReadU32(bytes, 20);
		const auto disposableBlockOffset = ReadU32(bytes, 28);
		const auto disposableOnDiskSize = ReadU32(bytes, idBlockOffset + 28 + 4) & 0x0FFFFFFF;
		bytes[disposableBlockOffset + ReadU32(bytes, idBlockOffset + 40 + 4) + disposableOnDiskSize - 1] ^= 0xFF;

		Bundle bundle;
		if (!Expect(bundle.Load(std::span<const uint8_t>(bytes)), "corrupt block: load should defer decompression"))
			return false;

		bool ok = true;
		ok &= ExpectBytes(bundle.GetBinary(resourceID, MemoryType::MainMemory), mainBytes, "corrupt block: intact main memory should still be readable");
		ok &= Expect(bundle.GetBinary(resourceID, MemoryType::Disposable) == nullptr, "corrupt block: corrupt disposable memory was returned");
		ok &= Expect(bundle.GetLastErrorCode() == ErrorCode::DecompressionFailed, "corrupt block: GetBinary error code mismatch");
		ok &= Expect(!bundle.GetResource(resourceID), "corrupt block: resource with a corrupt block was returned without it");
		ok &= Expect(bundle.GetLastErrorCode() == ErrorCode::DecompressionFailed, "corrupt block: GetResource error code mismatch");
		ok &= Expect(!bundle.GetResource(ResourceID("missing_resource")) && bundle.GetLastErrorCode() == ErrorCode::ResourceNotFound, "corrupt block: missing resource error code mismatch");
		ok &= Expect(bundle.DescribeResources().empty() && bundle.GetLastErrorCode() == ErrorCode::DecompressionFailed, "corrupt block: DescribeResources hid the unreadable resource");
		ok &= Expect(bundle.SaveToMemory() == bytes, "corrupt block: raw resave was not byte-identical");

#ifndef LIBTUB_SKIP_PROJECT_TESTS
		const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
		const auto directory = std::filesystem::temp_directory_path() / ("libtub-corrupt-export-" + std::to_string(stamp));
		ok &= Expect(!bundle.ExportProject(directory), "corrupt block: ExportProject silently dropped the unreadable resource");
		std::error_code error;
		std::filesystem::remove_all(directory, error);
#endif
		return ok;
	}

	bool TestFormatFailureCodes()
	{
		using namespace libtub;

		bool ok = true;
		Bundle bundle;
		const std::vector<uint8_t> unsupportedVersion{ 'b', 'n', 'd', '2', 0x04, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00 };
		ok &= Expect(!bundle.Load(std::span<const uint8_t>(unsupportedVersion)) && bundle.GetLastErrorCode() == ErrorCode::UnsupportedVersion, "failure codes: BND2 v4 was not reported as an unsupported version");

		// BND2 v2 can't store the multistream flag.
		Bundle multistream(Magic::Bnd2, 2, Platform::PC, Flags::MultistreamBundle);
		ok &= Expect(multistream.SaveToMemory().empty() && multistream.GetLastErrorCode() == ErrorCode::UnsupportedFlags, "failure codes: unsupported v2 flags were not reported");

		Bundle wrongPlatform(Magic::Bnd2, 2, Platform::WiiU, Flags());
		ok &= Expect(wrongPlatform.SaveToMemory().empty() && wrongPlatform.GetLastErrorCode() == ErrorCode::UnsupportedPlatform, "failure codes: unsupported v2 platform was not reported");
		return ok;
	}

	bool TestCApiErrorCodes()
	{
		bool ok = true;

		libtub_bundle *bundle = nullptr;
		ok &= Expect(libtub_load(&bundle, "this/path/does/not/exist.bundle") == LIBTUB_ERROR_INVALID_PATH && bundle == nullptr, "C API codes: missing file was not reported as an invalid path");
		ok &= Expect(libtub_create(&bundle, 0x7F, 5, LIBTUB_PLATFORM_PC, 0) == LIBTUB_ERROR_INVALID_ARGUMENT && bundle == nullptr, "C API codes: invalid magic was not rejected as an argument error");

		if (!Expect(libtub_create(&bundle, LIBTUB_MAGIC_BND2, 2, LIBTUB_PLATFORM_PC, LIBTUB_FLAGS_MULTISTREAM_BUNDLE) == LIBTUB_ERROR_SUCCESS, "C API codes: create failed"))
			return false;

		const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
		const auto path = (std::filesystem::temp_directory_path() / ("libtub-c-api-" + std::to_string(stamp) + ".bundle")).string();
		ok &= Expect(libtub_save(bundle, path.c_str()) == LIBTUB_ERROR_UNSUPPORTED_FLAGS, "C API codes: unsupported flags were not reported on save");
		std::error_code error;
		std::filesystem::remove(path, error);
		libtub_free(bundle);
		bundle = nullptr;

		libtub_resource_debug_data *debugData = nullptr;
		if (!Expect(libtub_resource_debug_data_create(&debugData, "resource_name", "TypeName") == LIBTUB_ERROR_SUCCESS, "C API codes: debug data create failed"))
			return false;

		char small[4]{};
		char full[32]{};
		ok &= Expect(libtub_resource_debug_data_get_name(debugData, small, sizeof(small)) == LIBTUB_ERROR_INSUFFICIENT_BUFFER && std::strcmp(small, "res") == 0, "C API codes: truncation was not reported");
		ok &= Expect(libtub_resource_debug_data_get_name(debugData, full, sizeof(full)) == LIBTUB_ERROR_SUCCESS && std::strcmp(full, "resource_name") == 0, "C API codes: full name copy failed");
		libtub_resource_debug_data_free(debugData);
		return ok;
	}

	bool TestStreamIndexRange()
	{
		using namespace libtub;

		Bundle bundle(Magic::Bnd2, 5, Platform::PC, Flags());
		Resource resource(ResourceType::NeedForSpeed::BinaryFile);
		resource.ReplaceBinary(MemoryType::MainMemory, MakeBuffer({ 0x01 }, 1));

		bool ok = true;
		ok &= Expect(!bundle.AddResource(ResourceID("out_of_range_stream"), resource, 4), "stream range: resource added to stream 4");
		ok &= Expect(bundle.GetResourceCount() == 0, "stream range: rejected resource left an entry");
		ok &= Expect(!static_cast<bool>(bundle.GetFlags() & Flags::MultistreamBundle), "stream range: rejected resource set the multistream flag");

		libtub_bundle *cBundle = nullptr;
		libtub_resource *cResource = nullptr;
		if (!Expect(libtub_create(&cBundle, LIBTUB_MAGIC_BND2, 5, LIBTUB_PLATFORM_PC, 0) == LIBTUB_ERROR_SUCCESS && libtub_resource_create(&cResource, LIBTUB_RESOURCE_TYPE_NFS_BINARY_FILE) == LIBTUB_ERROR_SUCCESS, "stream range: C API setup failed"))
		{
			libtub_resource_free(cResource);
			libtub_free(cBundle);
			return false;
		}

		const auto cResourceID = libtub_resource_id_from_name("c_out_of_range_stream");
		ok &= Expect(libtub_add_resource(cBundle, cResourceID, cResource, LIBTUB_STREAM_MAX_COUNT) == LIBTUB_ERROR_OUT_OF_RANGE, "stream range: C API add accepted stream 4");
		ok &= Expect(libtub_replace_resource(cBundle, cResourceID, cResource, LIBTUB_STREAM_MAX_COUNT) == LIBTUB_ERROR_OUT_OF_RANGE, "stream range: C API replace accepted stream 4");

		libtub_resource_free(cResource);
		libtub_free(cBundle);
		return ok;
	}

	bool TestFailedLoadKeepsBundle()
	{
		using namespace libtub;

		auto bundle = MakeReferenceBundle();
		const auto resourceCount = bundle.GetResourceCount();

		// Valid magic and version but a truncated header, so the parser itself rejects it.
		const std::vector<uint8_t> truncated{ 'b', 'n', 'd', '2', 0x05, 0x00, 0x01, 0x00 };
		bool ok = true;
		ok &= Expect(!bundle.Load(std::span<const uint8_t>(truncated)), "failed load: truncated bundle was accepted");
		ok &= Expect(bundle.GetLastErrorCode() != ErrorCode::Success, "failed load: no error code reported");
		ok &= Expect(bundle.IsValid() && bundle.GetResourceCount() == resourceCount, "failed load: previous bundle contents were discarded");

		const std::vector<uint8_t> unknownMagic{ 'n', 'o', 'p', 'e', 0x00, 0x00, 0x00, 0x00 };
		ok &= Expect(!bundle.Load(std::span<const uint8_t>(unknownMagic)), "failed load: unknown magic was accepted");
		ok &= Expect(bundle.GetLastErrorCode() == ErrorCode::UnsupportedFormat, "failed load: unknown magic error code mismatch");
		ok &= Expect(bundle.IsValid() && bundle.GetResourceCount() == resourceCount, "failed load: unknown magic discarded previous contents");

		Bundle empty;
		ok &= Expect(!empty.Load(std::span<const uint8_t>(truncated)), "failed load: truncated bundle was accepted by empty bundle");
		ok &= Expect(!empty.IsValid(), "failed load: empty bundle became valid after a failed load");
		return ok;
	}

	bool TestBundleBuilderMoveAndValidation()
	{
		using namespace libtub;

		const std::vector<uint8_t> bytes{ 0x01, 0x02, 0x03, 0x04 };
		bool ok = true;

		BundleBuilder builder(BundleProfiles::NeedForSpeedHotPursuitPC());
		auto original = builder.AddResource(ResourceID("moved_resource"), ResourceType::NeedForSpeed::BinaryFile);
		original.MainMemory(std::span<const uint8_t>(bytes), 4);
		auto moved = std::move(original);
		ok &= Expect(!original.Commit(), "builder move: moved-from resource builder committed");
		ok &= Expect(builder.GetBundle().GetResourceCount() == 0, "builder move: moved-from commit added a resource");
		ok &= Expect(moved.Commit(), "builder move: moved-to resource builder failed to commit");

		auto badAlignment = builder.AddResource(ResourceID("bad_alignment"), ResourceType::NeedForSpeed::BinaryFile);
		ok &= Expect(!badAlignment.MainMemory(std::span<const uint8_t>(bytes), 3).Commit(), "builder validation: non power-of-two alignment was accepted");

		BundleBuilder bndlBuilder(BundleProfiles::BndlPC());
		auto streamed = bndlBuilder.AddResource(ResourceID("bndl_streamed"), ResourceType::Burnout::BinaryFile, 1);
		ok &= Expect(!streamed.MainMemory(std::span<const uint8_t>(bytes), 4).Commit(), "builder validation: BNDL accepted a non-zero stream index");
		return ok;
	}

#ifndef LIBTUB_SKIP_PROJECT_TESTS
	bool TestProjectResourceWithoutBlocks()
	{
		using namespace libtub;

		const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
		const auto directory = std::filesystem::temp_directory_path() / ("libtub-project-empty-test-" + std::to_string(stamp));

		// A resource with no populated memory blocks must survive export and import.
		Bundle bundle(Magic::Bnd2, 5, Platform::PC, Flags::HasDebugData);
		const ResourceID resourceID("empty_resource");
		bool ok = Expect(bundle.AddResource(resourceID, Resource(ResourceType::NeedForSpeed::BinaryFile)), "project empty resource: failed to add resource");
		ok &= Expect(bundle.ExportProject(directory), "project empty resource: export failed");

		Bundle imported;
		ok &= Expect(imported.ImportProject(directory) && imported.GetResourceCount() == 1, "project empty resource: import failed");

		// Projects exported before the fix wrote such resources as "binaries: ~".
		{
			std::ofstream meta(directory / ".meta.yaml", std::ios::binary);
			meta << "bundle:\n  magic: bnd2\n  version: 5\n  platform: pc\nresources:\n  - id: 0x12345678\n    streamIndex: 0\n    type: 0x00000000\n    binaries: ~\n";
		}
		Bundle legacy;
		ok &= Expect(legacy.ImportProject(directory) && legacy.GetResourceCount() == 1, "project empty resource: legacy null binaries were rejected");

		std::error_code error;
		std::filesystem::remove_all(directory, error);
		return ok;
	}

	bool TestProjectRejectsOutOfRangeValues()
	{
		const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
		const auto directory = std::filesystem::temp_directory_path() / ("libtub-project-range-test-" + std::to_string(stamp));
		std::filesystem::create_directories(directory);

		// Version 65541 truncates to 5 if the parser does not range-check.
		{
			std::ofstream meta(directory / ".meta.yaml", std::ios::binary);
			meta << "bundle:\n  magic: bnd2\n  version: 65541\n  platform: pc\nresources: []\n";
		}

		libtub::Bundle imported;
		const bool rejected = Expect(!imported.ImportProject(directory), "project range: out-of-range version was accepted");

		std::error_code error;
		std::filesystem::remove_all(directory, error);
		return rejected;
	}
#endif
}

int main()
{
	bool ok = true;
	ok &= TestBnd2ImportRoundTrips();
#ifndef LIBTUB_SKIP_PROJECT_TESTS
	ok &= TestProjectRoundTrip();
	ok &= TestProjectRejectsOutOfRangeValues();
	ok &= TestProjectResourceWithoutBlocks();
#endif
	ok &= TestBundleBuilder();
	ok &= TestBundleBuilderValidation();
	ok &= TestBundleBuilderMoveAndValidation();
	ok &= TestCApiBinaryErrors();
	ok &= TestBndlPlatformRoundTrips();
	ok &= TestBndlDebugDataRoundTrip();
	ok &= TestBndlImportRoundTrip();
	ok &= TestBnd2DecompRoundTrip();
	ok &= TestCompressedBnd2RoundTrip(3);
	ok &= TestCompressedBnd2RoundTrip(5);
	ok &= TestFailedLoadKeepsBundle();
	ok &= TestStreamIndexRange();
	ok &= TestCorruptBlockIsReported();
	ok &= TestFormatFailureCodes();
	ok &= TestCApiErrorCodes();
	return ok ? 0 : 1;
}
