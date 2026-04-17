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

			virtual bool Load(binaryio::BinaryReader &reader) override;
			virtual bool Save(binaryio::BinaryWriter &reader) override;

			[[nodiscard]] virtual constexpr Magic GetMagic() const override { return Magic::Bndl; }

			[[nodiscard]] virtual std::optional<Resource> GetResource(ResourceKey resourceKey) const override;

		private:
			std::map<ResourceID, std::vector<ImportEntry>> m_imports;

			virtual constexpr bool AppendsImportsToResource() const override { return false; }

			[[nodiscard]] std::optional<uint8_t> MapFileBlockToLibBlock(uint8_t block) const;
		};
	}
}
