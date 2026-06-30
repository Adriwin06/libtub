#include <libtub/builder.hpp>
#include <algorithm>
#include <array>
#include <cstring>

using namespace libtub;

namespace
{
	Buffer MakeBuffer(std::span<const uint8_t> data, uint32_t alignment)
	{
		auto buffer = std::make_unique_for_overwrite<uint8_t[]>(data.size());
		if (!data.empty())
			std::memcpy(buffer.get(), data.data(), data.size());

		return { std::move(buffer), data.size(), alignment };
	}

	constexpr Flags kBurnoutParadiseFlags = Flags::Compressed | Flags::MainMemOptimised | Flags::GraphicsMemOptimised | Flags::HasDebugData;

	constexpr std::array<MemoryType, 4> kMemoryTypeSlots = {
		MemoryType::MainMemory,
		MemoryType::GraphicsSystem,
		MemoryType::GraphicsLocal,
		MemoryType::Disposable,
	};
}

BundleProfile BundleProfiles::BurnoutParadisePC()
{
	return { Magic::Bnd2, 2, Platform::PC, kBurnoutParadiseFlags };
}

BundleProfile BundleProfiles::BurnoutParadiseXbox360()
{
	return { Magic::Bnd2, 2, Platform::Xbox360, kBurnoutParadiseFlags };
}

BundleProfile BundleProfiles::BurnoutParadisePS3()
{
	return { Magic::Bnd2, 2, Platform::PS3, kBurnoutParadiseFlags };
}

BundleProfile BundleProfiles::BurnoutParadiseDecomp()
{
	return { Magic::Bnd2, 2, Platform::PCx64, kBurnoutParadiseFlags };
}

BundleProfile BundleProfiles::NeedForSpeedHotPursuitPC()
{
	return { Magic::Bnd2, 5, Platform::PC, Flags::HasDebugData };
}

BundleProfile BundleProfiles::BndlPC(uint16_t version, Flags flags)
{
	return { Magic::Bndl, version, Platform::PC, flags };
}

BundleResourceBuilder::BundleResourceBuilder(Bundle &bundle, ResourceID resourceID, uint32_t resourceType, uint8_t streamIndex)
	: m_bundle(&bundle), m_resourceID(resourceID), m_streamIndex(streamIndex), m_resource(resourceType)
{
}

BundleResourceBuilder::BundleResourceBuilder(BundleResourceBuilder &&other) noexcept = default;

BundleResourceBuilder &BundleResourceBuilder::operator=(BundleResourceBuilder &&other) noexcept = default;

BundleResourceBuilder &BundleResourceBuilder::Binary(MemoryType memoryType, std::span<const uint8_t> data, uint32_t alignment)
{
	m_resource.ReplaceBinary(memoryType, MakeBuffer(data, alignment));
	return *this;
}

BundleResourceBuilder &BundleResourceBuilder::MainMemory(std::span<const uint8_t> data, uint32_t alignment)
{
	return Binary(MemoryType::MainMemory, data, alignment);
}

BundleResourceBuilder &BundleResourceBuilder::GraphicsSystem(std::span<const uint8_t> data, uint32_t alignment)
{
	return Binary(MemoryType::GraphicsSystem, data, alignment);
}

BundleResourceBuilder &BundleResourceBuilder::Physical(std::span<const uint8_t> data, uint32_t alignment)
{
	return Binary(MemoryType::Physical, data, alignment);
}

BundleResourceBuilder &BundleResourceBuilder::Mem1(std::span<const uint8_t> data, uint32_t alignment)
{
	return Binary(MemoryType::Mem1, data, alignment);
}

BundleResourceBuilder &BundleResourceBuilder::GraphicsLocal(std::span<const uint8_t> data, uint32_t alignment)
{
	return Binary(MemoryType::GraphicsLocal, data, alignment);
}

BundleResourceBuilder &BundleResourceBuilder::GraphicsMem2(std::span<const uint8_t> data, uint32_t alignment)
{
	return Binary(MemoryType::GraphicsMem2, data, alignment);
}

