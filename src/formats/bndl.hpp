#pragma once
#include "base.hpp"

namespace libtub
{
	namespace Formats
	{
		class Bndl : public Base
		{
		public:
			using Base::Base;

			ErrorCode Load(binaryio::BinaryReader &reader) override;
			ErrorCode Save(binaryio::BinaryWriter &writer) override;

			[[nodiscard]] constexpr Magic GetMagic() const override { return Magic::Bndl; }

			[[nodiscard]] std::optional<Resource> GetResource(ResourceKey resourceKey) const override;

		private:
			std::map<ResourceID, std::vector<ImportEntry>> m_imports;

			[[nodiscard]] constexpr bool AppendsImportsToResource() const override { return false; }
			void StoreSeparateImports(ResourceKey resourceKey, const std::vector<Import> &imports) override;

			[[nodiscard]] std::optional<uint8_t> MapFileBlockToLibBlock(uint8_t block) const;
		};
	}
}
