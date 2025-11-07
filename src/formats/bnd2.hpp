#pragma once
#include "base.hpp"

namespace libbndl
{
	namespace Formats
	{
		class BND2 : public Base
		{
		public:
			using Base::Base;

			virtual bool Load(binaryio::BinaryReader &reader) override;
			virtual bool Save(binaryio::BinaryWriter &reader) override;

			[[nodiscard]] virtual constexpr MagicNumber GetMagicNumber() const override { return MagicNumber::BND2; }

			[[nodiscard]] virtual std::optional<Resource> GetResource(ResourceKey resourceKey) const override;

			[[nodiscard]] virtual ResourceID GetDefaultResourceID() const override;
			[[nodiscard]] virtual int32_t GetDefaultResourceStreamIndex() const override;
			[[nodiscard]] virtual std::string GetStreamName(uint8_t index) const override;

			[[nodiscard]] virtual std::vector<MemoryType> GetMemoryTypes() const override;

		protected:
			virtual constexpr bool AppendsImportsToResource() const override { return false; }
			virtual bool IsValidPlatform() const override;

			virtual std::vector<ResourceKey> SortedDebugDataKeys() const override;
			virtual std::vector<std::pair<std::string, std::string>> GetDebugDataAttributes(const ResourceKey &resourceKey, const ResourceDebugInfoEntry &debugInfo) const override;

		private:
			ResourceID m_defaultResourceID;
			int32_t m_defaultResourceStreamIndex;
			std::array<std::string, kStreamLimit> m_streamNames;

			std::optional<uint8_t> MapFileBlockToLibBlock(uint8_t block) const;
		};
	}
}
