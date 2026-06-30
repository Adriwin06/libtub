# libtub

`libtub` is the "ultimate bundle lib": a C++ library for Criterion bundle formats. It is built on top of `libbndl`'s 2025 codebase and extends it with YAP-style YAML project serialization plus higher-level resource and dependency metadata inspired by Bundle-Manager.

## What `libtub` adds on top

- BNDL plus BND2 v2, v3, and v5 support through the imported `libbndl` 2025 parser/writer model.
- Direct in-memory loading and saving, so callers do not need temp extraction just to inspect or transform a bundle.
- A lean `libtub_core` target for parser/writer and builder use without the YAML project layer.
- A `BundleBuilder` facade with common profiles and validation for tools that want to author resources without dealing with the lower-level `Resource` plumbing directly.
- YAML project export/import inspired by YAP, but generalized for multistream bundles and non-pointer import kinds.
- High-level `DescribeResources()` output that surfaces stream indices, debug data, imports, and per-memory binary metadata in one place.
- Last-error reporting for the C and C++ APIs, so callers do not have to treat every failed `bool` as a generic parse error.
- BND2 v5 setters for default-resource metadata and stream names so project round-trips can preserve more than raw payload bytes.
- A cleaned-up standalone `libtub` target/namespace/header layout instead of leaving the project as a renamed upstream drop.

## Build targets

- `libtub_core`: core bundle loading, saving, resources, imports, debug string table support, compression, and `BundleBuilder`.
- `libtub`: the full compatibility target. It includes `libtub_core` features plus YAML project import/export.

Project import/export is controlled by `LIBTUB_BUILD_PROJECT_SUPPORT`, which defaults to `ON`.

The build tree and installed package also expose namespaced aliases:

- `libtub::core`
- `libtub::libtub`

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

Installed consumers can use the generated package config:

```cmake
find_package(libtub CONFIG REQUIRED)
target_link_libraries(my_tool PRIVATE libtub::libtub)
```

The library uses CMake `FetchContent` for:

- `libbinaryio`
- `zlib`
- `pugixml`
- `yaml-cpp`

## Example

```cpp
#include <libtub/builder.hpp>
#include <filesystem>
#include <vector>

int main()
{
	libtub::BundleBuilder builder(libtub::BundleProfiles::NeedForSpeedHotPursuitPC());
	builder.SetStreamName(0, "base");

	const std::vector<uint8_t> data{ 0x01, 0x02, 0x03, 0x04 };
	const auto resourceID = libtub::ResourceID("example.bin");
	if (!builder.AddResource(resourceID, libtub::ResourceType::NeedForSpeed::BinaryFile)
		.MainMemory(data, 4)
		.DebugData("example.bin", "BinaryFile")
		.Commit())
		return 1;

	if (!builder.SetDefaultResource(resourceID))
		return 2;

	const auto bytes = builder.SaveToMemory();
	return bytes.empty();
}
```

Available builder profiles:

- `BundleProfiles::BurnoutParadisePC()`
- `BundleProfiles::BurnoutParadiseXbox360()`
- `BundleProfiles::BurnoutParadisePS3()`
- `BundleProfiles::NeedForSpeedHotPursuitPC()`
- `BundleProfiles::BndlPC(version, flags)`

Project export/import remains available through the full `libtub` target:

```cpp
#include <libtub/bundle.hpp>
#include <iostream>

int main()
{
	libtub::Bundle bundle;
	if (!bundle.Load("GLOBALB.LZC"))
	{
		std::cerr << bundle.GetLastErrorMessage() << '\n';
		return 1;
	}

	return bundle.ExportProject("out/project") ? 0 : 2;
}
```

`bndl_util --pack <project-dir> --file <output>` can pack directories exported through `ExportProject()`. It refuses to overwrite existing output files. The older `_config.xml` extraction layout is still extract-only.

## Public API highlights

- `bool Load(const std::string &path)`
- `bool Load(std::span<const uint8_t> data)`
- `bool Save(const std::string &path)`
- `std::vector<uint8_t> SaveToMemory()`
- `ErrorCode GetLastErrorCode() const`
- `const std::string &GetLastErrorMessage() const`
- `std::vector<libtub::ResourceDescriptor> DescribeResources() const`
- `bool ExportProject(const std::filesystem::path &, const ProjectExportOptions &) const`
- `bool ImportProject(const std::filesystem::path &)`
- `bool SetDefaultResource(ResourceID, int32_t streamIndex = 0)`
- `bool SetStreamName(uint8_t index, std::string_view name)`

## Notes

- `ref/` is kept as reference material only. `libtub`'s actual source lives in the repo root `include/`, `src/`, and `tools/`.
- The bundle editor/utility tool sources are still present behind CMake options, but the core deliverable here is the reusable C++ library.
