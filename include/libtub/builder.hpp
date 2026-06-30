#pragma once
#include <libtub/bundle.hpp>
#include <span>
#include <utility>

namespace libtub
{
	struct BundleProfile
	{
		Magic magic;
		uint16_t version;
		Platform platform;
		Flags flags;
	};

	namespace BundleProfiles
	{
		[[nodiscard]] LIBTUB_EXPORT BundleProfile BurnoutParadisePC();
		[[nodiscard]] LIBTUB_EXPORT BundleProfile BurnoutParadiseXbox360();
		[[nodiscard]] LIBTUB_EXPORT BundleProfile BurnoutParadisePS3();
		[[nodiscard]] LIBTUB_EXPORT BundleProfile NeedForSpeedHotPursuitPC();
		[[nodiscard]] LIBTUB_EXPORT BundleProfile BndlPC(uint16_t version = 5, Flags flags = Flags::HasDebugData);
	}

	class BundleResourceBuilder
	{
	public:
		LIBTUB_EXPORT BundleResourceBuilder(Bundle &bundle, ResourceID resourceID, uint32_t resourceType, uint8_t streamIndex = 0);
		BundleResourceBuilder(const BundleResourceBuilder &) = delete;
		BundleResourceBuilder &operator=(const BundleResourceBuilder &) = delete;
		LIBTUB_EXPORT BundleResourceBuilder(BundleResourceBuilder &&other) noexcept;
		LIBTUB_EXPORT BundleResourceBuilder &operator=(BundleResourceBuilder &&other) noexcept;

		LIBTUB_EXPORT BundleResourceBuilder &Binary(MemoryType memoryType, std::span<const uint8_t> data, uint32_t alignment = 1);
		LIBTUB_EXPORT BundleResourceBuilder &MainMemory(std::span<const uint8_t> data, uint32_t alignment = 1);
		LIBTUB_EXPORT BundleResourceBuilder &GraphicsSystem(std::span<const uint8_t> data, uint32_t alignment = 1);
		LIBTUB_EXPORT BundleResourceBuilder &Physical(std::span<const uint8_t> data, uint32_t alignment = 1);
		LIBTUB_EXPORT BundleResourceBuilder &Mem1(std::span<const uint8_t> data, uint32_t alignment = 1);
		LIBTUB_EXPORT BundleResourceBuilder &GraphicsLocal(std::span<const uint8_t> data, uint32_t alignment = 1);
		LIBTUB_EXPORT BundleResourceBuilder &GraphicsMem2(std::span<const uint8_t> data, uint32_t alignment = 1);
		LIBTUB_EXPORT BundleResourceBuilder &Disposable(std::span<const uint8_t> data, uint32_t alignment = 1);
		LIBTUB_EXPORT BundleResourceBuilder &Import(ResourceID resourceID, uint32_t offset, Import::ImportType type = Import::ImportType::Pointer);
		LIBTUB_EXPORT BundleResourceBuilder &DebugData(std::string name, std::string typeName);
		LIBTUB_EXPORT bool Commit();
		[[nodiscard]] LIBTUB_EXPORT const std::string &GetLastErrorMessage() const noexcept;

	private:
		bool Fail(std::string message);
		[[nodiscard]] bool Validate();

		Bundle *m_bundle;
		ResourceID m_resourceID;
		uint8_t m_streamIndex;
		Resource m_resource;
		std::optional<ResourceDebugData> m_debugData;
		std::string m_lastErrorMessage;
	};

	class BundleBuilder
	{
	public:
		LIBTUB_EXPORT explicit BundleBuilder(BundleProfile profile);
		LIBTUB_EXPORT BundleBuilder(Magic magic, uint16_t version, Platform platform, Flags flags);
		BundleBuilder(const BundleBuilder &) = delete;
		BundleBuilder &operator=(const BundleBuilder &) = delete;
		LIBTUB_EXPORT BundleBuilder(BundleBuilder &&other) noexcept;
		LIBTUB_EXPORT BundleBuilder &operator=(BundleBuilder &&other) noexcept;

		[[nodiscard]] LIBTUB_EXPORT Bundle &GetBundle() noexcept;
		[[nodiscard]] LIBTUB_EXPORT const Bundle &GetBundle() const noexcept;
		LIBTUB_EXPORT BundleResourceBuilder AddResource(ResourceID resourceID, uint32_t resourceType, uint8_t streamIndex = 0);
		LIBTUB_EXPORT bool SetDefaultResource(ResourceID resourceID, int32_t streamIndex = 0);
		LIBTUB_EXPORT bool SetStreamName(uint8_t index, std::string_view name);
		LIBTUB_EXPORT bool Save(const std::filesystem::path &path);
		[[nodiscard]] LIBTUB_EXPORT std::vector<uint8_t> SaveToMemory();
		[[nodiscard]] LIBTUB_EXPORT Bundle Build() &&;

	private:
		Bundle m_bundle;
	};
}
