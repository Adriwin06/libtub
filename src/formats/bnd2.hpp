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

			[[nodiscard]] virtual std::optional<Resource> GetResource(ResourceID resourceID) const override;

			[[nodiscard]] virtual std::vector<MemoryType> GetMemoryTypes() const override;

		protected:
			virtual constexpr bool AppendsImportsToResource() const override { return false; }

		private:
			std::optional<uint8_t> MapFileBlockToLibBlock(uint8_t block) const;
		};
	}
}
