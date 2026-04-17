# libtub

`libtub` is the "ultimate bundle lib": a C++ library for Criterion bundle formats. It is built on top of `libbndl`'s 2025 codebase and extends it with YAP-style YAML project serialization plus higher-level resource and dependency metadata inspired by Bundle-Manager.

## What `libtub` adds on top

- BNDL plus BND2 v2, v3, and v5 support through the imported `libbndl` 2025 parser/writer model.
- Direct in-memory loading and saving, so callers do not need temp extraction just to inspect or transform a bundle.
- YAML project export/import inspired by YAP, but generalized for multistream bundles and non-pointer import kinds.
- High-level `DescribeResources()` output that surfaces stream indices, debug data, imports, and per-memory binary metadata in one place.
- BND2 v5 setters for default-resource metadata and stream names so project round-trips can preserve more than raw payload bytes.
- A cleaned-up standalone `libtub` target/namespace/header layout instead of leaving the project as a renamed upstream drop.

## Project layout

`ExportProject()` writes a directory that looks like this:

```text
bundle-project/
  .meta.yaml
  .imports.yaml                # optional when combined import export is enabled
  type_0000000c_Renderable/
	0x00b4802c.s0.mainMemory.bin
	0x00b4802c.s0.disposable.bin
	0x00b4802c.s0.imports.yaml # optional when imports are split per resource
```

The metadata file contains:

- Bundle identity: magic, version, platform, flags.
- Optional default-resource information and stream names.
- A resource list with stream indices, types, debug names, binary paths, alignments, and import references.

## Building

```powershell
cmake -S . -B build
cmake --build build --config Debug
```

The library uses CMake `FetchContent` for:

- `libbinaryio`
- `zlib`
- `pugixml`
- `yaml-cpp`

## Example

```cpp
#include <libtub/bundle.hpp>
#include <filesystem>

int main()
{
	libtub::Bundle bundle;
	if (!bundle.Load("GLOBALB.LZC"))
		return 1;

	const auto resources = bundle.DescribeResources();
	const auto bytes = bundle.SaveToMemory();

	libtub::ProjectExportOptions options;
	options.sortByType = true;
	options.combineImports = false;

	if (!bundle.ExportProject("out/project", options))
		return 2;

	libtub::Bundle roundTrip;
	if (!roundTrip.ImportProject("out/project"))
		return 3;

	return resources.empty() || bytes.empty();
}
```

## Public API highlights

- `bool Load(const std::string &path)`
- `bool Load(std::span<const uint8_t> data)`
- `bool Save(const std::string &path)`
- `std::vector<uint8_t> SaveToMemory()`
- `std::vector<libtub::ResourceDescriptor> DescribeResources() const`
- `bool ExportProject(const std::filesystem::path &, const ProjectExportOptions &) const`
- `bool ImportProject(const std::filesystem::path &)`
- `bool SetDefaultResource(ResourceID, int32_t streamIndex = 0)`
- `bool SetStreamName(uint8_t index, std::string_view name)`

## Notes

- `ref/` is kept as reference material only. `libtub`'s actual source lives in the repo root `include/`, `src/`, and `tools/`.
- The upstream `libbndl` MIT license is preserved in [NOTICE.libbndl.md](NOTICE.libbndl.md).
- The bundle editor/utility tool sources are still present behind CMake options, but the core deliverable here is the reusable C++ library.