BundleResourceBuilder &BundleResourceBuilder::Disposable(std::span<const uint8_t> data, uint32_t alignment)
{
	return Binary(MemoryType::Disposable, data, alignment);
}

BundleResourceBuilder &BundleResourceBuilder::Import(ResourceID resourceID, uint32_t offset, libtub::Import::ImportType type)
{
	m_resource.AddImport(libtub::Import(resourceID, offset, type));
	return *this;
}

BundleResourceBuilder &BundleResourceBuilder::DebugData(std::string name, std::string typeName)
{
	m_debugData = ResourceDebugData(std::move(name), std::move(typeName));
	return *this;
}

bool BundleResourceBuilder::Commit()
{
	if (!Validate())
		return false;

	if (!m_bundle->AddResource(m_resourceID, m_resource, m_streamIndex))
		return Fail(m_bundle->GetLastErrorMessage().empty() ? "Failed to add resource to bundle." : m_bundle->GetLastErrorMessage());

	if (m_debugData && !m_bundle->AddResourceDebugData(m_resourceID, *m_debugData, m_streamIndex))
		return Fail(m_bundle->GetLastErrorMessage().empty() ? "Failed to add resource debug data to bundle." : m_bundle->GetLastErrorMessage());

	m_lastErrorMessage.clear();
	return true;
}

const std::string &BundleResourceBuilder::GetLastErrorMessage() const noexcept
{
	return m_lastErrorMessage;
}

bool BundleResourceBuilder::Fail(std::string message)
{
	m_lastErrorMessage = std::move(message);
	return false;
}

bool BundleResourceBuilder::Validate()
{
	if (m_bundle == nullptr)
		return Fail("Builder is no longer attached to a bundle.");

	if (!m_bundle->IsValid())
		return Fail("Builder is attached to an invalid bundle.");

	if (m_streamIndex >= 4)
		return Fail("Stream index must be in the range 0..3.");

	const auto memoryTypes = m_bundle->GetMemoryTypes();
	for (const auto memoryType : kMemoryTypeSlots)
	{
		const auto &buffer = m_resource.GetBinary(memoryType);
		if (buffer == nullptr)
			continue;

		if (buffer.GetAlignment() == 0)
			return Fail("Memory block alignment must be greater than zero.");

		if (std::find(memoryTypes.begin(), memoryTypes.end(), memoryType) == memoryTypes.end())
			return Fail("Memory block is not valid for the selected bundle platform.");
	}

	return true;
}

BundleBuilder::BundleBuilder(BundleProfile profile)
	: BundleBuilder(profile.magic, profile.version, profile.platform, profile.flags)
{
}

BundleBuilder::BundleBuilder(Magic magic, uint16_t version, Platform platform, Flags flags)
	: m_bundle(magic, version, platform, flags)
{
}

BundleBuilder::BundleBuilder(BundleBuilder &&other) noexcept = default;

BundleBuilder &BundleBuilder::operator=(BundleBuilder &&other) noexcept = default;

Bundle &BundleBuilder::GetBundle() noexcept
{
	return m_bundle;
}

const Bundle &BundleBuilder::GetBundle() const noexcept
{
	return m_bundle;
}

BundleResourceBuilder BundleBuilder::AddResource(ResourceID resourceID, uint32_t resourceType, uint8_t streamIndex)
{
	return BundleResourceBuilder(m_bundle, resourceID, resourceType, streamIndex);
}

bool BundleBuilder::SetDefaultResource(ResourceID resourceID, int32_t streamIndex)
{
	return m_bundle.SetDefaultResource(resourceID, streamIndex);
}

bool BundleBuilder::SetStreamName(uint8_t index, std::string_view name)
{
	return m_bundle.SetStreamName(index, name);
}

bool BundleBuilder::Save(const std::filesystem::path &path)
{
	return m_bundle.Save(path);
}

std::vector<uint8_t> BundleBuilder::SaveToMemory()
{
	return m_bundle.SaveToMemory();
}

Bundle BundleBuilder::Build() &&
{
	return std::move(m_bundle);
}
