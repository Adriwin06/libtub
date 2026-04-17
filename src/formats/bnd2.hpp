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

			virtual bool Load(binaryio::BinaryReader &reader) override;
			virtual bool Save(binaryio::BinaryWriter &reader) override;

			[[nodiscard]] virtual constexpr Magic GetMagic() const override { return Magic::Bnd2; }

			[[nodiscard]] virtual std::optional<Resource> GetResource(ResourceKey resourceKey) const override;

			[[nodiscard]] virtual ResourceID GetDefaultResourceID() const override;
			[[nodiscard]] virtual int32_t GetDefaultResourceStreamIndex() const override;
			[[nodiscard]] virtual std::string GetStreamName(uint8_t index) const override;
			virtual bool SetDefaultResource(ResourceKey resourceKey) override;
			virtual bool SetStreamName(uint8_t index, const std::string &name) override;

			[[nodiscard]] virtual std::vector<MemoryType> GetMemoryTypes() const override;

		protected:
			virtual constexpr bool AppendsImportsToResource() const override { return true; }
			virtual bool IsValidPlatform() const override;

			virtual std::vector<ResourceKey> SortedDebugDataKeys() const override;
			virtual std::vector<std::pair<std::string, std::string>> GetDebugDataAttributes(const ResourceKey &resourceKey, const ResourceDebugDataEntry &debugData) const override;

		private:
			ResourceID m_defaultResourceID{};
			int32_t m_defaultResourceStreamIndex = -1;
			std::array<std::string, kStreamLimit> m_streamNames{};

			std::optional<uint8_t> MapFileBlockToLibBlock(uint8_t block) const;
		};
	}
}
