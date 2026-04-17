#include <libtub/bundle.hpp>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <filesystem>
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

	libtub::Bundle MakeReferenceBundle()
	{
		using namespace libtub;

		auto bundle = Bundle(Magic::Bnd2, 5, Platform::PC, Flags::HasDebugData);
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
			ok &= Expect(imports[0].GetOffset() == 0x00000004, label + ": pointer import offset mismatch");
			ok &= Expect(imports[0].GetImportType() == Import::ImportType::Pointer, label + ": pointer import kind mismatch");
			ok &= Expect(imports[1].GetOffset() == 0x00000008, label + ": resource handle import offset mismatch");
			ok &= Expect(imports[1].GetImportType() == Import::ImportType::ResourceHandle, label + ": resource handle import kind mismatch");
		}

		return ok;
	}

	bool TestBnd2ImportRoundTrip()
	{
		auto bundle = MakeReferenceBundle();
		const auto bytes = bundle.SaveToMemory();
		if (!Expect(!bytes.empty(), "memory round-trip: failed to serialize bundle"))
			return false;

		libtub::Bundle reloaded;
		if (!Expect(reloaded.Load(std::span<const uint8_t>(bytes)), "memory round-trip: failed to load serialized bundle"))
			return false;

		return VerifyBundleState(reloaded, "memory round-trip");
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
}

int main()
{
	bool ok = true;
	ok &= TestBnd2ImportRoundTrip();
	ok &= TestProjectRoundTrip();
	return ok ? 0 : 1;
}
