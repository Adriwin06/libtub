#pragma once
#include "base.hpp"

namespace libtub
{
	namespace Formats
	{
		class Bnd2 : public Base
		{
		public:
			using Base::Base;

			ErrorCode Load(binaryio::BinaryReader &reader) override;
			ErrorCode Save(binaryio::BinaryWriter &writer) override;

			[[nodiscard]] constexpr Magic GetMagic() const override { return Magic::Bnd2; }

			[[nodiscard]] std::optional<Resource> GetResource(ResourceKey resourceKey) const override;
			[[nodiscard]] std::optional<Buffer> GetResourceBinary(ResourceKey resourceKey, MemoryType memoryType) const override;

			[[nodiscard]] ResourceID GetDefaultResourceID() const override;
			[[nodiscard]] int32_t GetDefaultResourceStreamIndex() const override;
			[[nodiscard]] std::string GetStreamName(uint8_t index) const override;
			bool SetDefaultResource(ResourceKey resourceKey) override;
			bool SetStreamName(uint8_t index, const std::string &name) override;

			[[nodiscard]] std::vector<MemoryType> GetMemoryTypes() const override;

		protected:
			[[nodiscard]] constexpr bool AppendsImportsToResource() const override { return true; }
			[[nodiscard]] bool IsValidPlatform() const override;

			[[nodiscard]] std::vector<ResourceKey> SortedDebugDataKeys() const override;
			[[nodiscard]] std::vector<std::pair<std::string, std::string>> GetDebugDataAttributes(const ResourceKey &resourceKey, const ResourceDebugDataEntry &debugData) const override;

		private:
			ResourceID m_defaultResourceID{};
			int32_t m_defaultResourceStreamIndex = -1;
			std::array<std::string, kStreamLimit> m_streamNames{};

			[[nodiscard]] std::optional<uint8_t> MapFileBlockToLibBlock(uint8_t block) const;
		};
	}
}
